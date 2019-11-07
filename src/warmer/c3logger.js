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

var fs = require("fs");
var version = require("./c3version.js");
var options = require("./c3options.js");

var file = false; // log file stream

///////////////////////////////////////////////////////////////////////////////
// IMPLEMENTATION
///////////////////////////////////////////////////////////////////////////////

function twoDigits(num) {
  return '' + (num > 9? num: '0' + num);
}

function getTimestampString() {
  var ts = new Date();
  return ts.getFullYear() + "-" + twoDigits(ts.getMonth() + 1) + "-" + twoDigits(ts.getDate()) + " " +
    twoDigits(ts.getHours()) + ":" + twoDigits(ts.getMinutes()) + ":" + twoDigits(ts.getSeconds());
}

function logMessage() {
  if (file !== false) {
    var level = options.LL_NORMAL;
    var first = 0;
    if (arguments.length > 0 && typeof arguments[0] === "number") {
      level = arguments[0];
      first = 1;
    }
    if (level <= options.values.logLevel) {
      var message;
      switch (level) {
        case options.LL_WARNING:
          message = "[WARNING] ";
          break;
        case options.LL_ERROR:
          message = "[ERROR] ";
          break;
        default:
          message = "";
      }
      var delimiter = "";
      for (var i = first; i < arguments.length; i++) {
        message += delimiter + arguments[i];
        delimiter = " ";
      }
      if (options.values.displayMode === options.DM_REGULAR) {
        console.log(message);
      }
      file.writeLine(getTimestampString() + " " + message);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// INTERFACE
///////////////////////////////////////////////////////////////////////////////

/**
 * Opens log file specified using option `--log-file` and logs program/version information.
 *
 * Does nothing if `--log-level` was set to `none`.
 */
exports.openLogFile = function() {
  if (file === false && options.values.logLevel !== options.LL_NONE) {
    var path = options.values.logFilePath;
    try {
      file = fs.open(path, "a");
      logMessage(options.LL_TERSE, "CyberCache Warmer/Profiler", version.versionNumber);
      logMessage("Written by Vadim Sytnikov");
      logMessage("Copyright (C) CyberHULL.");
    } catch (error) {
      console.error("Could not open log file '" + path + "' (" + error.message + ")");
      phantom.exit(options.EC_ARGUMENT_ERROR);
    }
  }
};

/**
 * Writes message to the log file and, optionally, to the console.
 *
 * Takes arbitrary number of arguments. If first argument is a number, it is treated as log level (should be one
 * of the `options.LL_xxx` constants); otherwise, level of the message is assumed to be `options.LL_NORMAL`.
 *
 * If logging level set using `--log-level` option is "more terse/less verbose" than log level of the message,
 * then it is neither written to the file nor printed to console.
 *
 * All the arguments (except very first if it's a number) are concatenated using space as the delimiter and logged
 * as a single string. If `--display` mode is `regular`, the string is also copied to console.
 */
exports.log = logMessage;

/**
 * Closes log file opened by `openLogFile()`.
 *
 * If log file was not open at the time of the call, then does nothing.
 */
exports.closeLogFile = function() {
  if (file !== false) {
    file.close();
    file = false;
  }
};
