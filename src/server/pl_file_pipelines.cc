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
#include "pl_file_pipelines.h"
#include "ls_utils.h"
#include "ht_shared_buffers.h"

namespace CyberCache {

///////////////////////////////////////////////////////////////////////////////
// FilePipeline
///////////////////////////////////////////////////////////////////////////////

constexpr char FilePipeline::BINLOG_SIGNATURE[8];

FilePipeline::FilePipeline(const char* name, domain_t domain, c3_ulong_t max_size):
  FileBase(max_size), fp_name(name), fp_domain(domain) {
  fp_active = true;
}

void FilePipeline::close_binlog() {
  if (is_fd_valid()) {
    c3_assert(!fp_path.is_empty());
    if (close_file()) {
      log(LL_NORMAL, "%s: closed binlog '%s'", fp_name, fp_path.get_chars());
    } else {
      log(LL_ERROR, "%s: could not close binlog '%s' (%s)",
        fp_name, fp_path.get_chars(), c3_get_error_message());
    }
  }
}

bool FilePipeline::read_binlog_header() {
  c3_assert(is_fd_valid() && !fp_path.is_empty() && get_current_size() == 0);
  binlog_header_t header;
  ssize_t n = c3_read_file(get_fd(), &header, sizeof header);
  if (n == sizeof header) {
    if (std::memcmp(header.bh_signature, BINLOG_SIGNATURE, sizeof BINLOG_SIGNATURE) != 0) {
      log(LL_ERROR, "%s: bad binlog signature in '%s'", fp_name, fp_path.get_chars());
      return false;
    }
    int major_version = C3_GET_MAJOR_VERSION(header.bh_version);
    if (major_version != C3_VERSION_MAJOR) {
      log(LL_ERROR, "%s: binlog '%s' is from incompatible version (major: %d, current: %d)",
        fp_name, fp_path.get_chars(), major_version, C3_VERSION_MAJOR);
      return false;
    }
    if (header.bh_timestamp > Timer::current_timestamp()) {
      log(LL_WARNING, "%s: binlog '%s' has timestamp that is in the future (%s)",
        fp_name, fp_path.get_chars(), Timer::to_ascii(header.bh_timestamp));
    }
    char buffer[C3_BUILD_NAME_BUFFER_SIZE];
    log(LL_VERBOSE, "%s: binlog '%s' (created: %s, version: %d.%d.%d [%s])",
      fp_name, fp_path.get_chars(), Timer::to_ascii(header.bh_timestamp),
      C3_GET_MAJOR_VERSION(header.bh_version),
      C3_GET_MINOR_VERSION(header.bh_version),
      C3_GET_PATCH_VERSION(header.bh_version),
      c3_get_build_mode_name(buffer, C3_GET_BUILD_MODE_ID(header.bh_version)));
    set_current_size(sizeof header);
    return true;
  } else {
    log(LL_ERROR, "%s: could not read binlog header of '%s' (%s)",
      fp_name, fp_path.get_chars(), c3_get_error_message());
    return false;
  }
}

bool FilePipeline::write_binlog_header() {
  c3_assert(is_fd_valid() && !fp_path.is_empty() && get_current_size() == 0);
  binlog_header_t header;
  std::memcpy(header.bh_signature, BINLOG_SIGNATURE, sizeof BINLOG_SIGNATURE);
  header.bh_version = c3lib_version_id;
  header.bh_timestamp = Timer::current_timestamp();
  ssize_t n = c3_write_file(get_fd(), &header, sizeof header);
  if (n == sizeof header) {
    set_current_size(sizeof header);
    return true;
  } else {
    log(LL_ERROR, "%s: could not write binlog header to '%s' (%s)",
      fp_name, fp_path.get_chars(), c3_get_error_message());
    return false;
  }
}

void FilePipeline::enter_quit_state() {
  fp_active = false;
  Thread::set_state(TS_QUITTING);
}

///////////////////////////////////////////////////////////////////////////////
// FileInputPipeline
///////////////////////////////////////////////////////////////////////////////

FileInputPipeline::FileInputPipeline(const char* name, domain_t domain, host_object_t host, c3_byte_t id) noexcept:
  FilePipeline(name, domain, 0),
  fip_input_queue(domain, host, DEFAULT_QUEUE_CAPACITY, MAX_QUEUE_CAPACITY, id) {
  fip_command_consumer = nullptr;
}

void FileInputPipeline::load_binlog(const char* path, c3_uint_t length) {
  if (open_file(path, FM_READ)) {
    log(LL_NORMAL, "%s: loading binlog '%s'...", fp_name, path);
    fp_path.set(fp_domain, path, length);
    if (read_binlog_header()) { // this logs errors, if any
      c3_long_t total_size = c3_get_file_size(get_fd());
      if (total_size > 0) {
        set_max_size((c3_ulong_t) total_size);
        Memory& memory = get_memory_object();
        c3_ulong_t num = 0;
        while (get_current_size() < get_max_size()) {
          auto rw = alloc<FileCommandReader>(memory);
          auto sob = SharedObjectBuffers::create_object(memory);
          new (rw) FileCommandReader(memory, get_fd(), sob);
          c3_ulong_t size;
          if (rw->read(size) == IO_RESULT_OK) {
            increment_current_size(size);
            num++;
            get_command_consumer().post_command_reader(rw);
          } else {
            log(LL_ERROR, "%s: could not read command from binlog '%s' at offset %llu",
              fp_name, path, get_current_size());
            break;
          }
          if (Thread::received_stop_request()) {
            log(LL_WARNING, "%s: received QUIT request, aborting loading binlog '%s' at offset %llu",
              fp_name, path, get_current_size());
            fp_active = false;
            break;
          }
        }
        log(LL_NORMAL, "%s: loaded %llu commands (%llu bytes) from binlog '%s'",
          fp_name, num, get_current_size(), path);
        reset_max_size();
      } else {
        log(LL_ERROR, "%s: could not get binlog file size: '%s'", fp_name, path);
      }
    }
    close_binlog();
  } else {
    log(LL_ERROR, "%s: could not open binlog file '%s' (%s)", fp_name, path, c3_get_error_message());
  }
}

bool FileInputPipeline::send_command(file_input_command_t cmd) {
  return fip_input_queue.put(FileInputMessage(cmd));
}

bool FileInputPipeline::send_command(file_input_command_t cmd, const void* data, size_t size) {
  return fip_input_queue.put(FileInputMessage(PipelineCommand::create(cmd, fp_domain, data, size)));
}

bool FileInputPipeline::send_load_file_command(const char* path) {
  assert(path);
  return send_command(FIC_LOAD_FILE, path, strlen(path) + 1);
}

void FileInputPipeline::thread_proc(c3_uint_t id, ThreadArgument arg) {
  Thread::set_state(TS_ACTIVE);
  auto fip = (FileInputPipeline*) arg.get_pointer();
  assert(fip && fip->fp_active && fip->fp_path.is_empty() && fip->is_fd_invalid());
  while (fip->fp_active && !Thread::received_stop_request()) {
    Thread::set_state(TS_IDLE);
    FileInputMessage msg = fip->fip_input_queue.get();
    Thread::set_state(TS_ACTIVE);
    switch (msg.get_type()) {
      case CMT_ID_COMMAND:
        switch (msg.get_id_command()) {
          case FIC_QUIT:
            fip->enter_quit_state();
            break;
          default:
            c3_assert_failure();
        }
        break;
      case CMT_DATA_COMMAND: {
        c3_uint_t requested_capacity, set_capacity;
        const PipelineCommand& cmd = msg.get_data_command();
        switch (cmd.get_id()) {
          case FIC_SET_CAPACITY:
            requested_capacity = cmd.get_uint_data();
            set_capacity = fip->fip_input_queue.set_capacity(requested_capacity);
            fip->log(LL_VERBOSE, "%s: queue capacity set to %u (requested %u)",
              fip->fp_name, set_capacity, requested_capacity);
            break;
          case FIC_SET_MAX_CAPACITY:
            requested_capacity = cmd.get_uint_data();
            set_capacity = fip->fip_input_queue.set_max_capacity(requested_capacity);
            fip->log(LL_VERBOSE, "%s: max queue capacity set to %u (requested %u)",
              fip->fp_name, set_capacity, requested_capacity);
            break;
          case FIC_LOAD_FILE:
            fip->load_binlog((const char*) cmd.get_data(), cmd.get_size());
            break;
          default:
            c3_assert_failure();
        }
        break;
      }
      default:
        c3_assert_failure();
    }
  }
  fip->close_binlog();
  Thread::set_state(TS_QUITTING);
}

///////////////////////////////////////////////////////////////////////////////
// FileOutputPipeline
///////////////////////////////////////////////////////////////////////////////

FileOutputPipeline::FileOutputPipeline(const char* name, domain_t domain, host_object_t host, c3_byte_t id) noexcept:
  FilePipeline(name, domain, DEFAULT_ROTATION_THRESHOLD),
  fop_input_queue(domain, host, DEFAULT_QUEUE_CAPACITY, MAX_QUEUE_CAPACITY, id) {
  fop_sync_mode = SM_NONE;
  fop_binlog_size_warning = false;
  fop_binlog_io_error = false;
}

void FileOutputPipeline::on_closing_binlog() {
  // do nothing by default
}


void FileOutputPipeline::dispose() {
  close_binlog();
  fp_path.empty();
  fop_rotation_path.empty();
  fop_input_queue.dispose();
}

bool FileOutputPipeline::send_command(file_output_command_t cmd) {
  return fop_input_queue.put(FileOutputMessage(cmd));
}

bool FileOutputPipeline::send_command(file_output_command_t cmd, const void* data, size_t size) {
  return fop_input_queue.put(FileOutputMessage(PipelineCommand::create(cmd, fp_domain, data, size)));
}

bool FileOutputPipeline::send_command(file_output_command_t cmd, const char* path) {
  assert(path && path[0]);
  return fop_input_queue.put(FileOutputMessage(PipelineCommand::create(cmd, fp_domain,
    path, std::strlen(path) + 1)));
}

void FileOutputPipeline::open_binlog_error(const char* action) {
  log(LL_ERROR, "%s: could not %s binlog '%s'", fp_name, action, fp_path.get_chars());
  close_file();
}

void FileOutputPipeline::open_binlog(const char* reason) {
  c3_assert(is_fd_invalid());
  const char* path = fp_path.get_chars();
  if (path != nullptr) {
    log(LL_NORMAL, "%s: opening binlog '%s' %s", fp_name, path, reason);
    if (c3_file_access(path)) {
      ssize_t pos;
      if (!open_file(path, FM_READWRITE, fop_sync_mode) ||
          !read_binlog_header() || (pos = c3_seek_file(get_fd(), 0, PM_END)) < 0) {
        open_binlog_error("restart existing");
      } else {
        set_current_size((c3_ulong_t) pos);
      }
    } else {
      if (!open_file(path, FM_CREATE, fop_sync_mode) || !write_binlog_header()) {
        open_binlog_error("start");
      }
    }
  }
}

void FileOutputPipeline::rotate_binlog(const char* reason) {
  log(LL_NORMAL, "%s: rotating binlog '%s' because %s", fp_name, fp_path.get_chars(), reason);
  close_binlog();
  char rotation_path[MAX_FILE_PATH_LENGTH];
  switch (LogUtils::rotate_log(fp_path.get_chars(), fop_rotation_path.get_chars(), rotation_path)) {
    case RR_SUCCESS:
    case RR_SUCCESS_RND:
      log(LL_NORMAL, "%s: binlog successfully moved to '%s'", fp_name, rotation_path);
      break;
    default:
      log(LL_ERROR, "%s: could not rotate binlog (template: '%s')", fp_name, fop_rotation_path.get_chars());
  }
  open_binlog("after rotation");
}

void FileOutputPipeline::process_id_command(file_output_command_t cmd) {
  switch (cmd) {
    case FOC_DISABLE_ROTATION:
      fop_rotation_path.empty();
      log(LL_VERBOSE, "%s: disabled binlog rotation", fp_name);
      return;
    case FOC_ROTATE_BINLOG:
      if (fp_path.is_empty()) {
        log(LL_WARNING, "%s: received ROTATE request, but binlog had not been started yet", fp_name);
      }
      else if (fop_rotation_path.is_empty()) {
        log(LL_WARNING, "%s: received binlog ROTATE request, but rotation path had not been set", fp_name);
      } else {
        rotate_binlog("ROTATE request received");
      }
      return;
    case FOC_SET_SYNC_MODE_NONE:
      if (fop_sync_mode != SM_NONE) {
        close_binlog();
        fop_sync_mode = SM_NONE;
        open_binlog("upon SYNC 'none' command");
      } else {
        log(LL_VERBOSE, "%s: received SYNC 'none' command, but binlog is already in this mode", fp_name);
      }
      return;
    case FOC_SET_SYNC_MODE_DATA_ONLY:
      if (fop_sync_mode != SM_DATA_ONLY) {
        close_binlog();
        fop_sync_mode = SM_DATA_ONLY;
        open_binlog("upon SYNC 'data-only' command");
      } else {
        log(LL_VERBOSE, "%s: received SYNC 'data-only' command, but binlog is already in this mode",
          fp_name);
      }
      return;
    case FOC_SET_SYNC_MODE_FULL:
      if (fop_sync_mode != SM_FULL) {
        close_binlog();
        fop_sync_mode = SM_FULL;
        open_binlog("upon SYNC 'full' command");
      } else {
        log(LL_VERBOSE, "%s: received SYNC 'full' command, but binlog is already in this mode", fp_name);
      }
      return;
    case FOC_CLOSE_BINLOG:
      close_binlog();
      on_closing_binlog();
      return;
    case FOC_QUIT:
      log(LL_VERBOSE, "%s: received QUIT request", fp_name);
      enter_quit_state();
      return;
    default:
      c3_assert_failure();
  }
}

void FileOutputPipeline::process_data_command(const PipelineCommand& pc) {
  c3_uint_t requested_capacity, set_capacity;
  switch (pc.get_id()) {
    case FOC_OPEN_BINLOG:
      close_binlog();
      fp_path.set(fp_domain, (const char*) pc.get_data(), pc.get_size());
      open_binlog("upon OPEN command");
      break;
    case FOC_SET_ROTATION_PATH: {
      auto path = (const char*) pc.get_data();
      if (LogUtils::get_log_rotation_type(path) == RT_INVALID) {
        log(LL_ERROR, "%s: invalid rotation template '%s'", fp_name, path);
      } else {
        fop_rotation_path.set(fp_domain, path, pc.get_size());
        log(LL_NORMAL, "%s: rotation template set to '%s'", fp_name, path);
      }
      break;
    }
    case FOC_SET_ROTATION_THRESHOLD: {
      c3_ulong_t threshold = pc.get_ulong_data();
      if (threshold >= MIN_ROTATION_THRESHOLD && threshold <= MAX_ROTATION_THRESHOLD) {
        set_max_size(threshold);
        log(LL_VERBOSE, "%s: rotation threshold set to %llu bytes", fp_name, threshold);
      } else {
        log(LL_ERROR, "%s: rotation threshold %llu not in %llu..%llu range",
          fp_name, threshold, MIN_ROTATION_THRESHOLD, MAX_ROTATION_THRESHOLD);
      }
      break;
    }
    case FOC_SET_CAPACITY:
      requested_capacity = pc.get_uint_data();
      set_capacity = fop_input_queue.set_capacity(requested_capacity);
      log(LL_VERBOSE, "%s: queue capacity set to %u (requested %u)",
        fp_name, set_capacity, requested_capacity);
      break;
    case FOC_SET_MAX_CAPACITY:
      requested_capacity = pc.get_uint_data();
      set_capacity = fop_input_queue.set_max_capacity(requested_capacity);
      log(LL_VERBOSE, "%s: max queue capacity set to %u (requested %u)",
        fp_name, set_capacity, requested_capacity);
      break;
    default:
      c3_assert_failure();
  }
}

void FileOutputPipeline::process_object(ReaderWriter& rw) {
  c3_assert(rw.is_clear(IO_FLAG_IS_RESPONSE) && rw.is_clear(IO_FLAG_IS_READER) && rw.is_clear(IO_FLAG_NETWORK));
  if (is_fd_valid()) { // is binlog enabled? if not, just silently ignore the object
    rw.io_rewind(get_fd(), INVALID_IPV4_ADDRESS);
    c3_ulong_t ntotal;
    if (rw.write(ntotal) == IO_RESULT_OK) {
      increment_current_size(ntotal);
      if (get_current_size() >= get_max_size()) {
        if (fop_rotation_path.is_empty()) {
          if (!fop_binlog_size_warning) {
            log(LL_WARNING, "%s: max binlog size %llu had been exceeded for '%s', "
                "but binlog rotation had not been enabled",
              fp_name, get_max_size(), fp_path.get_chars());
            fop_binlog_size_warning = true;
          }
        } else {
          rotate_binlog("binlog size limit exceeded");
        }
      }
    } else {
      if (!fop_binlog_io_error) {
        log(LL_ERROR, "%s: could not write command to binlog '%s'",
          fp_name, fp_path.get_chars());
        log(LL_ERROR, "%s: subsequent errors will NOT be logged", fp_name);
        fop_binlog_io_error = true;
      }
    }
  }
}

bool FileOutputPipeline::send_open_binlog_command(const char* path) {
  if (path != nullptr && path[0] != '\0') {
    return send_command(FOC_OPEN_BINLOG, path);
  } else {
    return send_command(FOC_CLOSE_BINLOG);
  }
}

bool FileOutputPipeline::send_set_rotation_path_command(const char* path) {
  if (path != nullptr && path[0] != '\0') {
    return send_command(FOC_SET_ROTATION_PATH, path);
  } else {
    return send_command(FOC_DISABLE_ROTATION);
  }
}

bool FileOutputPipeline::send_set_sync_mode_command(sync_mode_t mode) {
  file_output_command_t command;
  switch (mode) {
    case SM_NONE:
      command = FOC_SET_SYNC_MODE_NONE;
      break;
    case SM_DATA_ONLY:
      command = FOC_SET_SYNC_MODE_DATA_ONLY;
      break;
    default:
      c3_assert(mode == SM_FULL);
      command = FOC_SET_SYNC_MODE_FULL;
  }
  return send_command(command);
}

bool FileOutputPipeline::send_object(FileCommandWriter* rw) {
  return fop_input_queue.put(FileOutputMessage(rw));
}

void FileOutputPipeline::thread_proc(c3_uint_t id, ThreadArgument arg) {
  Thread::set_state(TS_ACTIVE);
  auto fop = (FileOutputPipeline*) arg.get_pointer();
  assert(fop && fop->fp_active && fop->fp_path.is_empty() && fop->is_fd_invalid());
  bool keep_going = true;
  do {
    if (fop->fp_active && Thread::received_stop_request()) {
      fop->enter_quit_state();
    }
    // in "quitting" mode, we only fetch messages until the queue is depleted
    FileOutputMessage msg;
    if (fop->fp_active) {
      Thread::set_state(TS_IDLE);
      msg = fop->fop_input_queue.get();
      Thread::set_state(TS_ACTIVE);
    } else {
      msg = fop->fop_input_queue.try_get();
    }
    switch (msg.get_type()) {
      case CMT_INVALID:
        c3_assert(!fop->fp_active);
        keep_going = false;
        break;
      case CMT_ID_COMMAND:
        fop->process_id_command(msg.get_id_command());
        break;
      case CMT_DATA_COMMAND:
        fop->process_data_command(msg.get_data_command());
        break;
      case CMT_OBJECT:
        if (fop->is_fd_valid()) {
          fop->process_object(msg.get_object());
        }
    }
  } while (keep_going);

  fop->dispose();
}

///////////////////////////////////////////////////////////////////////////////
// FileOutputNotifyingPipeline
///////////////////////////////////////////////////////////////////////////////

FileOutputNotifyingPipeline::FileOutputNotifyingPipeline(const char* name, domain_t domain, host_object_t host,
  c3_byte_t id) noexcept: FileOutputPipeline(name, domain, host, id),
  fonp_output_queue(domain, host, DEFAULT_QUEUE_CAPACITY, MAX_QUEUE_CAPACITY, id) {
  // effectively disables rotation
  set_max_size(terabytes2bytes(1));
}

void FileOutputNotifyingPipeline::on_closing_binlog() {
  fonp_output_queue.put(binlog_notification_t(FON_BINLOG_CLOSED));
}

#if INCLUDE_WAIT_FOR_NOTIFICATION_VOID
void FileOutputNotifyingPipeline::wait_for_notification() {
  c3_assert_def(binlog_notification_t msg) fonp_output_queue.get();
  c3_assert(msg.get_type() == FON_BINLOG_CLOSED);
}
#endif // INCLUDE_WAIT_FOR_NOTIFICATION_VOID

bool FileOutputNotifyingPipeline::wait_for_notification(c3_uint_t seconds) {
  binlog_notification_t msg = fonp_output_queue.get(seconds * 1000);
  return msg.get_type() == FON_BINLOG_CLOSED;
}

} // CyberCache
