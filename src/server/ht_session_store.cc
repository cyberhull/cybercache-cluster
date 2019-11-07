/**
 * This file is a part of the implementation of the CyberCache Cluster.
 * Written by Vadim Sytnikov.
 * Copyright (C) 2016-2019 CyberHULL. All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#include "ht_shared_buffers.h"
#include "ht_session_store.h"
#include "ht_optimizer.h"
#include "pl_net_configuration.h"
#include "pl_socket_pipelines.h"

namespace CyberCache {

void SessionObjectStore::destroy_session_record(StringChunk& id) {
  c3_hash_t hash = table_hasher.hash(id.get_chars(), id.get_length());
  TableLock lock(*this, hash);
  HashTable& table = lock.get_table();
  auto so = (SessionObject*) table.find(hash, id.get_chars(), id.get_short_length());
  if (so != nullptr && so->flags_are_clear(HOF_BEING_DELETED)) {
    LockableObjectGuard guard(so);
    if (guard.is_locked() && so->flags_are_clear(HOF_BEING_DELETED)) {
      so->set_flags(HOF_BEING_DELETED);
      /*
       * Make first attempt to dispose session object buffer.
       *
       * If it fails (because there are still some readers transferring buffer contents over socket
       * pipeline, of dumping it to binlog), then second attempt will be done by session optimizer.
       *
       * If that one also fails, third attempt will be done by table lock cleanup code, which processes
       * queue of deleted objects immediately before releasing an exclusive lock. That code would try
       * to release the buffer and, if there are still some readers, will put the object back to the
       * queue for later continued attempts, until the buffer can be finally released.
       *
       * Since the object is already marked as "deleted", new readers cannot be attached to it, so
       * release process is guaranteed to be completed at some point.
       */
      so->try_dispose_buffer(session_memory);
      guard.unlock();

      // notify optimizer
      get_optimizer().post_delete_message(so);
    }
  }
}

bool SessionObjectStore::process_read_command(CommandReader& cr) {
  command_status_t status = CS_FORMAT_ERROR;
  CommandHeaderIterator iterator(cr);
  StringChunk id = iterator.get_string();
  if (id.is_valid_name()) {
    NumberChunk agent = iterator.get_number();
    if (agent.is_valid()) {
      auto ua = (user_agent_t) agent.get_uint();
      if (ua < UA_NUMBER_OF_ELEMENTS) {
        c3_uint_t request_id = 0;
        bool format_ok = true;
        if (iterator.get_next_chunk_type() == CHUNK_NUMBER) {
          NumberChunk request_id_chunk = iterator.get_number();
          if (request_id_chunk.is_valid_uint()) {
            request_id = request_id_chunk.get_uint();
          } else {
            format_ok = false;
          }
        }
        if (format_ok && !iterator.has_more_chunks()) {
          status = CS_FAILURE;
          c3_hash_t hash = table_hasher.hash(id.get_chars(), id.get_length());
          TableLock lock(*this, hash);
          HashTable& table = lock.get_table();
          auto so = (SessionObject*) table.find(hash, id.get_chars(), id.get_short_length());
          if (so != nullptr && so->flags_are_clear(HOF_BEING_DELETED)) {
            c3_assert(so->get_type() == HOT_SESSION_OBJECT);
            LockableObjectGuard guard(so);
            if (guard.is_locked()) {
              // the object could have been deleted while we were trying to lock it
              if (so->flags_are_clear(HOF_BEING_DELETED)) {
                if (so->get_expiration_time() >= Timer::current_timestamp()) {
                  // lock the session (to prevent reads with different request IDs)
                  switch (so->lock_session(request_id)) {
                    case SLR_BROKE_LOCK:
                      log(LL_WARNING, "Broke lock on session record '%.*s'",
                        (int) so->get_name_length(), so->get_name());
                      // fall through
                    case SLR_SUCCESS:
                      get_consumer().post_data_response(cr, (PayloadHashObject*) so, "");
                      guard.unlock();
                      // notify optimizer
                      get_optimizer().post_read_message(so, ua);
                      status = CS_SUCCESS;
                      break;
                    default: // SLR_DELETED
                      // the hash object will be unlocked by lock guard
                      // "failure" status will cause sending "OK" response
                      c3_assert(status == CS_FAILURE);
                  }
                } else {
                  C3_DEBUG(log(LL_DEBUG, "Deleting expired session record '%.*s' (%u : %s)",
                    (int) so->get_name_length(), so->get_name(),
                    so->get_expiration_time(), Timer::to_ascii(so->get_expiration_time())));
                  so->set_flags(HOF_BEING_DELETED);
                  so->try_dispose_buffer(session_memory);
                  guard.unlock();
                  // notify optimizer
                  get_optimizer().post_delete_message(so);
                  c3_assert(status == CS_FAILURE);
                }
              }
            }
          }
        }
      }
    }
  }
  switch (status) {
    case CS_SUCCESS:
      PERF_INCREMENT_DOMAIN_COUNTER(SESSION, Cache_Hits)
      return true;
    case CS_FORMAT_ERROR:
      return get_consumer().post_format_error_response(cr);
    case CS_FAILURE:
      PERF_INCREMENT_DOMAIN_COUNTER(SESSION, Cache_Misses)
      return get_consumer().post_ok_response(cr);
    default: // just to satisfy the compiler
      return false;
  }
}

bool SessionObjectStore::process_write_command(CommandReader& cr) {
  command_status_t status = CS_FORMAT_ERROR;
  CommandHeaderIterator iterator(cr);
  StringChunk id = iterator.get_string();
  if (id.is_valid_name()) {
    NumberChunk agent = iterator.get_number();
    if (agent.is_valid_uint()) {
      auto ua = (user_agent_t) agent.get_uint();
      if (ua < UA_NUMBER_OF_ELEMENTS) {
        NumberChunk lifetime_chunk = iterator.get_number();
        if (lifetime_chunk.is_valid()) {
          c3_uint_t request_id = 0;
          bool format_ok = true;
          if (iterator.get_next_chunk_type() == CHUNK_NUMBER) {
            NumberChunk request_id_chunk = iterator.get_number();
            if (request_id_chunk.is_valid_uint()) {
              request_id = request_id_chunk.get_uint();
            } else {
              format_ok = false;
            }
          }
          if (format_ok && !iterator.has_more_chunks()) {
            c3_timestamp_t lifetime = lifetime_chunk.is_negative()? Timer::MAX_TIMESTAMP: lifetime_chunk.get_uint();
            payload_info_t pi;
            if (cr.get_payload_info(pi)) {
              c3_assert(!pi.pi_has_errors);
              status = CS_INTERNAL_ERROR;
              c3_hash_t hash = table_hasher.hash(id.get_chars(), id.get_length());
              TableLock lock(*this, hash);
              HashTable& table = lock.get_table();
              auto so = (SessionObject*) table.find(hash, id.get_chars(), id.get_short_length());
              bool locked = false;
              if (so != nullptr && so->flags_are_clear(HOF_BEING_DELETED)) {
                locked = so->lock();
              }
              if (so == nullptr || so->flags_are_set(HOF_BEING_DELETED)) {
                if (locked) {
                  // the object could have been marked as "deleted" while we were locking it
                  so->unlock();
                }
                so = (SessionObject*) session_memory.alloc(SessionObject::calculate_size(id.get_length()));
                new (so) SessionObject(hash, id.get_chars(), id.get_short_length());
                locked = so->lock();
                lock.upgrade_lock();
                bool resized = table.add(so);
                /*
                 * Before transferring the payload, we may have to wait until all readers release the object,
                 * so the sooner we unlock the table (making it available at least for reading), the better.
                 * On the other hand, downgrading the lock may trigger (inside table lock) a sequence of
                 * object removals from the table, but this extra wait will *overlap* with any potential wait
                 * for other readers rather than be added to it.
                 */
                lock.downgrade_lock(resized);
              } else {
                /*
                 * Even though we downgraded table lock in the above branch, so the object could have been
                 * "found" by another thread, that other thread could not have added any readers, since it
                 * would need object lock for that, and we lock the object *before* adding it to the table. So
                 * it is safe to assume that previous branch did not have to wait for readers to complete.
                 */
                so->wait_until_no_readers();
              }
              c3_assert(so && so->get_type() == HOT_SESSION_OBJECT && locked);
              cr.command_reader_transfer_payload(so, DOMAIN_SESSION, pi.pi_usize, pi.pi_compressor);
              get_consumer().post_ok_response(cr);
              // unlocks both session and hash object
              so->unlock_session(request_id);

              // notify optimizer
              get_optimizer().post_write_message(so, ua, lifetime);
              status = CS_SUCCESS;
            }
          }
        }
      }
    }
  }
  switch (status) {
    case CS_SUCCESS:
      return true;
    case CS_FORMAT_ERROR:
      return get_consumer().post_format_error_response(cr);
    case CS_INTERNAL_ERROR:
      return get_consumer().post_internal_error_response(cr);
    default: // just to satisfy the compiler
      return false;
  }
}

bool SessionObjectStore::process_destroy_command(CommandReader& cr) {
  CommandHeaderIterator iterator(cr);
  StringChunk id = iterator.get_string();
  if (id.is_valid_name() && !iterator.has_more_chunks() && !PayloadChunkIterator::has_payload_data(cr)) {
    destroy_session_record(id);
    return get_consumer().post_ok_response(cr);
  } else {
    return get_consumer().post_format_error_response(cr);
  }
}

bool SessionObjectStore::process_gc_command(CommandReader& cr) {
  CommandHeaderIterator iterator(cr);
  NumberChunk seconds = iterator.get_number();
  if (seconds.is_valid_uint() && !iterator.has_more_chunks() && !PayloadChunkIterator::has_payload_data(cr)) {
    // notify optimizer
    get_optimizer().post_gc_message(seconds.get_uint());
    return get_consumer().post_ok_response(cr);
  } else {
    return get_consumer().post_format_error_response(cr);
  }
}

FileCommandWriter* SessionObjectStore::create_file_command_writer(PayloadHashObject* pho, c3_timestamp_t time) {
  c3_assert(pho && pho->flags_are_clear(HOF_BEING_DELETED) && pho->get_type() == HOT_SESSION_OBJECT && pho->is_locked());
  c3_timestamp_t expiration_time = pho->get_expiration_time();
  if (expiration_time > time) {
    Memory& memory = get_memory_object();
    SharedObjectBuffers* sob = SharedObjectBuffers::create_object(memory);
    sob->attach_payload(pho);
    auto fcw = alloc<FileCommandWriter>(memory);
    new (fcw) FileCommandWriter(memory, 0, sob);
    CommandHeaderChunkBuilder header(*fcw, server_net_config, CMD_WRITE, false);
    const char* id_buff = pho->get_name();
    c3_uint_t id_len = pho->get_name_length();
    c3_uint_t ua = pho->get_user_agent();
    c3_uint_t lifetime = expiration_time - time;
    if (header.estimate_string(id_len) != 0 &&
      header.estimate_number(ua) != 0 &&
      header.estimate_number(lifetime) != 0) {
      PayloadChunkBuilder payload(*fcw, server_net_config);
      payload.add();
      header.configure(&payload);
      header.add_string(id_buff, id_len);
      header.add_number(ua);
      header.add_number(lifetime);
      header.check();
      return fcw;
    }
    ReaderWriter::dispose(fcw);
    log(LL_ERROR, "Could not create WRITE command for '%.*s'", id_len, id_buff);
  }
  return nullptr;
}

bool SessionObjectStore::process_command(CommandReader* cr) {
  assert(cr != nullptr && cr->is_active());
  bool result = false;
  switch (cr->get_command_id()) {
    case CMD_READ:
      result = process_read_command(*cr);
      break;
    case CMD_WRITE:
      result = process_write_command(*cr);
      break;
    case CMD_DESTROY:
      result = process_destroy_command(*cr);
      break;
    case CMD_GC:
      result = process_gc_command(*cr);
      break;
    default:
      // unknown commands are handled by connection threads
      c3_assert_failure();
  }
  if (result) {
    // otherwise, caller will need command reader to do its own reporting
    ReaderWriter::dispose(cr);
  }
  return result;
}

}
