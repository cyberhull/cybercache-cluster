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
var options = require("./c3options.js");
var logger = require("./c3logger.js");

///////////////////////////////////////////////////////////////////////////////
// IMPLEMENTATION
///////////////////////////////////////////////////////////////////////////////

// List of all loaded URLs
var urlList = [];

// Associative array with URLs being keys, and priorities being values
var urlPriorities = {};

function urlError(message) {
  logger.log(options.LL_ERROR, message);
  logger.closeLogFile();
  phantom.exit(options.EC_ARGUMENT_ERROR);
}

function urlFormatError(path, message) {
  urlError("File '" + path + "': " + message);
}

function urlCustomFormatError(path, line, message) {
  urlError("File '" + path + "', line " + line + ": " + message);
}

function addURL(url, priority) {
  if (priority >= options.values.lowestPriority && priority <= options.values.highestPriority) {
    if (!urlPriorities.hasOwnProperty(url)) {
      urlList.push(url);
      logger.log(options.LL_VERBOSE, "Added URL:", url);
    }
    urlPriorities[url] = priority;
  } else {
    logger.log("Rejected URL '" + url + "' as having out-of-range priority (" + priority + ")");
  }
}

function loadCustomURLFile(path) {
  try {
    var file = fs.open(path, "r");
    logger.log("Reading custom URL file '" + path + "'...");
    var lineNum = 1;
    while(!file.atEnd()) {
      var line = file.readLine().trim();
      if (line.length > 0 && line.charAt(0) !== '#') {
        var match = /^([\d.]+)\s+(https?:\/\/.+)$/.exec(line);
        if (match) {
          var priority = parseFloat(match[1]);
          if (priority >= 0.0 && priority <= 1.0) {
            addURL(match[2], priority);
          } else {
            urlCustomFormatError(path, lineNum, "URL priority not in [0.0..1.0]: " + priority);
          }
        } else {
          urlCustomFormatError(path, lineNum, "invalid URL format (must be '<priority> <url>')");
        }
      }
      lineNum++;
    }
    file.close();
  } catch (error) {
    urlError("Could not read custom URL file '" + path + "' (" + error.message + ")");
  }
}

function loadSiteMapFile(path) {
  try {
    var xml = fs.read(path);
    logger.log("Reading site map file '" + path + "'...");
    var match, pattern = /<url>(.+?)<\/url>/g;
    while ((match = pattern.exec(xml)) !== null) {
      var url_tag = match[1];
      var loc_tag = /<loc>(https?:\/\/.+)<\/loc>/.exec(url_tag);
      if (loc_tag) {
        var url = loc_tag[1];
        var priority_tag = /<priority>([\d.]+)<\/priority>/.exec(url_tag);
        if (priority_tag) {
          var priority = parseFloat(priority_tag[1]);
          if (priority >= 0.0 && priority <= 1.0) {
            var lastmod_tag = /<lastmod>(.+)<\/lastmod>/.exec(url_tag);
            if (lastmod_tag) {
              var lastmod = lastmod_tag[1];
              /*
               * Even though it is often recommended to manually parse date/time strings, we use `Date.parse()`
               * here because Magento timestamps are in ISO-8601 format with time zone offset specified as +/-hh:ss,
               * so `parse()` should work fine.
               */
              var ts = Date.parse(lastmod);
              if (!isNaN(ts)) {
                if (ts >= options.values.oldestURL) {
                  addURL(url, priority);
                } else {
                  logger.log(options.LL_VERBOSE,
                    "Rejected URL '" + url + "' as outdated [" + lastmod + " --> " + (new Date(ts)) + "]");
                }
              } else {
                urlFormatError(path, "ill-formed <lastmod> tag in URL: " + url_tag);
              }
            } else {
              urlFormatError(path, "no <lastmod> tag in URL: " + url_tag);
            }
          } else {
            urlFormatError(path, "priority not in [0.0..1.0] in URL: " + url_tag);
          }
        } else {
          urlFormatError(path, "invalid or no <priority> tag in URL: " + url_tag);
        }
      } else {
        urlFormatError(path, "invalid or no <loc> tag in URL: " + url_tag);
      }
    }
  } catch (error) {
    urlError("Could not read site map file '" + path + "' (" + error.message + ")");
  }
}

///////////////////////////////////////////////////////////////////////////////
// INTERFACE
///////////////////////////////////////////////////////////////////////////////

/**
 * Loads and pre-processes (checks dates, sorts, etc.) URLs from all sources registered in the `options`
 * object. If, after pre-processing, URL set is empty, exits to the shell with code `1` ("nothing to do").
 */
exports.load = function () {

  // 1) Collect URLs (process individual URLs and URL sources)
  // ---------------------------------------------------------

  for (var i = 0; i < options.sources.length; i++) {
    var source = options.sources[i];
    if (options.urls.hasOwnProperty(source)) {
      var priority = options.urls[source];
      addURL(source, priority);
    } else {
      if (/\.xml$/i.test(source)) {
        loadSiteMapFile(source);
      } else {
        loadCustomURLFile(source);
      }
    }
  }

  logger.log(options.LL_VERBOSE, "Number of collected URLs after timestamp/priority filtering:", urlList.length);

  // 2) Check that there remained some URLs to process
  // -------------------------------------------------

  if (urlList.length === 0) {
    /*
     * Options processor would have exited with code `1` if there were no sources; therefore, all
     * URLs must have been filtered out based on their timestamps and `-o`/`--older-than` option.
     */
    logger.log(options.LL_WARNING, "All URLs had been filtered out as obsolete: nothing to do");
    logger.closeLogFile();
    phantom.exit(options.EC_NOTHING_TO_DO);
  }

  // 3) Optionally sort the URLs based on their priorities (apply `-s`/`--sort` option)
  // ----------------------------------------------------------------------------------

  if (options.values.sortOrder !== options.SO_AS_IS) {
    var ascending = options.values.sortOrder === options.SO_LOW_PRIORITY_FIRST;
    urlList.sort(function (url1, url2) {
      var priority1 = urlPriorities[url1];
      var priority2 = urlPriorities[url2];
      return ascending? priority1 - priority2: priority2 - priority1;
    });
  }

  // 4) Optionally reduce the array (apply `-b`/`--begin` and `-n`/`--number` options)
  // ---------------------------------------------------------------------------------

  var first = options.values.firstURL;
  var num = options.values.numURLs;
  if (num === 0) { // zero means "include all URLs"
    num = urlList.length;
  }
  if (first > 0 || num < urlList.length) {
    // the `options` module guarantees that `num` is at least `1`
    if (first >= urlList.length) {
      logger.log(options.LL_WARNING,
        "The '--begin' option (" + first + ") excludes all collected URLs (" + urlList.length + "): nothing to do");
      logger.closeLogFile();
      phantom.exit(options.EC_NOTHING_TO_DO);
    }
    urlList = urlList.slice(first, first + num);
  }

  // 5) Clean up and log stats
  // -------------------------

  urlPriorities = {};
  logger.log("Number of collected URLs after all filtering:", urlList.length);
};

/**
 * Get number of URLs in the set.
 *
 * @returns {Number} Number of successfully registered URLs.
 */
exports.getNumber = function () {
  return urlList.length;
};

/**
 * Get URL with specified index in the set.
 *
 * @param index URL index; must be less than the number returned by `getNumber()` function.
 * @returns {String} URL string.
 */
exports.get = function (index) {
  return urlList[index];
};
