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
#include "ht_page_store.h"
#include "ht_tag_manager.h"
#include "ht_optimizer.h"
#include "pl_net_configuration.h"
#include "pl_socket_pipelines.h"

namespace CyberCache {

bool PageObjectStore::remove_fpc_record(const StringChunk& id, CommandReader& cr) {
  c3_hash_t hash = table_hasher.hash(id.get_chars(), id.get_length());
  TableLock lock(*this, hash);
  HashTable& table = lock.get_table();
  auto po = (PageObject*) table.find(hash, id.get_chars(), id.get_short_length());
  if (po != nullptr && po->flags_are_clear(HOF_BEING_DELETED)) {
    LockableObjectGuard guard(po);
    if (guard.is_locked() && po->flags_are_clear(HOF_BEING_DELETED)) {
      po->set_flags(HOF_BEING_DELETED);
      /*
       * Make first attempt to dispose FPC object buffer.
       *
       * If it fails (because there are still some readers transferring buffer contents over socket
       * pipeline, of dumping it to binlog), then second attempt will be done by tag manager.
       *
       * If that fails as well, third attempt will be done by FPC optimizer.
       *
       * If that one also fails, fourth attempt will be done by table lock cleanup code, which processes
       * queue of deleted objects immediately before releasing an exclusive lock. That code would try
       * to release the buffer and, if there are still some readers, will put the object back to the
       * queue for later continued attempts, until the buffer can be finally released.
       *
       * Since the object is already marked as "deleted", new readers cannot be attached to it, so
       * release process is guaranteed to be completed at some point.
       */
      po->try_dispose_buffer(fpc_memory);
      // we cannot defer posting response as `CommandReader` may be disposed by tag manager
      get_consumer().post_ok_response(cr);
      guard.unlock();
      // it is tag manager that will send "delete" message to FPC optimizer
      get_tag_manager().post_command_message(&cr, po);
      return true;
    }
  }
  return false;
}

bool PageObjectStore::process_load_command(CommandReader& cr) {
  command_status_t status = CS_FORMAT_ERROR;
  CommandHeaderIterator iterator(cr);
  StringChunk id = iterator.get_string();
  if (id.is_valid_name()) {
    NumberChunk agent = iterator.get_number();
    if (agent.is_valid()) {
      auto ua = (user_agent_t) agent.get_uint();
      if (ua < UA_NUMBER_OF_ELEMENTS && !iterator.has_more_chunks()) {
        status = CS_FAILURE;
        c3_hash_t hash = table_hasher.hash(id.get_chars(), id.get_length());
        TableLock lock(*this, hash);
        HashTable& table = lock.get_table();
        auto po = (PageObject*) table.find(hash, id.get_chars(), id.get_short_length());
        if (po != nullptr && po->flags_are_clear(HOF_BEING_DELETED)) {
          c3_assert(po->get_type() == HOT_PAGE_OBJECT);
          LockableObjectGuard guard(po);
          if (guard.is_locked()) {
            // the object could have been deleted while we were trying to lock it
            if (po->flags_are_clear(HOF_BEING_DELETED)) {
              get_consumer().post_data_response(cr, (PayloadHashObject*) po, "");
              guard.unlock();
              // notify optimizer
              get_optimizer().post_read_message(po, ua);
              status = CS_SUCCESS;
            }
          }
        }
      }
    }
  }
  switch (status) {
    case CS_SUCCESS:
      PERF_INCREMENT_DOMAIN_COUNTER(FPC, Cache_Hits)
      return true;
    case CS_FORMAT_ERROR:
      return get_consumer().post_format_error_response(cr);
    case CS_FAILURE:
      PERF_INCREMENT_DOMAIN_COUNTER(FPC, Cache_Misses)
      return get_consumer().post_ok_response(cr);
    default: // just to satisfy the compiler
      return false;
  }
}

bool PageObjectStore::process_test_command(CommandReader& cr) {
  command_status_t status = CS_FORMAT_ERROR;
  CommandHeaderIterator iterator(cr);
  StringChunk id = iterator.get_string();
  if (id.is_valid_name()) {
    NumberChunk agent = iterator.get_number();
    if (agent.is_valid()) {
      auto ua = (user_agent_t) agent.get_uint();
      if (ua < UA_NUMBER_OF_ELEMENTS && !iterator.has_more_chunks()) {
        status = CS_FAILURE;
        c3_hash_t hash = table_hasher.hash(id.get_chars(), id.get_length());
        TableLock lock(*this, hash);
        HashTable& table = lock.get_table();
        auto po = (PageObject*) table.find(hash, id.get_chars(), id.get_short_length());
        if (po != nullptr && po->flags_are_clear(HOF_BEING_DELETED)) {
          c3_assert(po->get_type() == HOT_PAGE_OBJECT);
          LockableObjectGuard guard(po);
          if (guard.is_locked()) {
            // the object could have been deleted while we were trying to lock it
            if (po->flags_are_clear(HOF_BEING_DELETED)) {
              get_consumer().post_data_response(cr, "U", po->get_last_modification_time());
              guard.unlock();
              // notify optimizer
              get_optimizer().post_read_message(po, ua);
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
    case CS_FAILURE:
      return get_consumer().post_ok_response(cr);
    default: // just to satisfy the compiler
      return false;
  }
}

bool PageObjectStore::process_save_command(CommandReader& cr) {
  command_status_t status = CS_FORMAT_ERROR;
  CommandHeaderIterator iterator(cr);
  StringChunk id = iterator.get_string();
  if (id.is_valid_name()) {
    NumberChunk agent = iterator.get_number();
    if (agent.is_valid_uint()) {
      auto ua = (user_agent_t) agent.get_uint();
      if (ua < UA_NUMBER_OF_ELEMENTS) {
        NumberChunk lifetime = iterator.get_number();
        if (lifetime.is_in_range(-1, UINT_MAX_VAL)) {
          ListChunk tags = iterator.get_list();
          if (tags.is_valid()) {
            c3_uint_t ntags = tags.get_count();
            bool tags_ok = true;
            for (c3_uint_t i = 0; i < ntags; i++) {
              StringChunk tag = tags.get_string();
              if (!tag.is_valid_name()) {
                tags_ok = false;
                break;
              }
            }
            if (tags_ok && !iterator.has_more_chunks()) {
              payload_info_t pi;
              if (cr.get_payload_info(pi)) {
                c3_assert(!pi.pi_has_errors);
                status = CS_INTERNAL_ERROR;
                c3_hash_t hash = table_hasher.hash(id.get_chars(), id.get_length());
                TableLock lock(*this, hash);
                HashTable& table = lock.get_table();
                auto po = (PageObject*) table.find(hash,
                  id.get_chars(), id.get_short_length());
                bool locked = false;
                if (po != nullptr && po->flags_are_clear(HOF_BEING_DELETED)) {
                  locked = po->lock();
                }
                if (po == nullptr || po->flags_are_set(HOF_BEING_DELETED)) {
                  if (locked) {
                    // the object could have been marked as "deleted" while we were locking it
                    po->unlock();
                  }
                  po = (PageObject*) fpc_memory.alloc(PageObject::calculate_size(id.get_length()));
                  new (po) PageObject(hash, id.get_chars(), id.get_short_length());
                  locked = po->lock();
                  lock.upgrade_lock();
                  bool resized = table.add(po);
                  /*
                   * See comments in the `WRITE` command implementation for reasons for downgrading
                   * the lock at this point.
                   */
                  lock.downgrade_lock(resized);
                } else {
                  /*
                   * See comments in the `WRITE` command implementation on why it is safe to assume that
                   * previous branch does not have to wait until there are no readers.
                   */
                  po->wait_until_no_readers();
                }
                c3_assert(po && po->get_type() == HOT_PAGE_OBJECT && locked);
                cr.command_reader_transfer_payload(po, DOMAIN_FPC, pi.pi_usize, pi.pi_compressor);
                // we cannot defer posting response as `CommandReader` may be disposed by tag manager
                get_consumer().post_ok_response(cr);
                po->unlock();
                /*
                 * Here, we create clone of the `CommandReader` that, in turn, will contain clone of the
                 * `SharedBuffers` object that contains *empty* payload buffer. It is this cloned object that will
                 * be sent to the tag manager for further processing.
                 *
                 * Previously, we would send original `CommandReader` object to the tag manager, which on some
                 * rare occasions would cause deadlocks.
                 *
                 * The thing is that the above `command_reader_transfer_payload()` call registers `SharedBuffers`
                 * field of the `CommandReader` object as hash object reader. This is necessary, because the same
                 * instance of the `SharedBuffers` could have been sent to replicator (as part of
                 * `SocketCommandWriter`) or to the binlog writer (as part of `FileCommandWriter`), which should
                 * then have full access to the payload data until they finish their respective jobs. Which they
                 * will, sooner or later, do, and unregister themselves as object readers -- they do not have to
                 * acquire hash object locks for that.
                 *
                 * Situation with `CommandReader` sent to the tag manager is, however, different in that it *will*
                 * have to acquire hash object lock to do its job (in tag manager) before it can be disposed and
                 * unregister itself as hash object reader. So, if several `SAVE` requests with same ID (i.e.
                 * targeting the same hash object) arrive in quick succession, we could end up with the following
                 * situation: a) request 1 arrives, updates buffer in `process_save_command()`, registers
                 * `CommandReader` as hash object reader, and forwards it to tag manager, b) request 2 arrives,
                 * locks hash object, and starts waiting in `wait_until_no_readers()` (because there *is* at least
                 * one registered reader: `CommandReader` "traveling" from FPC store to tag manager, c) the
                 * `CommandReader` object forwarded by request 1 arrives to tag manager and tries to lock hash
                 * object... which it cannot do, because it's locked by request 2; consequently, it wouldn't
                 * unregister itself as a reader, which request 2 would need to release the lock; so we have a
                 * classic deadlock.
                 *
                 * The solution is to create *deep* copy of `CommandReader` (i.e. with own copy of `SharedBuffers`
                 * that would only contain header data, and thus not attached to the hash object) and send *that*
                 * "abridged" copy to the tag manager.
                 */
                CommandReader* header_cr = cr.clone(false);
                // it is tag manager that will send "update" message to FPC optimizer
                get_tag_manager().post_command_message(header_cr, po);
                status = CS_SUCCESS;
              }
            }
          }
        }
      }
    }
  }
  switch (status) {
    case CS_FORMAT_ERROR:
      get_consumer().post_format_error_response(cr);
      break;
    case CS_INTERNAL_ERROR:
      get_consumer().post_internal_error_response(cr);
      break;
    case CS_SUCCESS:
      // return `false` to indicate that we do want to dispose `CommandReader`
      break;
    case CS_FAILURE:
      get_consumer().post_ok_response(cr);
      // return `true`
  }
  return true;
}

bool PageObjectStore::process_remove_command(CommandReader& cr) {
  command_status_t status = CS_FORMAT_ERROR;
  CommandHeaderIterator iterator(cr);
  StringChunk id = iterator.get_string();
  if (id.is_valid_name() && !iterator.has_more_chunks() && !PayloadChunkIterator::has_payload_data(cr)) {
    status = remove_fpc_record(id, cr)? CS_SUCCESS: CS_FAILURE;
  }
  switch (status) {
    case CS_FORMAT_ERROR:
      get_consumer().post_format_error_response(cr);
      break;
    case CS_FAILURE:
      get_consumer().post_ok_response(cr);
      break;
    case CS_SUCCESS:
      // tell caller we must not dispose `CommandReader` object (being used by tag manager)
      return false;
    default: // just to satisfy the compiler
      ;
  }
  return true;
}

bool PageObjectStore::process_getfillingpercentage_command(CommandReader& cr) {
  if (!ChunkIterator::has_any_data(cr)) {
    c3_uint_t percentage = 0;
    if (fpc_memory.is_quota_set()) {
      percentage = (c3_uint_t)((fpc_memory.get_used_size() * 100) / fpc_memory.get_quota());
      if (percentage > 100) {
        /*
         * peak memory usage may exceed quota for a brief moment: until optimizer kicks in and frees some
         * memory; but the protocol requires that reported value is in 0..100 range
         */
        percentage = 100;
      }
    }
    return get_consumer().post_data_response(cr, "U", percentage);
  } else {
    return get_consumer().post_format_error_response(cr);
  }
}

bool PageObjectStore::process_getmetadatas_command(CommandReader& cr) {
  command_status_t status = CS_FORMAT_ERROR;
  CommandHeaderIterator iterator(cr);
  StringChunk id = iterator.get_string();
  if (id.is_valid_name() && !iterator.has_more_chunks() && !PayloadChunkIterator::has_payload_data(cr)) {
    status = CS_FAILURE;
    c3_hash_t hash = table_hasher.hash(id.get_chars(), id.get_length());
    TableLock lock(*this, hash);
    HashTable& table = lock.get_table();
    auto po = (PageObject*) table.find(hash, id.get_chars(), id.get_short_length());
    if (po != nullptr && po->flags_are_clear(HOF_BEING_DELETED)) {
      LockableObjectGuard guard(po);
      if (guard.is_locked()) {
        if (po->flags_are_clear(HOF_BEING_DELETED)) {
          guard.unlock();
          // it is tag manager that will send "update" message to FPC optimizer
          get_tag_manager().post_command_message(&cr, po);
          status = CS_SUCCESS;
        }
      }
    }
  }
  switch (status) {
    case CS_FORMAT_ERROR:
      get_consumer().post_format_error_response(cr);
      break;
    case CS_FAILURE:
      get_consumer().post_ok_response(cr);
      break;
    case CS_SUCCESS:
      // tell caller we must not dispose `CommandReader` object
      return false;
      break;
    default: // just to satisfy the compiler
      ;
  }
  return true;
}

bool PageObjectStore::process_touch_command(CommandReader& cr) {
  command_status_t status = CS_FORMAT_ERROR;
  CommandHeaderIterator iterator(cr);
  StringChunk id = iterator.get_string();
  if (id.is_valid_name()) {
    NumberChunk lifetime = iterator.get_number();
    if (lifetime.is_valid_uint() && !iterator.has_more_chunks()) {
      status = CS_FAILURE;
      c3_hash_t hash = table_hasher.hash(id.get_chars(), id.get_length());
      TableLock lock(*this, hash);
      HashTable& table = lock.get_table();
      auto po = (PageObject*) table.find(hash, id.get_chars(), id.get_short_length());
      if (po != nullptr && po->flags_are_clear(HOF_BEING_DELETED)) {
        c3_assert(po->get_type() == HOT_PAGE_OBJECT);
        LockableObjectGuard guard(po);
        if (guard.is_locked()) {
          // the object could have been deleted while we were trying to lock it
          if (po->flags_are_clear(HOF_BEING_DELETED)) {
            get_consumer().post_ok_response(cr);
            guard.unlock();
            // notify optimizer
            get_page_optimizer().post_fpc_touch_message(po, lifetime.get_uint());
            status = CS_SUCCESS;
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
    case CS_FAILURE:
      return get_consumer().post_error_response(cr, "Entry '%.*s' did not exist of had been deleted",
        id.get_length(), id.get_chars());
    default: // just to satisfy the compiler
      return false;
  }
}

FileCommandWriter* PageObjectStore::create_file_command_writer(PayloadHashObject* pho, c3_timestamp_t time) {
  c3_assert(pho && pho->flags_are_clear(HOF_BEING_DELETED) && pho->get_type() == HOT_PAGE_OBJECT && pho->is_locked());
  Memory& memory = get_memory_object();
  SharedObjectBuffers* sob = SharedObjectBuffers::create_object(memory);
  sob->attach_payload(pho);
  auto fcw = alloc<FileCommandWriter>(memory);
  new (fcw) FileCommandWriter(memory, 0, sob);
  CommandHeaderChunkBuilder header(*fcw, server_net_config, CMD_SAVE, false);
  auto po = (PageObject*) pho;
  const char* id_buff = po->get_name();
  c3_uint_t id_len = po->get_name_length();
  c3_uint_t ua = po->get_user_agent();
  c3_timestamp_t expiration_time = po->get_expiration_time();
  c3_uint_t lifetime = expiration_time > time? expiration_time - time: 1;
  HeaderListChunkBuilder list(*fcw, server_net_config);
  bool ok = true;
  c3_uint_t num_tags = po->get_num_tag_refs();
  for (c3_uint_t i = 0; i < num_tags; i++) {
    TagRef& tref = po->get_tag_ref(i);
    TagObject* tag = tref.get_tag_object();
    if (list.estimate(tag->get_name_length()) == 0) {
      ok = false;
      break;
    }
  }
  if (ok) {
    list.configure();
    for (c3_uint_t j = 0; j < num_tags; j++) {
      TagRef& tref = po->get_tag_ref(j);
      TagObject* tag = tref.get_tag_object();
      list.add(tag->get_name_length(), tag->get_name());
    }
    list.check();
    if (header.estimate_string(id_len) != 0 &&
      header.estimate_number(ua) != 0 &&
      header.estimate_number(lifetime) != 0 &&
      header.estimate_list(list) != 0) {
      PayloadChunkBuilder payload(*fcw, server_net_config);
      payload.add();
      header.configure(&payload);
      header.add_string(id_buff, id_len);
      header.add_number(ua);
      header.add_number(lifetime);
      header.add_list(list);
      header.check();
      return fcw;
    }
  }
  ReaderWriter::dispose(fcw);
  log(LL_ERROR, "Could not create SAVE command for '%.*s'", id_len, id_buff);
  return nullptr;
}

bool PageObjectStore::process_command(CommandReader* cr) {
  assert(cr != nullptr && cr->is_active());
  bool do_dispose = true;
  bool result = true;
  switch (cr->get_command_id()) {
    case CMD_LOAD:
      result = process_load_command(*cr);
      break;
    case CMD_TEST:
      result = process_test_command(*cr);
      break;
    case CMD_SAVE:
      do_dispose = process_save_command(*cr);
      break;
    case CMD_REMOVE:
      do_dispose = process_remove_command(*cr);
      break;
    case CMD_GETFILLINGPERCENTAGE:
      result = process_getfillingpercentage_command(*cr);
      break;
    case CMD_GETMETADATAS:
      do_dispose = process_getmetadatas_command(*cr);
      break;
    case CMD_TOUCH:
      result = process_touch_command(*cr);
      break;
    default:
      // unknown commands are handled by connection threads
      c3_assert_failure();
      result = false;
  }
  if (result && do_dispose) {
    // otherwise, caller may have to do its own reporting
    ReaderWriter::dispose(cr);
  }
  return result;
}

}
