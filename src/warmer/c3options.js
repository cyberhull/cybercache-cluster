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

var system = require('system');
var fs = require("fs");

///////////////////////////////////////////////////////////////////////////////
// DEFINITIONS
///////////////////////////////////////////////////////////////////////////////

// actions that should be taken after options parsing (iternal to this module)
const PA_EXECUTE       = 0;
const PA_PRINT_VERSION = 1;
const PA_PRINT_HELP    = 2;

// application exit codes
const EC_OK               = 0;
const EC_NOTHING_TO_DO    = 1;
const EC_ARGUMENT_ERROR   = 2;
const EC_RECORDED_ERRORS  = 3;
const EC_MISSING_PAGE     = 4;
const EC_MISSING_RESOURCE = 5;

// sorting orders
const SO_AS_IS               = 0;
const SO_LOW_PRIORITY_FIRST  = 1;
const SO_HIGH_PRIORITY_FIRST = 2;

// user agents
const UA_UNKNOWN = 0;
const UA_BOT     = 1;
const UA_WARMER  = 2;
const UA_USER    = 3;

// display modes
const DM_NONE       = 0;
const DM_REGULAR    = 1;
const DM_TOTAL_TIME = 2;
const DM_PAGE_TIME  = 3;

// exit reasons
const ER_USER_INTERRUPT   = 0;
const ER_RECORDED_ERRORS  = 1;
const ER_MISSING_PAGE     = 2;
const ER_MISSING_RESOURCE = 3;

// log levels
const LL_NONE    = 0;
const LL_ERROR   = 1;
const LL_WARNING = 2;
const LL_TERSE   = 3;
const LL_NORMAL  = 4;
const LL_VERBOSE = 5;

///////////////////////////////////////////////////////////////////////////////
// MODULE INTERAFCE
///////////////////////////////////////////////////////////////////////////////

/**
 * All CyberCache warmer options and their defaults (except `-u`/`--url`) for warm up runs
 */
var options = {
  profileRuns:     0,                       // number of profile runs; 0 means do cache warm-up
  queueSize:       16,                      // number of concurrent page requests
  firstURL:        0,                       // skip this many URLs in collected set
  numURLs:         0,                       // use only this many URLs from the collected set
  oldestURL:       0,                       // exclude URLs with <lastmod> tag older than this
  lowestPriority:  0.0,                     // lowest URL priority to include
  highestPriority: 1.0,                     // highest URL priority to include
  sortOrder:       SO_HIGH_PRIORITY_FIRST,  // sort order, an `SO_xxx` constant
  userAgent:       UA_WARMER,               // user agent to emulate while accessing pages, a `UA_xxx` constant
  warmUpPass:      false,                   // delay, in milliseconds, between warm-up pass and profiling
  loadImages:      false,                   // whether to load images
  loadCSS:         false,                   // whether to load external style sheets
  loadJavaScript:  true,                    // whether to load external JavaScript files
  logLevel:        LL_NORMAL,               // log level, an `LL_xxx` constant
  logFilePath:     'cybercache_warmer.log', // name of the log file
  displayMode:     DM_REGULAR,              // display/reporting mode, a `DM_xxx` constant
  exitIf:          ER_USER_INTERRUPT,       // error tolerance, an `ER_xxx` constant
  numScreenshots:  0,                       // how many screenshots to take
  windowWidth:     0,                       // width of each screenshot
  windowHeight:    0,                       // height of each screenshot
  imageTemplate:   false,                   // screenshot path template
  saveTimeout:     1000                     // timeout before first try to save a screenshot
};

/**
 * Flags for options whose defaults can be set automatically upon `-p`/`--profile`; if an option is set explicitly,
 * its "flag" it set to `true`, so that not to reset the option upon following `-p`/`--profile`.
 */
var option_set = {
  sortOrder:       false,
  userAgent:       false,
  exitIf:          false
};

/**
 * List of source map files and URLs specified on the command line. Both map files and URLs are kept in the
 * single array for the processor to be able to implement `SO_AS_IS` sorting order. URLs' priorities are also
 * stored in the `urls` object, so if an element of the `sources` array also exists as a key/member in the `urls`
 * object, then it's a URL; otherwise, it's a site map XML or URL list file.
 *
 * @type {Array}
 */
var sources = [];

/**
 * Hash table with keys being URLs, and values being URL priorities.
 *
 * @type {{}}
 */
var urls = {};

// export public constants
exports.EC_OK                  = EC_OK;
exports.EC_NOTHING_TO_DO       = EC_NOTHING_TO_DO;
exports.EC_ARGUMENT_ERROR      = EC_ARGUMENT_ERROR;
exports.EC_RECORDED_ERRORS     = EC_RECORDED_ERRORS;
exports.EC_MISSING_PAGE        = EC_MISSING_PAGE;
exports.EC_MISSING_RESOURCE    = EC_MISSING_RESOURCE;
exports.SO_AS_IS               = SO_AS_IS;
exports.SO_LOW_PRIORITY_FIRST  = SO_LOW_PRIORITY_FIRST;
exports.SO_HIGH_PRIORITY_FIRST = SO_LOW_PRIORITY_FIRST;
exports.UA_UNKNOWN             = UA_UNKNOWN;
exports.UA_BOT                 = UA_BOT;
exports.UA_WARMER              = UA_WARMER;
exports.UA_USER                = UA_USER;
exports.DM_NONE                = DM_NONE;
exports.DM_REGULAR             = DM_REGULAR;
exports.DM_TOTAL_TIME          = DM_TOTAL_TIME;
exports.DM_PAGE_TIME           = DM_PAGE_TIME;
exports.ER_USER_INTERRUPT      = ER_USER_INTERRUPT;
exports.ER_RECORDED_ERRORS     = ER_RECORDED_ERRORS;
exports.ER_MISSING_PAGE        = ER_MISSING_PAGE;
exports.ER_MISSING_RESOURCE    = ER_MISSING_RESOURCE;
exports.LL_NONE                = LL_NONE;
exports.LL_ERROR               = LL_ERROR;
exports.LL_WARNING             = LL_WARNING;
exports.LL_TERSE               = LL_TERSE;
exports.LL_NORMAL              = LL_NORMAL;
exports.LL_VERBOSE             = LL_VERBOSE;

// export options' values
exports.values = options;

// export map files and URLs
exports.sources = sources;

// export URLs' priorities
exports.urls = urls;

///////////////////////////////////////////////////////////////////////////////
// HELPER FUNCTIONS
///////////////////////////////////////////////////////////////////////////////

function optionError(message) {
  console.error("ERROR: " + message);
  switch (options.displayMode) {
    case DM_TOTAL_TIME:
    case DM_PAGE_TIME:
      console.log("0");
  }
  phantom.exit(EC_ARGUMENT_ERROR);
}

function parseNumber(num, name, minValue, maxValue) {
  if (maxValue === undefined) {
    maxValue = Number.MAX_VALUE;
    if (minValue === undefined) {
      minValue = 0;
    }
  }
  num = parseInt(num); // callers ensure that `num` string is composed of digits only
  if (num < minValue || num > maxValue) {
    optionError("Argument of option '--" + name + "' not in " + minValue + " .. " + maxValue + ": '" + num + "'");
  }
  return num;
}

function parseTimestamp(ts, name, future) {
  // 1) process timestamp specified as an interval
  var match = /^(\d+)([smhdw])$/.exec(ts);
  if (match) {
    var offset = parseInt(match[1]);
    if (future !== true) {
      offset = -offset;
    }
    switch (match[2]) {
      case 'w':
        offset *= 7;
        // fall through
      case 'd':
        offset *= 24;
        // fall through
      case 'h':
        offset *= 60;
        // fall through
      case 'm':
        offset *= 60;
        // fall through
      case 's':
        offset *= 1000;
    }
    return Date.now() + offset;
  }
  // 2) process "real" timestamps (we do not use JS date parsing as that would force users to use 'T' as time
  // separator, end timestamp with Z or time zone offset etc.)
  match = /^(\d{4})-(\d\d)-(\d\d)[ _]+(\d\d):(\d\d):(\d\d)$/.exec(ts);
  if (match) {
    ts = new Date(parseInt(match[1]), // year
      parseInt(match[2]) - 1,         // month (0-based)
      parseInt(match[3]),             // day
      parseInt(match[4]),             // hour
      parseInt(match[5]),             // minute
      parseInt(match[6])).valueOf();  // second
    if (future === true) {
      if (ts < Date.now()) {
        optionError("Timestamp provided to option '--" + name + "' must be in the future");
      }
    } else {
      if (ts > Date.now()) {
        optionError("Timestamp provided to option '--" + name + "' must be in the past");
      }
    }
    return ts;
  } else {
    optionError("Ill-formed timestamp argument to option '--" + name + "': '" + ts + "'");
  }
}

function parsePriority(priority, name) {
  var num = parseFloat(priority);
  if (isNaN(num) || num < 0 || num > 1) {
    optionError("Priority argument passed to option '--" + name + "' not in 0..1: '" + priority + "'");
  }
  return num;
}

function parseBoolean(value, name) {
  if (/^(true|yes|on)$/i.test(value)) {
    return true;
  }
  if (/^(false|no|off)$/i.test(value)) {
    return false;
  }
  optionError("Invalid boolean argument passed to option '--" + name + "': '" + value + "'");
}

///////////////////////////////////////////////////////////////////////////////
// PARSERS OF INDIVIDUAL OPTIONS
///////////////////////////////////////////////////////////////////////////////

function parseProfileOption(numRuns) {
  numRuns = parseNumber(numRuns, "profile");
  options.profileRuns = numRuns;
  /*
   * If certain options had not been set explicitly yet, set them to default values
   * corresponding to selected mode (warm-up/profiling).
   */
  if (!option_set.sortOrder) {
    options.sortOrder = numRuns? SO_HIGH_PRIORITY_FIRST: SO_AS_IS;
  }
  if (!option_set.userAgent) {
    options.userAgent = numRuns? UA_USER: UA_WARMER;
  }
  if (!option_set.exitIf) {
    options.exitIf = numRuns? ER_MISSING_PAGE: ER_USER_INTERRUPT;
  }
}

function parseRangeOption(lowest, highest) {
  lowest = parsePriority(lowest, "range");
  highest = parsePriority(highest, "range");
  if (lowest > highest) {
    optionError("Invalid priorities passed to option '--range' (exclude all URLs): " + lowest + ".." + highest);
  }
  options.lowestPriority = lowest;
  options.highestPriority = highest;
}

function parseSortOption(order) {
  switch (order) {
    case "low-priority-first":
      options.sortOrder = SO_LOW_PRIORITY_FIRST;
      break;
    case "high-priority-first":
      options.sortOrder = SO_HIGH_PRIORITY_FIRST;
      break;
    case "as-is":
      options.sortOrder = SO_AS_IS;
      break;
    default:
      optionError("Invalid order passed to option '--sort': '" + order + "'");
  }
  option_set.sortOrder = true;
}

function parseAgentOption(agent) {
  switch (agent) {
    case "unknown":
      options.userAgent = UA_UNKNOWN;
      break;
    case "bot":
      options.userAgent = UA_BOT;
      break;
    case "warmer":
      options.userAgent = UA_WARMER;
      break;
    case "user":
      options.userAgent = UA_USER;
      break;
    default:
      optionError("Invalid user agent passed to option '--agent': '" + agent + "'");
  }
  option_set.userAgent = true;
}

function parseURLOption(priority, url) {
  priority = parsePriority(priority, "url");
  if (!/^https?:\/\/.*$/.test(url)) {
    url = "http://" + url;
  }
  urls[url] = priority;
  sources.push(url);
}

function parseLogLevelOption(level) {
  switch (level) {
    case "none":
      options.logLevel = LL_NONE;
      break;
    case "error":
      options.logLevel = LL_ERROR;
      break;
    case "warning":
      options.logLevel = LL_WARNING;
      break;
    case "terse":
      options.logLevel = LL_TERSE;
      break;
    case "normal":
      options.logLevel = LL_NONE;
      break;
    case "verbose":
      options.logLevel = LL_VERBOSE;
      break;
    default:
      optionError("Invalid level passed to option '--log-level': '" + level + "'");
  }
}

function parseDisplayOption(mode) {
  switch (mode) {
    case "regular":
      options.displayMode = DM_REGULAR;
      break;
    case "total-time":
      options.displayMode = DM_TOTAL_TIME;
      break;
    case "page-time":
      options.displayMode = DM_PAGE_TIME;
      break;
    case "none":
      options.displayMode = DM_NONE;
      break;
    default:
      optionError("Invalid mode passed to option '--display': '" + mode + "'");
  }
}

function parseExitIfOption(reason) {
  switch (reason) {
    case "user-interrupt":
      options.exitIf = ER_USER_INTERRUPT;
      break;
    case "recorded-errors":
      options.exitIf = ER_RECORDED_ERRORS;
      break;
    case "missing-page":
      options.exitIf = ER_MISSING_PAGE;
      break;
    case "missing-resource":
      options.exitIf = ER_MISSING_RESOURCE;
      break;
    default:
      optionError("Invalid reason passed to option '--exit-if': '" + reason + "'");
  }
  option_set.exitIf = true;
}

function parseExportOption(args) {
  var match = /^(all|\d+),(\d*),(\d*),(.+)$/.exec(args);
  if (match) {
    options.numScreenshots = match[1] === "all"? 1000000000: parseInt(match[1]);
    var width = match[2].length? parseInt(match[2]): 0;
    var height = match[3].length? parseInt(match[3]): 0;
    if (width < 1 && height < 1) {
      width = 1024;
      height = 768;
    } else if (width < 1) {
      width = Math.floor(height * 4 / 3);
    } else if (height < 1) {
      height = Math.floor(width * 3 / 4);
    }
    options.windowWidth = Math.max(width, 120);
    options.windowHeight = Math.max(height, 90);
    options.imageTemplate = match[4]; // will be validated upon finishing processing of all options
  } else {
    optionError("Ill-formed arguments to option '--export': '" + args + "'");
  }
}

///////////////////////////////////////////////////////////////////////////////
// PROCESS INFO REQUEST OPTIONS
///////////////////////////////////////////////////////////////////////////////

function printInfo(full) {
  var version = require("./c3version.js");
  console.log("CyberCache Warmer/Profiler " + version.versionNumber +
    ".\nWritten by Vadim Sytnikov" +
    ".\nCopyright (C) 2016-2019 CyberHULL. All rights reserved." +
    ".\nThis program is free software distributed under GPL v2+ license.");

  if (full) {
    var helpText = "\n" +
      "Usage:\n" +
      "\n" +
      "  cybercachewarmer [ <option> [ <option> [...]]] <url-file> [ <ulr-file> [...]]\n" +
      "\n" +
      "where <url-file> is either a site map XML file exported from Magento 2, or a\n" +
      "custom file with URLs; valid options are:\n" +
      "\n" +
      "  -p[:<num-runs>] or --profile[=<num-runs>]\n" +
      "      Runs warmer in profiling mode: does <num-runs> of runs over specified URL\n" +
      "      set. The argument is optional; if omitted, assumed to be `1`.\n" +
      "\n" +
      "  -q:<num-requests> or --queue=<num-requests>\n" +
      "      Sets number of concurrent page requests that will be used to load site\n" +
      "      pages. Must be within 1..256 range; default is 16.\n" +
      "\n" +
      "  -b:<number> or --begin=<number>\n" +
      "      Skips first `<number>` of URLs in specified set; default is 0.\n" +
      "\n" +
      "  -n:<number> or --number=<number>\n" +
      "      Uses only up to `<number>` of URLs from specified set; zero <number> is\n" +
      "      treated as \"use all\"; default is 0.\n" +
      "\n" +
      "  -o:<timestamp> or --older-than=<timestamp>\n" +
      "      Only loads pages modified after specified timestamp. The `<timestamp>`\n" +
      "      must be either in \"YYYY-MM-DD HH:MM:SS\", or in \"<number>{s|m|h|d|w}\"\n" +
      "      format. Underscore can be used instead of space as date/time separator.\n" +
      "\n" +
      "  -r:<low-priority>,<high-priority> or --range=<low-priority>,<high-priority>\n" +
      "      Only load pages with priorities within specified range; the range is\n" +
      "      inclusive and defaults to `0.0..1.0`.\n" +
      "\n" +
      "  -s:<order> or --sort=<order>\n" +
      "      Sorts loaded URLs before accessing them; valid sort orders are\n" +
      "      `low-priority-first`, `high-priority-first` (default in cache  warming\n" +
      "      mode), and `as-is` (default in profiling mode).\n" +
      "\n" +
      "  -a:<user-agent> or --agent=<user-agent>\n" +
      "      Emulates specified user agent; possible user agents are `unknown`, `bot`,\n" +
      "      `warmer` (default in cache warming mode), and `user` (default in\n" +
      "      performance measurement mode).\n" +
      "\n" +
      "  -w:<timestamp> or --warm-up=<timestamp>\n" +
      "      Tells profiler to do an extra non-timed warm-up run before actual\n" +
      "      profiling runs; ignored if `-p` is not specified. The `<timestamp>`\n" +
      "      argument has the same format as that of `-o`/`--older-than` option, it\n" +
      "      makes profiler wait specified time (or until specified time) before doing\n" +
      "      actual profiler run(s).\n" +
      "\n" +
      "  -u:<priority>,<url> or --url=<priority>,<url>\n" +
      "      Adds specified URL to the list of URLs that will be accessed; the\n" +
      "      `<priority>` is a floating point number in range `0.0..1.0`, inclusive.\n" +
      "\n" +
      "  -i[:<boolean>] or --images[=<boolean>]\n" +
      "      Tells warmer/profiler whether to load images and favicons; default\n" +
      "      boolean is `true`. If not specified at all, images will not be loaded.\n" +
      "\n" +
      "  -c[:<boolean>] or --css[=<boolean>]\n" +
      "      Tells warmer/profiler whether to load style sheets and web fonts; default\n" +
      "      boolean is `true`. If not specified at all, CSS/fonts will not be loaded.\n" +
      "\n" +
      "  -j[:<boolean>] or --javascript[=<boolean>]\n" +
      "      Tells warmer/profiler whether to load and execute JavaScript [files];\n" +
      "      default boolean is `true`. If not specified at all, JavaScript [files]\n" +
      "      will be loaded and executed.\n" +
      "\n" +
      "  -l:<level> or --log-level=<level>\n" +
      "      Sets log level; valid levels are `none`, `error`, `warning`, `terse`,\n" +
      "      `normal` (the default), and `verbose`.\n" +
      "\n" +
      "  -f:<log-file-path> or --log-file=<log-file-path>\n" +
      "      Sets the name of the log file; default is `cybercache_warmer.log`.\n" +
      "\n" +
      "  -d:<mode> or --display=<mode>\n" +
      "      Sets display mode; valid modes are `regular` (the default), `total-time`,\n" +
      "      `page-time`, and `none`.\n" +
      "\n" +
      "  -e:<reason> or --exit-if=<reason>\n" +
      "      Sets error tolerance; valid reasons are `user-interrupt` (default in\n" +
      "      cache warming mode), `recorded-errors`, `missing-page` (default in\n" +
      "      profiling mode), and `missing-resource`.\n" +
      "\n" +
      "  -x:<num>,<width>,<height>,<path> or --export=<num>,<width>,<height>,<path>\n" +
      "      Exports loaded page or pages to image file(s). `<num>` is number of last\n" +
      "      pages to save, or `all`; `<width>` and `<height>` set viewport size. The\n" +
      "      `<path>` specifies file name and format (`.pdf`, `.png`, `.jpg`, `.bmp`,\n" +
      "      `.ppm`, `.gif`); if path contains directory, if will be created; if name\n" +
      "      contains '@', it will be replaced with page component of the URL.\n" +
      "\n" +
      "  -t:<milliseconds> or --timeout=<milliseconds>\n" +
      "      Sets timeout before page is exported to an image file, which may be\n" +
      "      necessary for elaborate AJAX pages; default timeout is 1000.\n" +
      "\n" +
      "  -v or --version\n" +
      "      Prints out version number and exits.\n" +
      "\n" +
      "  -h or --help\n" +
      "      Prints out help information and exits.\n" +
      "\n" +
      "  @<option-file>\n" +
      "      Loads and parses specified option (a.k.a. response) file.\n" +
      "\n" +
      "  --\n" +
      "      Marks end of options.\n" +
      "\n" +
      "See `cybercachewarmer(1)` for the detailed description.";

    console.log(helpText);
  }
  phantom.exit(EC_NOTHING_TO_DO);
}

///////////////////////////////////////////////////////////////////////////////
// OPTION SET PARSERS
///////////////////////////////////////////////////////////////////////////////

function parseOptionFile(path, level, action) {
  try {
    var lines = [];
    var file = fs.open(path, "r");
    while(!file.atEnd()) {
      var line = file.readLine().trim();
      if (line.length > 0 && line.charAt(0) !== '#') {
        lines.push(line);
      }
    }
    file.close();
    return lines.length > 0? parseOptions(lines, 0, level, action): action;
  } catch (error) {
    optionError("Could not read option file '" + path + "' (" + error.message + ")");
  }
}

function parseOptions(args, firstArg, level, action) {

  var optionsEnded = false;
  for (var i = firstArg; i < args.length; i++) {
    var arg = args[i];
    var firstChar = arg.charAt(0);
    if ((firstChar !== '-' && firstChar !== '@') || optionsEnded) {
      sources.push(arg);
      optionsEnded = true;
      continue;
    }
    if (arg === '--') {
      optionsEnded = true;
      continue;
    }
    var match = /^(-p|--profile)$/.exec(arg);
    if (match) {
      parseProfileOption(1);
      continue;
    }
    match = /^(-p:|--profile=)(\d+)$/.exec(arg);
    if (match) {
      parseProfileOption(match[2]);
      continue;
    }
    match = /^(-q:|--queue=)(\d+)$/.exec(arg);
    if (match) {
      options.queueSize = parseNumber(match[2], "queue", 1, 256);
      continue;
    }
    match = /^(-b:|--begin=)(\d+)$/.exec(arg);
    if (match) {
      options.firstURL = parseNumber(match[2], "begin");
      continue;
    }
    match = /^(-n:|--number=)(\d+)$/.exec(arg);
    if (match) {
      options.numURLs = parseNumber(match[2], "number");
      continue;
    }
    match = /^(-o:|--older-than=)(.+)$/.exec(arg);
    if (match) {
      options.oldestURL = parseTimestamp(match[2], "older-than", false);
      continue;
    }
    match = /^(-r:|--range=)(.+),(.+)$/.exec(arg);
    if (match) {
      parseRangeOption(match[2], match[3]);
      continue;
    }
    match = /^(-s:|--sort=)(.+)$/.exec(arg);
    if (match) {
      parseSortOption(match[2]);
      continue;
    }
    match = /^(-a:|--agent=)(.+)$/.exec(arg);
    if (match) {
      parseAgentOption(match[2]);
      continue;
    }
    match = /^(-w:|--warm-up=)(.+)$/.exec(arg);
    if (match) {
      var currentTS = Date.now();
      options.warmUpPass = parseTimestamp(match[2], "warm-up", true) - currentTS;
      continue;
    }
    match = /^(-u:|--url=)(.+),(.+)$/.exec(arg);
    if (match) {
      parseURLOption(match[2], match[3]);
      continue;
    }
    match = /^(-i|--images)$/.exec(arg);
    if (match) {
      options.loadImages = true;
      continue;
    }
    match = /^(-i:|--images=)(\w+)$/.exec(arg);
    if (match) {
      options.loadImages = parseBoolean(match[2], "images");
      continue;
    }
    match = /^(-c|--css)$/.exec(arg);
    if (match) {
      options.loadCSS = true;
      continue;
    }
    match = /^(-c:|--css=)(\w+)$/.exec(arg);
    if (match) {
      options.loadCSS = parseBoolean(match[2], "css");
      continue;
    }
    match = /^(-j|--javascript)$/.exec(arg);
    if (match) {
      options.loadJavaScript = true;
      continue;
    }
    match = /^(-j:|--javascript=)(\w+)$/.exec(arg);
    if (match) {
      options.loadJavaScript = parseBoolean(match[2], "javascript");
      continue;
    }
    match = /^(-l:|--log-level=)(\w+)$/.exec(arg);
    if (match) {
      parseLogLevelOption(match[2]);
      continue;
    }
    match = /^(-f:|--log-file=)(.+)$/.exec(arg);
    if (match) {
      options.logFilePath = match[2];
      continue;
    }
    match = /^(-d:|--display=)(.+)$/.exec(arg);
    if (match) {
      parseDisplayOption(match[2]);
      continue;
    }
    match = /^(-e:|--exit-if=)(.+)$/.exec(arg);
    if (match) {
      parseExitIfOption(match[2]);
      continue;
    }
    match = /^(-x:|--export-if=)(.+)$/.exec(arg);
    if (match) {
      parseExportOption(match[2]);
      continue;
    }
    match = /^(-t:|--timeout=)(\d+)$/.exec(arg);
    if (match) {
      options.saveTimeout = parseNumber(match[2], "timeout", 1, 60 * 1000);
      continue;
    }
    if (/^(-v|--version)$/.test(arg)) {
      // option `--help` takes precedence over `--version`
      if (action !== PA_PRINT_HELP) {
        action = PA_PRINT_VERSION;
      }
      continue;
    }
    if (/^(-h|--help)$/.test(arg)) {
      action = PA_PRINT_HELP;
      continue;
    }
    match = /^@(.+)$/.exec(arg);
    if (match) {
      var path = match[1];
      if (level > 16) {
        optionError("Too many nested option files. Is '" + path + "' included recursively?");
      }
      action = parseOptionFile(path, level + 1, action);
      continue;
    }
    optionError("Unrecognized or ill-formed option: '" + arg + "'");
  }
  return action;
}

function validateOptions() {
  if (sources.length === 0) {
    optionError("No URLs or URL sources specified");
  }
  if (options.numScreenshots > 0) {
    if (options.numScreenshots > 1 && options.profileRuns > 0) {
      optionError("Cannot export more than one page to image file in profiling mode");
    }
    var match = /^(.+\/)?([^/]+)\.(pdf|png|jpe?g|bmp|ppm|gif)$/.exec(options.imageTemplate);
    if (match) {
      var name = match[2];
      var pos = name.indexOf('@');
      if (pos >= 0 && name.indexOf('@', pos + 1) > 0) {
        optionError("The '@' placeholder can be specified in image template once: '" + name + "'");
      }
      var dir = match[1];
      if (dir !== undefined && dir.length > 0) {
        if (dir.indexOf('@') >= 0) {
          optionError("The '@' placeholder can only be present in name part of image template: '" + dir + "'");
        }
        if (!fs.makeTree(dir)) {
          optionError("Could not create directory for exported images: '" + dir + "'");
        }
      }
    } else {
      optionError("Ill-formed export image file template: '" + options.imageTemplate + "'");
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// OPTION PARSER INTERFACE
///////////////////////////////////////////////////////////////////////////////

/**
 * Parses command line arguments and populates `options[]` and `sources[]` arrays. If help or version was requested,
 * or no sources were specified, or there was an error processing arguments, exits to the shell.
 */
exports.parse = function () {

  switch (parseOptions(system.args, 1, 0, PA_EXECUTE)) {
    case PA_PRINT_VERSION:
      printInfo(false);
      // fall through (comment to suppress IDE warning: `printInfo()` does not return)
    case PA_PRINT_HELP:
      printInfo(true);
      // fall through (ditto)
    default:
      validateOptions();
  }
};
