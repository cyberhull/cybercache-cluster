/**
 * CyberCache Cluster
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
 * ----------------------------------------------------------------------------
 *
 * I/O pipelines: classes implementing file (binlog) pipelines.
 */
#ifndef _PL_FILE_PIPELINES_H
#define _PL_FILE_PIPELINES_H

#include "c3lib/c3lib.h"
#include "mt_message_queue.h"
#include "pl_socket_pipelines.h"

// whether to compile `wait_for_notification()`
#define INCLUDE_WAIT_FOR_NOTIFICATION_VOID 0

namespace CyberCache {

///////////////////////////////////////////////////////////////////////////////
// FilePipeline
///////////////////////////////////////////////////////////////////////////////

/// Base class for all file pipelines
class FilePipeline: public virtual AbstractLogger, public FileBase {
  static constexpr char BINLOG_SIGNATURE[8] = {'C', '3', 'B', 'i', 'n', 'L', 'o', 'g'};
  struct binlog_header_t {
    char           bh_signature[8]; // "C3BinLog"
    c3_uint_t      bh_version;      // version ID; major version must match current
    c3_timestamp_t bh_timestamp;    // timestamp; must be in the past
  };

protected:
  String             fp_path;         // current file path
  const char* const  fp_name;         // name of the pipeline (for logging etc.)
  const domain_t     fp_domain;       // memory domain
  bool               fp_active;       // `true` if not quitting

  FilePipeline(const char* name, domain_t domain, c3_ulong_t max_size) C3_FUNC_COLD;
  ~FilePipeline() override C3_FUNC_COLD { close_binlog(); }

  Memory& get_memory_object() const { return Memory::get_memory_object(fp_domain); }
  void close_binlog() C3_FUNC_COLD;
  bool read_binlog_header() C3_FUNC_COLD;
  bool write_binlog_header() C3_FUNC_COLD;
  void enter_quit_state() C3_FUNC_COLD;

public:
  FilePipeline(const FilePipeline&) = delete;
  FilePipeline(FilePipeline&&) = delete;

  FilePipeline& operator=(const FilePipeline&) = delete;
  FilePipeline& operator=(FilePipeline&&) = delete;
};

///////////////////////////////////////////////////////////////////////////////
// FileInputPipeline
///////////////////////////////////////////////////////////////////////////////

/// Input commands understood by binlog reader
enum file_input_command_t: c3_uintptr_t {
  FIC_INVALID = 0,      // an invalid command (placeholder)
  FIC_LOAD_FILE,        // load binlog file
  FIC_SET_CAPACITY,     // set size of the input (command) queue
  FIC_SET_MAX_CAPACITY, // set size limit to which input queue can grow automatically
  FIC_QUIT,             // quit immediately
  FIC_NUMBER_OF_ELEMENTS
};

/**
 * Server binlog loader, or binlog optimizer utility's binlog loader.
 *
 * A pipeline that is used for restoration from binlogs; its itput queue accepts only commands; it does
 * not have own output queue, but instead accepts reference to an object implementing
 * `CommandObjectConsumer` interface as an argument; when it receives "load-binlog-file" command, it
 * opens that file and starts loading command objects and pumping them to the queue of the
 * `CommandObjectConsumer`-derived object (while doing that, it does not accept any new commands, but it
 * does check thread termination requests, so there is no need to synchronize it with other potential
 * users: it won't open other binlog files until it full processes its current one, or until thread
 * termination request arrives).
 */
class FileInputPipeline: public FilePipeline {
  static constexpr c3_uint_t DEFAULT_QUEUE_CAPACITY = 4;  // initial capacity of the input queue (messages)
  static constexpr c3_uint_t MAX_QUEUE_CAPACITY     = 16; // *default* maximum capacity of the input queue

  /// Message type for binlog reader's input message queue
  typedef CommandMessage<file_input_command_t, PipelineCommand, ReaderWriter, FIC_NUMBER_OF_ELEMENTS>
    FileInputMessage;
  /// Type of the binlog reader's message queue
  typedef MessageQueue<FileInputMessage> FileInputQueue;

  FileInputQueue         fip_input_queue;      // input messages (commands)
  CommandObjectConsumer* fip_command_consumer; // where to post loaded commands

  CommandObjectConsumer& get_command_consumer() const {
    c3_assert(fip_command_consumer);
    return *fip_command_consumer;
  }
  void set_command_consumer(CommandObjectConsumer* command_consumer) C3_FUNC_COLD {
    c3_assert(command_consumer && fip_command_consumer == nullptr);
    fip_command_consumer = command_consumer;
  }

  void load_binlog(const char* path, c3_uint_t length);

  bool send_command(file_input_command_t cmd) C3_FUNC_COLD;
  bool send_command(file_input_command_t cmd, const void* data, size_t size) C3_FUNC_COLD;

public:
  FileInputPipeline(const char* name, domain_t domain, host_object_t host, c3_byte_t id) noexcept;

  void configure(CommandObjectConsumer* command_consumer) {
    set_command_consumer(command_consumer);
  }

  c3_uint_t get_queue_capacity() C3LM_OFF(const) { return fip_input_queue.get_capacity(); }
  c3_uint_t get_max_queue_capacity() C3LM_OFF(const) { return fip_input_queue.get_max_capacity(); }

  bool send_load_file_command(const char* path) C3_FUNC_COLD;
  bool send_set_capacity_command(c3_uint_t capacity) C3_FUNC_COLD {
    return send_command(FIC_SET_CAPACITY, &capacity, sizeof capacity);
  }
  bool send_set_max_capacity_command(c3_uint_t max_capacity) C3_FUNC_COLD {
    return send_command(FIC_SET_MAX_CAPACITY, &max_capacity, sizeof max_capacity);
  }
  bool send_quit_command() C3_FUNC_COLD { return send_command(FIC_QUIT); }

  // this method must *NOT* be called directly: its name should be passed to Thread::start()
  static void thread_proc(c3_uint_t id, ThreadArgument arg);
};

///////////////////////////////////////////////////////////////////////////////
// FileOutputPipeline
///////////////////////////////////////////////////////////////////////////////

/// Input commands understood by binlog writer
enum file_output_command_t: c3_uintptr_t {
  FOC_INVALID = 0,             // an invalid command (placeholder)
  FOC_OPEN_BINLOG,             // open [new] binlog file
  FOC_SET_ROTATION_PATH,       // set binlog rotation path
  FOC_SET_ROTATION_THRESHOLD,  // set maximum binlog size (rotate binlog if it grows bigger than this)
  FOC_DISABLE_ROTATION,        // reset rotation path
  FOC_ROTATE_BINLOG,           // force binlog rotation
  FOC_SET_SYNC_MODE_NONE,      // turn OFF synchronized writing
  FOC_SET_SYNC_MODE_DATA_ONLY, // turn OFF synchronized writing
  FOC_SET_SYNC_MODE_FULL,      // turn ON synchronized writing
  FOC_CLOSE_BINLOG,            // close binlog and stop saving objects, but keep getting messages
  FOC_SET_CAPACITY,            // set size of the input (object) queue
  FOC_SET_MAX_CAPACITY,        // set size limit to which input queue can grow automatically
  FOC_QUIT,                    // deplete input queue (only processing objects), then quit
  FOC_NUMBER_OF_ELEMENTS
};

/**
 * Server binlog writer, a pipeline that is used to pump data to persistent storage.
 */
class FileOutputPipeline: public FilePipeline {

  static constexpr c3_uint_t  DEFAULT_QUEUE_CAPACITY = 64;  // initial capacity of the input queue (messages)
  static constexpr c3_uint_t  MAX_QUEUE_CAPACITY     = 512; // *default* maximum capacity of the input queue
  static constexpr c3_ulong_t MIN_ROTATION_THRESHOLD = megabytes2bytes(1);
  static constexpr c3_ulong_t DEFAULT_ROTATION_THRESHOLD = megabytes2bytes(256);
  static constexpr c3_ulong_t MAX_ROTATION_THRESHOLD = terabytes2bytes(1);

  /// Message type for binlog writer's input message queue
  typedef CommandMessage<file_output_command_t, PipelineCommand, ReaderWriter, FOC_NUMBER_OF_ELEMENTS>
    FileOutputMessage;
  /// Type of the binlog writer's message queue
  typedef MessageQueue<FileOutputMessage> FileOutputQueue;

  FileOutputQueue fop_input_queue;         // input messages (commands)
  String          fop_rotation_path;       // binlog rotation path
  sync_mode_t     fop_sync_mode;           // what kind of synchronization to use during writing
  bool            fop_binlog_size_warning; // whether binlog size warning had been issued
  bool            fop_binlog_io_error;     // whether binlog I/O error had been logged

  bool send_command(file_output_command_t cmd) C3_FUNC_COLD;
  bool send_command(file_output_command_t cmd, const void* data, size_t size) C3_FUNC_COLD;
  bool send_command(file_output_command_t cmd, const char* path) C3_FUNC_COLD;

  void open_binlog_error(const char* action) C3_FUNC_COLD;
  void open_binlog(const char* reason) C3_FUNC_COLD;
  void rotate_binlog(const char* reason) C3_FUNC_COLD;
  void process_id_command(file_output_command_t cmd) C3_FUNC_COLD;
  void process_data_command(const PipelineCommand& pc) C3_FUNC_COLD;
  void process_object(ReaderWriter& rw);
  void dispose() C3_FUNC_COLD;
  virtual void on_closing_binlog() C3_FUNC_COLD;

public:
  FileOutputPipeline(const char* name, domain_t domain, host_object_t host, c3_byte_t id) noexcept C3_FUNC_COLD;

  c3_uint_t get_queue_capacity() C3LM_OFF(const) { return fop_input_queue.get_capacity(); }
  c3_uint_t get_max_queue_capacity() C3LM_OFF(const) { return fop_input_queue.get_max_capacity(); }
  sync_mode_t get_sync_mode() const { return fop_sync_mode; }
  static constexpr c3_ulong_t get_min_rotation_threshold() { return MIN_ROTATION_THRESHOLD; }
  static constexpr c3_ulong_t get_max_rotation_threshold() { return MAX_ROTATION_THRESHOLD; }

  bool send_open_binlog_command(const char* path) C3_FUNC_COLD;
  bool send_close_binlog_command() C3_FUNC_COLD { return send_open_binlog_command(nullptr); }
  bool send_set_rotation_path_command(const char* path) C3_FUNC_COLD;
  bool send_set_rotation_threshold(c3_ulong_t threshold) C3_FUNC_COLD {
    return send_command(FOC_SET_ROTATION_THRESHOLD, &threshold, sizeof threshold);
  }
  bool send_rotate_binlog_command() C3_FUNC_COLD { return send_command(FOC_ROTATE_BINLOG); }
  bool send_set_sync_mode_command(sync_mode_t mode) C3_FUNC_COLD;
  bool send_set_capacity_command(c3_uint_t capacity) C3_FUNC_COLD {
    return send_command(FOC_SET_CAPACITY, &capacity, sizeof capacity);
  }
  bool send_set_max_capacity_command(c3_uint_t max_capacity) C3_FUNC_COLD {
    return send_command(FOC_SET_MAX_CAPACITY, &max_capacity, sizeof max_capacity);
  }
  bool send_quit_command() C3_FUNC_COLD { return send_command(FOC_QUIT); }

  bool send_object(FileCommandWriter* rw);

  // this method must *NOT* be called directly: its name should be passed to Thread::start()
  static void thread_proc(c3_uint_t id, ThreadArgument arg);
};

///////////////////////////////////////////////////////////////////////////////
// FileOutputNotifyingPipeline
///////////////////////////////////////////////////////////////////////////////

/**
 * Server binlog writer, tailored for saving entire cache databases upon requests.
 */
class FileOutputNotifyingPipeline: public FileOutputPipeline {
  static constexpr c3_uint_t DEFAULT_QUEUE_CAPACITY = 2; // initial capacity of the output queue (messages)
  static constexpr c3_uint_t MAX_QUEUE_CAPACITY     = 8; // *default* maximum capacity of the output queue

  /// Notifications that can be sent by the binlog writer
  enum file_output_notification_t: c3_byte_t {
    FON_INVALID = 0,       // an invalid notification (placeholder)
    FON_BINLOG_CLOSED,     // output complete, binliog file has just been closed
    FON_NUMBER_OF_ELEMENTS
  };

  /// Structure that holds notifications
  class binlog_notification_t {
    file_output_notification_t bn_type; // notification type

  public:
    binlog_notification_t() { bn_type = FON_INVALID; }
    explicit binlog_notification_t(file_output_notification_t type) { bn_type = type; }
    binlog_notification_t(const binlog_notification_t&) = delete;
    binlog_notification_t(binlog_notification_t&& notification) noexcept { bn_type = notification.bn_type; }
    ~binlog_notification_t() = default;

    file_output_notification_t get_type() const { return bn_type; }

    binlog_notification_t& operator=(const binlog_notification_t&) = delete;
    binlog_notification_t& operator=(binlog_notification_t&& notification) noexcept {
      std::swap(bn_type, notification.bn_type);
      return *this;
    }
  };

  /// Type of the binlog writer's message queue
  typedef MessageQueue<binlog_notification_t> FileOutputNotificationQueue;

  FileOutputNotificationQueue fonp_output_queue; // output notifications

  void on_closing_binlog() override C3_FUNC_COLD;

public:
  FileOutputNotifyingPipeline(const char* name, domain_t domain, host_object_t host, c3_byte_t id)
    noexcept C3_FUNC_COLD;

  # if INCLUDE_WAIT_FOR_NOTIFICATION_VOID
  void wait_for_notification();
  #endif // INCLUDE_WAIT_FOR_NOTIFICATION_VOID
  bool wait_for_notification(c3_uint_t seconds);
};

} // CyberCache

#endif // _PL_FILE_PIPELINES_H
