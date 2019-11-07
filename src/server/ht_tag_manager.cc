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
#include "ht_tag_manager.h"
#include "ht_optimizer.h"
#include "pl_socket_pipelines.h"
#include "pl_net_configuration.h"

namespace CyberCache {

TagStore::TagStore() noexcept:
  ObjectStore("Tag manager", DOMAIN_FPC, DEFAULT_NUM_TABLES, DEFAULT_TABLE_CAPACITY),
  ts_queue(DOMAIN_FPC, HO_TAG_MANAGER, DEFAULT_QUEUE_CAPACITY, DEFAULT_MAX_QUEUE_CAPACITY, 255) {
  ts_page_store = nullptr;
  ts_untagged = nullptr;
  ts_quitting = false;
}

void TagStore::enter_quit_state() {
  Thread::set_state(TS_QUITTING);
  ts_quitting = true;
}

void TagStore::allocate_tag_store() {
  init_object_store();
  c3_ushort_t nlen = sizeof(NAME_OF_TAG_FOR_UNTAGGED_PAGES) - 1;
  c3_hash_t hash = table_hasher.hash(NAME_OF_TAG_FOR_UNTAGGED_PAGES, nlen);
  ts_untagged = create_tag(hash, NAME_OF_TAG_FOR_UNTAGGED_PAGES, nlen, true);
  log(LL_VERBOSE, "%s: initialized tag store", get_name());
}

void TagStore::dispose_tag_store() {
  // if FPC store had already been initialized...
  if (ts_page_store != nullptr) {
    // ... unlink all tags from all objects
    ts_page_store->enumerate_all(this, object_cleanup_unlink_callback);
  }
  // both dispose() calls, below, have internal guards allowing calling them multiple times
  dispose_object_store();
  ts_queue.dispose();
}

TagObject* TagStore::find_tag(const char* name, c3_ushort_t nlen) const {
  c3_hash_t hash = table_hasher.hash(name, nlen);
  HashTable& ht = table(get_table_index(hash));
  return (TagObject*) ht.find(hash, name, nlen);
}

TagObject* TagStore::create_tag(c3_hash_t hash, const char* name, c3_ushort_t nlen, bool untagged) const {
  auto to = (TagObject*) fpc_memory.alloc(TagObject::calculate_size(nlen));
  new (to) TagObject(hash, name, nlen, untagged);
  HashTable& ht = table(get_table_index(hash));
  ht.add(to);
  return to;
}

TagObject* TagStore::find_create_tag(const char* name, c3_ushort_t nlen) const {
  c3_hash_t hash = table_hasher.hash(name, nlen);
  HashTable& ht = table(get_table_index(hash));
  auto to = (TagObject*) ht.find(hash, name, nlen);
  if (to == nullptr) {
    to = (TagObject*) fpc_memory.alloc(TagObject::calculate_size(nlen));
    new (to) TagObject(hash, name, nlen, false);
    ht.add(to);
    PERF_INCREMENT_DOMAIN_COUNTER(GLOBAL, Cache_Hits)
  } else {
    PERF_INCREMENT_DOMAIN_COUNTER(GLOBAL, Cache_Misses)
  }
  return to;
}

TagObject* TagStore::extract_tag_names(ListChunk& list, TagObject** tags, c3_uint_t& ntags,
  bool& format_is_ok, bool& all_tags_found) const {
  ntags = 0;
  TagObject* shortest_tag = nullptr;
  format_is_ok = false;
  all_tags_found = true;
  if (list.is_valid()) {
    format_is_ok = true;
    /*
     * We combine search for tags with figuring out which tag, of all found ones, has the shortest chain of
     * marked objects (this is more efficient than having a separate extra search/comparison pass, and only then
     * removing the tag from the array). Knowing the tag with least number of marked objects helps
     * optimize further processing (at least in case of finding objects matching all tags).
     */
    for (c3_uint_t i = 0; i < list.get_count(); ++i) {
      StringChunk chunk = list.get_string();
      if (chunk.is_valid_name()) {
        TagObject* tag = find_tag(chunk.get_chars(), chunk.get_short_length());
        if (tag != nullptr) {
          bool is_unique = true;
          for (c3_uint_t j = 0; j < ntags; j++) {
            if (tags[j] == tag) {
              is_unique = false;
              break;
            }
          }
          if (is_unique) {
            if (shortest_tag != nullptr) {
              if (shortest_tag->get_num_marked_objects() > tag->get_num_marked_objects()) {
                tags[ntags++] = shortest_tag;
                shortest_tag = tag;
              } else {
                tags[ntags++] = tag;
              }
            } else {
              shortest_tag = tag;
            }
          }
        } else {
          all_tags_found = false;
        }
      } else {
        format_is_ok = false;
        break;
      }
    }
  }
  return shortest_tag;
}

void TagStore::dispose_tag(TagObject* to) const {
  c3_assert(to && !to->get_num_marked_objects());
  HashTable& ht = table(get_table_index(to));
  ht.remove(to);
  fpc_memory.free(to, to->get_size());
}

void TagStore::unlink_object_tags(PageObject* po) const {
  c3_assert(po && po->flags_are_set(HOF_LINKED_BY_TM) && po->get_num_tag_refs());
  c3_uint_t i = 0;
  do {
    TagObject* empty = po->get_tag_ref(i).unlink();
    if (empty != nullptr) { // unlink() guarantees that it's not the "untagged" chain
      dispose_tag(empty);
    }
  } while (++i < po->get_num_tag_refs());
  po->clear_flags(HOF_LINKED_BY_TM);
}

void TagStore::unlink_object(PayloadHashObject* pho) const {
  /*
   * An "unlink object" message came from FPC optimizer that was doing garbage collection.
   *
   * After unlinking the object, the FPC optimizer (but not session optimizer) sends message directly to
   * the tag manager, so here, after unlinking tags, we have to send the object to FPCs queue of deleted
   * objects ourselves.
   *
   * This is the only exception; usually, it is tag manager that handles the object first and only then
   * sends it to the optimizer (which, in turn, may post it to the store's queue of deleted objects after
   * unlinking it from its chains -- if the request was to remove the object).
   */
  assert(pho != nullptr && pho->get_type() == HOT_PAGE_OBJECT);
  auto po = (PageObject*) pho;
  LockableObjectGuard guard(po);
  if (guard.is_locked()) {
    c3_assert(po->flags_are_set(HOF_BEING_DELETED) && po->flags_are_clear(HOF_DELETED | HOF_LINKED_BY_OPTIMIZER));
    po->try_dispose_buffer(fpc_memory);
    if (po->flags_are_set(HOF_LINKED_BY_TM)) {
      unlink_object_tags(po);
    }
    guard.unlock();
    get_page_store().post_unlink_message(po);
  }
}

bool TagStore::tag_enum_callback(void* context, HashObject* ho) {
  c3_assert(context && ho && ho->get_type() == HOT_TAG_OBJECT);
  auto to = (TagObject*) ho;
  if (to->get_num_marked_objects() > 0 && !to->is_untagged()) {
    auto list = (PayloadListChunkBuilder*) context;
    c3_assert(list->is_valid());
    list->add(to->get_name_length(), to->get_name());
  }
  return true;
}

bool TagStore::object_cleanup_unlink_callback(void* context, HashObject* ho) {
  c3_assert(context && ho && ho->get_type() == HOT_PAGE_OBJECT);
  if (ho->flags_are_set(HOF_LINKED_BY_TM)) {
    auto po = (PageObject*) ho;
    auto tag_store = (TagStore*) context;
    tag_store->unlink_object_tags(po);
  }
  return true;
}

bool TagStore::object_unlink_callback(void* context, HashObject* ho) {
  c3_assert(context && ho && ho->get_type() == HOT_PAGE_OBJECT);
  auto po = (PageObject*) ho;
  LockableObjectGuard guard(po);
  if (guard.is_locked()) {
    if (po->flags_are_clear(HOF_BEING_DELETED) && po->flags_are_set(HOF_LINKED_BY_TM)) {
      auto info = (tag_unlink_info_t*) context;
      bool do_unlink;
      switch (info->tui_condition) {
        case TSC_ALWAYS:
          do_unlink = true;
          break;
        case TSC_MATCH:
        case TSC_NOT_MATCH:
          do_unlink = po->matches_tags(1, info->tui_tags, info->tui_ntags);
          if (info->tui_condition == TSC_NOT_MATCH) {
            do_unlink = !do_unlink;
          }
          break;
        default:
          c3_assert_failure();
          do_unlink = false;
      }
      if (do_unlink) {
        info->tui_tag_store.unlink_object_tags(po);
        po->set_flags(HOF_BEING_DELETED);
        guard.unlock();
        // notify optimizer
        info->tui_tag_store.get_optimizer().post_delete_message(po);
      }
    }
  }
  return true;
}

bool TagStore::object_enum_callback(void* context, HashObject* ho) {
  c3_assert(context && ho && ho->get_type() == HOT_PAGE_OBJECT);
  auto po = (PageObject*) ho;
  LockableObjectGuard guard(po);
  if (guard.is_locked()) {
    auto info = (tag_object_info_t*) context;
    if (po->flags_are_clear(HOF_BEING_DELETED) && po->flags_are_set(HOF_LINKED_BY_TM)) {
      bool do_list;
      switch (info->toi_condition) {
        case TSC_ALWAYS:
          do_list = true;
          break;
        case TSC_MATCH:
        case TSC_NOT_MATCH:
          do_list = po->matches_tags(1, info->toi_tags, info->toi_ntags);
          if (info->toi_condition == TSC_NOT_MATCH) {
            do_list = !do_list;
          }
          break;
        default:
          c3_assert_failure();
          do_list = false;
      }
      if (do_list) {
        info->toi_list.add(po->get_name_length(), po->get_name());
      }
    }
  }
  return true;
}

bool TagStore::unlink_objects(TagObject** tags, c3_uint_t ntags,
  TagStore::tag_select_condition_t condition) const {
  tag_unlink_info_t info(*this, tags, ntags, condition);
  return get_page_store().lock_enumerate_all(&info, object_unlink_callback);
}

bool TagStore::unlink_all_objects() const {
  return unlink_objects(nullptr, 0, TSC_ALWAYS);
}

bool TagStore::enum_objects(PayloadListChunkBuilder& list, TagObject** tags, c3_uint_t ntags,
  TagStore::tag_select_condition_t condition) const {
  tag_object_info_t info(list, tags, ntags, condition);
  return get_page_store().lock_enumerate_all(&info, object_enum_callback);
}

bool TagStore::enum_all_objects(PayloadListChunkBuilder& list) const {
  return enum_objects(list, nullptr, 0, TSC_ALWAYS);
}

void TagStore::add_dummy_references(TagObject** tags, c3_uint_t ntags) {
  assert(tags);
  for (c3_uint_t i = 0; i < ntags; i++) {
    TagObject* tag = tags[i];
    c3_assert(tag && tag->get_num_marked_objects());
    tag->add_reference();
  }
}

void TagStore::remove_dummy_references(TagObject** tags, c3_uint_t ntags) const {
  assert(tags);
  for (c3_uint_t i = 0; i < ntags; i++) {
    TagObject* tag = tags[i];
    c3_assert(tag && tag->get_num_marked_objects());
    if (tag->remove_reference()) {
      dispose_tag(tag);
    }
  }
}

void TagStore::process_save_command(CommandReader& cr, PayloadHashObject* pho) {
  /*
   * A `SAVE` command came from a connection thread.
   *
   * Here, we link the object (found or created by the connection thread) into tags' chains, and send it
   * to the FPC optimizer, which always receives these "add object" requests from tag manager (as opposed
   * to the session optimizer, which receives them directly from the connection thread).
   *
   * RESPONSE: `OK` or `ERROR` response had been sent by the connection thread, which should have parsed
   * and validated entire command.
   */
  assert(pho && pho->get_type() == HOT_PAGE_OBJECT);
  auto po = (PageObject*) pho;
  LockableObjectGuard guard(po);
  if (guard.is_locked()) {

    // 1) unlink (but not dispose yet!) existing tags
    c3_uint_t num_existing_tags = po->get_num_tag_refs();
    TagObject* empty_tags[num_existing_tags];
    c3_uint_t num_empty_tags = 0;
    if (po->flags_are_set(HOF_LINKED_BY_TM)) {
      c3_assert(num_existing_tags);
      c3_uint_t i = 0;
      do {
        TagObject* empty = po->get_tag_ref(i).unlink();
        if (empty != nullptr) { // unlink() guarantees that it's not the "untagged" chain
          empty_tags[num_empty_tags++] = empty;
        }
      } while (++i < num_existing_tags);
    } else {
      if (po->flags_are_set(HOF_BEING_DELETED)) {
        // a concurrent request managed to delete the object before it was linked into TM chains
        return;
      }
      c3_assert(num_existing_tags == 0);
    }

    // 2) link new tags
    CommandHeaderIterator iterator(cr);
    iterator.get_string(); // skip record ID
    NumberChunk agent = iterator.get_number();
    NumberChunk lifetime = iterator.get_number();
    ListChunk list = iterator.get_list();
    c3_assert(agent.is_valid() && lifetime.is_valid() && list.is_valid());
    c3_uint_t num_passed_tags = list.get_count();
    if (num_passed_tags != 0) {
      const char* tag_names[num_passed_tags];
      c3_ushort_t tag_name_lengths[num_passed_tags];
      c3_uint_t num_unique_tags = 0;
      for (c3_uint_t j = 0; j < num_passed_tags; j++) {
        StringChunk str = list.get_string();
        const char* tag_name = str.get_chars();
        const c3_ushort_t tag_name_length = str.get_short_length();
        bool is_unique = true;
        for (c3_uint_t k = 0; k < num_unique_tags; k++) {
          if (tag_name_lengths[k] == tag_name_length && std::memcmp(tag_names[k], tag_name, tag_name_length) == 0) {
            is_unique = false;
            break;
          }
        }
        if (is_unique) {
          tag_names[num_unique_tags] = tag_name;
          tag_name_lengths[num_unique_tags++] = tag_name_length;
        }
      }
      c3_assert(num_unique_tags);
      po->set_num_tag_refs(num_unique_tags); // optionally reallocate tag xrefs
      for (c3_uint_t l = 0; l < num_unique_tags; l++) {
        TagObject* to = find_create_tag(tag_names[l], tag_name_lengths[l]);
        po->get_tag_ref(l).link(po, to);
      }
    } else {
      po->set_num_tag_refs(1);
      c3_assert(ts_untagged);
      po->get_tag_ref(0).link(po, ts_untagged);
    }

    // 3) remove tags that remain empty after we re-linked new tags
    for (c3_uint_t m = 0; m < num_empty_tags; m++) {
      TagObject* empty = empty_tags[m];
      if (empty->get_num_marked_objects() == 0) {
        dispose_tag(empty);
      }
    }

    // 4) mark page object as linked into tag manager's chains and unlock it
    po->set_flags(HOF_LINKED_BY_TM);
    guard.unlock();

    // 5) notify optimizer about the new OR updated object
    c3_timestamp_t lt = lifetime.is_negative()? Timer::MAX_TIMESTAMP: lifetime.get_uint();
    get_optimizer().post_write_message(po, (user_agent_t) agent.get_uint(), lt);
  }
}

void TagStore::process_remove_command(PayloadHashObject* pho) {
  /*
   * A `REMOVE` command came from a connection thread.
   *
   * Here, we unlink the object from all tags, and send the object to the FPC optimizer, which always
   * receives "remove object" requests from the tag manager (whereas session optimizer receives them
   * directly from the connection threads).
   *
   * RESPONSE: `OK` or `ERROR` response had been sent by the connection thread, which should have parsed
   * and validated entire command.
   */
  assert(pho && pho->get_type() == HOT_PAGE_OBJECT);
  auto po = (PageObject*) pho;
  LockableObjectGuard guard(po);
  if (guard.is_locked()) {
    // connection thread was supposed to mark the object as "deleted"
    c3_assert(po->flags_are_set(HOF_BEING_DELETED));
    if (po->flags_are_set(HOF_LINKED_BY_TM)) {
      unlink_object_tags(po);
      po->try_dispose_buffer(fpc_memory);
      guard.unlock();
      // notify optimizer
      get_optimizer().post_delete_message(po);
    }
  }
}

void TagStore::process_clean_command(CommandReader& cr) {
  /*
   * A `CLEAN` command came from a connection thread.
   *
   * We delegate "remove all" and "do garbage collection" request to FPC optimizer (which will then send
   * "unlink object" command for each individual object back to tag manager), and we process all
   * tag-related requests ourselves (and send individual "remove object" commands to the FPC optimizer in
   * the process).
   *
   * The above-described sequence of actions ensures that all command implementations are atomic, in that
   * only the objects and/or tags that exist at the time of command execution are affected: because both
   * optimizer and tag manager have single threads only. The "remove objects not marked with specified
   * tags" needs special care, since to implement it we have to lock/unlock FPC store's hash tables one
   * by one, and it is possible that a page object is created in a yet unlocked table after the command
   * execution had already been started; however, that object's tags would not be linked yet, so it
   * cannot be affected either: we just need to check if the object is tagged already.
   *
   * RESPONSE: `OK` on success, or `ERROR` in case of invalid format (tags' existence does *not* affect
   * format checks).
   */
  command_status_t status = CS_FORMAT_ERROR;
  CommandHeaderIterator iterator(cr);
  NumberChunk mode_number = iterator.get_number();
  if (mode_number.is_valid_uint() && !PayloadChunkIterator::has_payload_data(cr)) {
    c3_uint_t mode = mode_number.get_uint();
    if (mode == CM_ALL) {
      if (!iterator.has_more_chunks()) {
        unlink_all_objects();
        status = CS_SUCCESS;
      }
    } else if (mode == CM_OLD) {
      if (!iterator.has_more_chunks()) {
        // notify optimizer
        get_optimizer().post_gc_message(0);
        status = CS_SUCCESS;
      }
    } else {
      ListChunk list = iterator.get_list();
      if (list.is_valid()) {
        TagObject* tags[list.get_count()];
        c3_uint_t ntags;
        bool format_is_ok;
        bool all_tags_found;
        TagObject* shortest = extract_tag_names(list, tags, ntags, format_is_ok, all_tags_found);
        if (format_is_ok) {
          switch (mode) {
            case CM_MATCHING_ALL_TAGS:
              /*
               * If server console or PHP client are told to issue this server command with an empty list
               * of tags, they just return immediately and do not even call the server, so we won't ever
               * see this command with empty list of tags sent by console or PHP client. However, some
               * other (custom) implementation of the client may just go ahead with whatever tag list it
               * got, so we have to process this case here as well.
               *
               * If not all tags were identified, then there does not exist an object marked with "all
               * specified tags".
               */
              if (shortest != nullptr && all_tags_found) {
                c3_assert(shortest->get_num_marked_objects());
                shortest->add_reference();
                add_dummy_references(tags, ntags);
                TagRef* ref = shortest->get_first_ref();
                while (ref != nullptr) {
                  PageObject* po = ref->get_page_object();
                  TagRef* next_ref = ref->get_next_ref();
                  if (po->flags_are_clear(HOF_BEING_DELETED)) {
                    LockableObjectGuard guard(po);
                    if (guard.is_locked()) {
                      if (po->flags_are_clear(HOF_BEING_DELETED)) {
                        c3_assert(po->flags_are_set(HOF_LINKED_BY_TM));
                        if (ntags == 0 || po->matches_tags(ntags, tags, ntags)) {
                          po->set_flags(HOF_BEING_DELETED);
                          unlink_object_tags(po);
                          guard.unlock();
                          // notify optimizer
                          get_optimizer().post_delete_message(po);
                        }
                      }
                    }
                  }
                  ref = next_ref;
                }
                tags[ntags++] = shortest;
                remove_dummy_references(tags, ntags);
              }
              status = CS_SUCCESS;
              break;
            case CM_NOT_MATCHING_ANY_TAG:
              if (shortest == nullptr) {
                unlink_all_objects();
                status = CS_SUCCESS;
                break;
              }
              // fall through
            case CM_MATCHING_ANY_TAG:
              if (shortest != nullptr) {
                tags[ntags++] = shortest;
                add_dummy_references(tags, ntags);
                unlink_objects(tags, ntags, mode == CM_MATCHING_ANY_TAG? TSC_MATCH: TSC_NOT_MATCH);
                remove_dummy_references(tags, ntags);
              }
              status = CS_SUCCESS;
              break;
            default:
              c3_assert_failure();
          }
        }
      }
    }
  }
  if (status == CS_SUCCESS) {
    get_consumer().post_ok_response(cr);
  } else {
    get_consumer().post_format_error_response(cr);
  }
}

void TagStore::process_getids_command(CommandReader& cr) {
  /*
   * A `GETIDS` command had been received from a connection thread.
   *
   * Even thought this command does not "belong" to the tag manager, we process `GETIDS` here because
   * tag manager has all necessary helper methods for a compact and efficient implementation.
   *
   * RESPONSE: `DATA` response with list of page object IDs in the payload, or `ERROR` response in case
   * of format error (extraneous chunks).
   */
  if (ChunkIterator::has_any_data(cr)) {
    get_consumer().post_format_error_response(cr);
  } else {
    // create response object
    SocketResponseWriter* srw = ResponseObjectConsumer::create_response(cr);
    PayloadListChunkBuilder id_list(*srw, server_net_config, 0, 0, 0);

    // collect object IDs
    enum_all_objects(id_list);

    // configure response object and send it back to the socket pipeline
    if (!get_consumer().post_list_response(srw, id_list)) {
      get_consumer().post_internal_error_response(cr);
    }
  }
}

void TagStore::process_gettags_command(CommandReader& cr) {
  /*
   * A `GETTAGS` command came from a connection thread.
   *
   * RESPONSE: `DATA` response with list of tag IDs in the payload, or `ERROR` response in case of format
   * error (extraneous chunks).
   */
  if (ChunkIterator::has_any_data(cr)) {
    get_consumer().post_format_error_response(cr);
  } else {
    SocketResponseWriter* srw = ResponseObjectConsumer::create_response(cr);
    PayloadListChunkBuilder list(*srw, server_net_config, 0, 0, 0);

    // collect tag IDs
    enumerate_all(&list, tag_enum_callback);

    // configure response object and send it back to the socket pipeline
    if (!get_consumer().post_list_response(srw, list)) {
      get_consumer().post_internal_error_response(cr);
    }
  }
}

void TagStore::process_getmatchingids_command(c3_byte_t cmd, CommandReader& cr) {
  /*
   * One of the following commands had been received from a connection thread:
   * - `GETIDSMATCHINGTAGS`, or
   * - `GETIDSNOTMATCHINGTAGS`, or
   * - `GETIDSMATCHINGANYTAGS`.
   *
   * RESPONSE: `DATA` response with list of page object IDs in the payload, or `ERROR` response in case
   * of protocol format error.
   */
  // extract list of tags
  command_status_t status = CS_FORMAT_ERROR;
  CommandHeaderIterator iterator(cr);
  ListChunk tag_list = iterator.get_list();
  if (tag_list.is_valid() && !PayloadChunkIterator::has_payload_data(cr)) {
    TagObject* tags[tag_list.get_count()];
    c3_uint_t ntags;
    bool format_is_ok;
    bool all_tags_found;
    TagObject* shortest = extract_tag_names(tag_list, tags, ntags, format_is_ok, all_tags_found);
    if (format_is_ok) {
      // create response object
      SocketResponseWriter* srw = ResponseObjectConsumer::create_response(cr);
      PayloadListChunkBuilder id_list(*srw, server_net_config, 0, 0, 0);

      // collect object IDs
      switch (cmd) {
        case CMD_GETIDSMATCHINGTAGS:
          if (shortest != nullptr && all_tags_found) {
            TagRef* ref = shortest->get_first_ref();
            while (ref != nullptr) {
              PageObject* po = ref->get_page_object();
              LockableObjectGuard guard(po);
              if (guard.is_locked()) {
                if (po->flags_are_clear(HOF_BEING_DELETED)) {
                  c3_assert(po->flags_are_set(HOF_LINKED_BY_TM));
                  if (ntags == 0 || po->matches_tags(ntags, tags, ntags)) {
                    id_list.add(po->get_name_length(), po->get_name());
                  }
                }
              }
              ref = ref->get_next_ref();
            }
          }
          break;
        case CMD_GETIDSNOTMATCHINGTAGS:
          if (shortest == nullptr) {
            enum_all_objects(id_list);
            break;
          }
          // fall through
        case CMD_GETIDSMATCHINGANYTAGS:
          if (shortest != nullptr) {
            tags[ntags++] = shortest;
            enum_objects(id_list, tags, ntags, cmd == CMD_GETIDSMATCHINGANYTAGS? TSC_MATCH: TSC_NOT_MATCH);
          }
          break;
        default:
          c3_assert_failure();
      }
      // configure response object and send it back to the socket pipeline
      if (get_consumer().post_list_response(srw, id_list)) {
        status = CS_SUCCESS;
      } else {
        status = CS_FAILURE;
      }
    }
  }

  switch (status) {
    case CS_FORMAT_ERROR:
      get_consumer().post_format_error_response(cr);
      break;
    case CS_FAILURE:
      get_consumer().post_internal_error_response(cr);
      // fall through
    case CS_SUCCESS:
      break;
    default:
      c3_assert_failure();
  }
}

void TagStore::process_getmetadatas_command(CommandReader& cr, PayloadHashObject* pho) {
  /*
   * A `GETMETADATAS` command came from a connection thread.
   *
   * Connection thread should have "revived" the object if it had been expired but not deleted; here, we
   * just report object state and do not modify it.
   *
   * RESPONSE: `DATA` response with expiration time, last modification time, and list of tags; `OK`
   * response if the object was marked as "deleted" while being transferred to the tag manager; connection
   * thread would have processed format errors (and sent `ERROR` response) itself.
   */
  assert(pho && pho->get_type() == HOT_PAGE_OBJECT);
  SocketResponseWriter* srw = ResponseObjectConsumer::create_response(cr);
  auto po = (PageObject*) pho;
  LockableObjectGuard guard(po);
  if (guard.is_locked()) {
    if (po->flags_are_clear(HOF_BEING_DELETED) && po->flags_are_set(HOF_LINKED_BY_TM)) {
      // compile list of tag names
      HeaderListChunkBuilder list(*srw, server_net_config);
      for (c3_uint_t i = 0; i < po->get_num_tag_refs(); ++i) {
        TagObject* tag = po->get_tag_ref(i).get_tag_object();
        c3_assert(tag && tag->get_type() == HOT_TAG_OBJECT);
        if (!tag->is_untagged()) {
          list.estimate(tag->get_name_length());
        }
      }
      list.configure();
      for (c3_uint_t j = 0; j < po->get_num_tag_refs(); ++j) {
        TagObject* tag = po->get_tag_ref(j).get_tag_object();
        c3_assert(tag && tag->get_type() == HOT_TAG_OBJECT);
        if (!tag->is_untagged()) {
          list.add(tag->get_name_length(), tag->get_name());
        }
      }
      list.check();
      // currently, `c3_timestamp_t` is signed, but this will change, so we pass it using 'U' specifier
      if (get_consumer().post_data_response(srw, "UUL",
        po->get_expiration_time(), po->get_last_modification_time(), &list)) {
        // notify optimizer
        user_agent_t ua = po->get_user_agent();
        if (ua < UA_NUMBER_OF_ELEMENTS) {
          /*
           * If user agent equals its maximum value, it means that metadata request came at a time
           * when the object had just been created, and was not yet processed by the optimizer (object ctor
           * sets user agent to `UA_NUMBER_OF_ELEMENTS`; optimizer then sets it to actual value). The whole
           * point of "notifying the optimizer" is to let it move the object up its chains (and to possibly
           * "resurrect" it); since the object apparently sits in optimizer's queue already as part of
           * `OR_WRITE` message, we do not have to worry about it here.
           *
           * The only other code that sets user agent to `UA_NUMBER_OF_ELEMENTS` is in payload object store,
           * during disposal of the object. However, precondition for that code is that the object has to be
           * marked as deleted already, and we know that right here that's not the case.
           */
          get_optimizer().post_read_message(po, ua);
        }
      } else {
        get_consumer().post_internal_error_response(cr);
      }
      return;
    }
  }

  // the object had been deleted while the message was "traveling" to tag manager...
  get_consumer().post_ok_response(srw);
}

void TagStore::process_id_message(TagStore::TagMessage &msg) {
  c3_uint_t num, requested;
  switch (msg.get_id()) {
    case TC_UNLINK_OBJECT:
      unlink_object(msg.get_object());
      return;
    case TC_CAPACITY_CHANGE:
      requested = msg.get_capacity();
      num = ts_queue.set_capacity(requested);
      log(LL_VERBOSE, "%s: queue capacity set to %u (requested: %u)", get_name(), num, requested);
      return;
    case TC_MAX_CAPACITY_CHANGE:
      requested = msg.get_capacity();
      num = ts_queue.set_max_capacity(requested);
      log(LL_VERBOSE, "%s: max queue capacity set to %u (requested: %u)", get_name(), num, requested);
      return;
    case TC_QUIT:
      enter_quit_state();
      return;
    default:
      c3_assert_failure();
  }
}

void TagStore::process_command_message(TagStore::TagMessage& msg) {
  CommandReader* cr = msg.get_command();
  c3_assert(cr && cr->is_active());
  PayloadHashObject* pho = msg.get_object();
  switch (cr->get_command_id()) {
    case CMD_SAVE:
      c3_assert(pho);
      process_save_command(*cr, pho);
      break;
    case CMD_REMOVE:
      c3_assert(pho);
      process_remove_command(pho);
      break;
    case CMD_CLEAN:
      c3_assert(pho == nullptr);
      process_clean_command(*cr);
      break;
    case CMD_GETIDS:
      c3_assert(pho == nullptr);
      process_getids_command(*cr);
      break;
    case CMD_GETTAGS:
      c3_assert(pho == nullptr);
      process_gettags_command(*cr);
      break;
    case CMD_GETIDSMATCHINGTAGS:
      c3_assert(pho == nullptr);
      process_getmatchingids_command(CMD_GETIDSMATCHINGTAGS, *cr);
      break;
    case CMD_GETIDSNOTMATCHINGTAGS:
      c3_assert(pho == nullptr);
      process_getmatchingids_command(CMD_GETIDSNOTMATCHINGTAGS, *cr);
      break;
    case CMD_GETIDSMATCHINGANYTAGS:
      c3_assert(pho == nullptr);
      process_getmatchingids_command(CMD_GETIDSMATCHINGANYTAGS, *cr);
      break;
    case CMD_GETMETADATAS:
      c3_assert(pho);
      process_getmetadatas_command(*cr, pho);
      break;
    default:
      c3_assert_failure();
  }
  ReaderWriter::dispose(cr);
}

void TagStore::thread_proc(c3_uint_t id, ThreadArgument arg) {
  Thread::set_state(TS_ACTIVE);
  auto ts = (TagStore*) arg.get_pointer();
  // `allocate()` must have been called before starting the thread
  assert(ts && ts->is_initialized());
  for (;;) { // main loop
    // see if main/configuration thread told us to quit
    if (!ts->ts_quitting && Thread::received_stop_request()) {
      ts->enter_quit_state();
    }
    // get next message
    TagMessage msg;
    if (ts->ts_quitting) {
      msg = ts->ts_queue.try_get();
      if (!msg.is_valid()) {
        // no [more] outstanding messages to process; we're done
        break;
      }
    } else {
      Thread::set_state(TS_IDLE);
      msg = ts->ts_queue.get();
      Thread::set_state(TS_ACTIVE);
    }
    // process message; `msg` should be a valid message at this point
    if (msg.is_id_command()) {
      ts->process_id_message(msg);
    } else {
      ts->process_command_message(msg);
    }
  }
  ts->dispose_tag_store();
}

}
