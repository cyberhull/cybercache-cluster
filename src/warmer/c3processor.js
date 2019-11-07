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

var webpage = require("webpage");
var version = require("./c3version.js");
var options = require("./c3options.js");
var logger = require("./c3logger.js");
var urls = require("./c3urls.js");

// This is what we recognize as resources for the sake of error reporting (everything else is treated as pages)
const resourcePattern = /\.(css|js|[to]tf|eot|woff2?|svg|ico|gif|png|jpe?g)$/;

// If any resources (images/CSS/JS) are disabled, this variable will contain regexp matching their URLs.
var disabledResourcesPattern = false;

// Array of pages used for cache warm-up and profiling
var webPages = [];

// Object that holds status of current access pass
var passStatus = {
  isPreWarmUp:  false, // `true` if it's an extra warm-up pass before profiling
  startTime:    false, // timestamp (milliseconds since Jan 1, 1970) of when the pass started
  urlsStarted:  0,     // number of URLs passed so far to pages' `open()` methods
  urlsFinished: 0,     // number of URLs that were loaded so far
  totalURLs:    0,     // total number of URLs for this run (num unique URLs times num passed for profiling)
  numErrors:    0      // number of encountered page and/or resource loading errors
};

///////////////////////////////////////////////////////////////////////////////
// HELPER FUNCTIONS
///////////////////////////////////////////////////////////////////////////////

function createPage(index) {
  var page = webpage.create();

  // 1) Set up callbacks
  // -------------------

  page.onResourceRequested = resourceRequestCallback;
  page.onResourceError = resourceErrorCallback;
  page.onResourceTimeout = resourceTimeoutCallback; // since PhantomJS 1.2
  page.onLoadFinished =
    /*
     * we need this quirk because PhantomJS does not invoke the callback on its page object; that is, in the
     * callback, page object is not accessible through `this`
     */
    eval("(function (status) { pageLoadCallback(" + index + ", status, false); })");

  // 2) Set resource retrieval-related properties
  // --------------------------------------------

  page.settings.javascriptEnabled = options.values.loadJavaScript;
  page.settings.loadImages = options.values.loadImages;

  // 3) Set user agent
  // -----------------

  var userAgents = [
    "",                     // UA_UNKNOWN
    "spider",               // UA_BOT (use one of the many recognized keywords)
    version.getUserAgent(), // UA_WARMER
    "Chrome/60.0.3112.101"  // UA_USER (anything that's not currently recognized as a bot or warmer will do)
  ];
  page.settings.userAgent = userAgents[options.values.userAgent];

  // 4) Optionally set window/viewport dimensions
  // --------------------------------------------

  if (options.values.imageTemplate !== false) {
    var windowWidth = options.values.windowWidth;
    var windowHeight = options.values.windowHeight;
    page.viewportSize = {
      width:  windowWidth,
      height: windowHeight
    };
    page.clipRect = {
      top:    0,
      left:   0,
      width:  windowWidth,
      height: windowHeight
    }
  }

  return page;
}

function getNextURL() {
  if (passStatus.urlsStarted < passStatus.totalURLs) {
    // during pass, we can go over entire set of URLs more than once
    return urls.get(passStatus.urlsStarted++ % urls.getNumber());
  }
  return false;
}

function startAccessPass(isPreWarmUp) {

  // 1) Configure the pass
  // ---------------------

  passStatus.isPreWarmUp = isPreWarmUp;
  passStatus.urlsStarted = 0;
  passStatus.urlsFinished = 0;
  var passType;
  var numURLs = urls.getNumber();
  if (isPreWarmUp) {
    passType = "profiling warm-up"
  } else {
    if (options.values.profileRuns > 0) {
      numURLs *= options.values.profileRuns;
      passType = "profiling";
    } else {
      passType = "warming";
    }
  }
  passStatus.totalURLs = numURLs;
  passStatus.numErrors = 0;

  // 2) Start loading pages
  // ----------------------

  logger.log("Starting cache", passType, "pass (" + numURLs + " planned page hits)...");
  passStatus.startTime = Date.now();
  for (var i = 0; i < webPages.length; i++) {
    // `run()` guarantees that number of pages does not exceed the number of URLs
    webPages[i].open(getNextURL());
  }
}

function saveImage(page) {
  var path = options.values.imageTemplate;
  if (path !== false) {
    if (path.indexOf('@') >= 0) {
      /*
       * We handle URLs ending with a name, with or without an extension. Currently, we do not handle
       * URLs ending with a slash.
       */
      var url = page.url;
      var name = "";
      var match = /\/([^/]+)\.[^.]+$/.exec(url);
      if (match) {
        name = match[1];
      } else {
        match = /\/([^/]+)$/.exec(url);
        if (match) {
          name = match[1];
        }
      }
      if (name === "") {
        logger.log(options.LL_WARNING, "Could not extract name portion from URL", url);
        return;
      }
      path = path.replace("@", name);
    }
    page.render(path);
    logger.log(options.LL_VERBOSE, "Exported image '" + path + "' of page", page.url);
  }
}

function saveImageAsync(page, index) {
  var numChecks = 10; // do up to 10 tries
  var interval = setInterval(function () {
    /*
     * It would seem natural to check if the images had already been loaded, and only create interval
     * timer if they hadn't.
     *
     * The first version of `saveImageAsync()` would indeed check if images were ready right away, and would
     * only go into async loop if they were not; that, however, wouldn't help with Magento 2 pages, which
     * require plain vanilla delays to get all their AJAX stuff complete: the check for images' completion
     * would always return `true` (it would take time for Magento 2 JS to insert new images into DOM for them
     * to appear in `document.images[]`; then, some of those images would be placeholder spinners etc.).
     *
     * Despite it being unusable for Magento 2 sites, we do not completely remove image readiness check as it
     * may come handy with simpler sites.
     */
    var ready = page.evaluate(function() {
      var images = document.images;
      for (var i = 0; i < images.length; i++) {
        if (!images[i].complete) {
          return false;
        }
      }
      return true;
    });
    if (ready || --numChecks === 0) {
      saveImage(page);
      clearInterval(interval);
      pageLoadCallback(index, "success", true);
    }
  }, options.values.saveTimeout);
}

function getDurationString(milliseconds) {
  var minutes = 0;
  var seconds = 0;
  if (milliseconds >= 1000) {
    if (milliseconds >= 60 * 1000) {
      minutes = Math.floor(milliseconds / (60 * 1000));
      milliseconds %= 60 * 1000;
    }
    seconds = Math.floor(milliseconds / 1000);
    milliseconds %= 1000;
  }
  if (minutes < 10) {
    minutes = '0' + minutes;
  }
  if (seconds < 10) {
    seconds = '0' + seconds;
  }
  if (milliseconds < 100) {
    milliseconds = (milliseconds < 10? '00': '0') + milliseconds;
  }
  return minutes + ':' + seconds + '.' + milliseconds;
}

function printPassStats(totalTime) {
  var pageTime = 0;
  logger.log(options.LL_TERSE, "Loaded", passStatus.urlsFinished, "pages in", getDurationString(totalTime));
  if (passStatus.urlsFinished > 0) {
    pageTime = Math.floor(totalTime / passStatus.urlsFinished);
    logger.log(options.LL_TERSE, "Average page load time:", getDurationString(pageTime));
  }
  return pageTime;
}

function reportError(code, message) {
  logger.log(options.LL_ERROR, message);
  exitWarmer(code);
}

function exitWarmer(code, action) {

  // 1) Print out stats (actual output may be suppressed depending upon display mode)
  // --------------------------------------------------------------------------------

  var totalTime = Date.now() - passStatus.startTime;
  var pageTime = printPassStats(totalTime);

  // 2) Execute post-run actions, if any
  // -----------------------------------

  if (typeof action === "function") {
    action();
  }

  // 3) Output processing time if requested by display mode
  // ------------------------------------------------------

  switch (options.values.displayMode) {
    case options.DM_TOTAL_TIME:
      console.log(totalTime);
      break;
    case options.DM_PAGE_TIME:
      console.log(pageTime);
  }

  // 4) Adjust exit code if there were errors
  // ----------------------------------------

  if (options.values.exitIf === options.ER_RECORDED_ERRORS && passStatus.numErrors > 0) {
    code = options.ER_RECORDED_ERRORS;
  }

  // 5) Shut down and exit
  // ---------------------

  logger.closeLogFile();
  phantom.exit(code);
}

///////////////////////////////////////////////////////////////////////////////
// CALLBACKS
///////////////////////////////////////////////////////////////////////////////

function resourceRequestCallback(requestData, networkRequest) {
  if (disabledResourcesPattern !== false) {
    if (disabledResourcesPattern.test(requestData.url)) {
      // this will trigger `resourceErrorCallback()`
      networkRequest.abort();
    }
  }
}

function resourceErrorCallback(resourceError) {
  var url = requestData.url;
  if (disabledResourcesPattern !== false) {
    if (disabledResourcesPattern.test(url)) {
      // it was us who triggered this call, so nothing to do
      return;
    }
  }
  if (resourcePattern.test(url)) {
    switch (options.values.exitIf) {
      case options.ER_MISSING_RESOURCE:
        reportError(options.EC_MISSING_RESOURCE, "Could not load resource:", url);
        // fall through (comment just to satisfy IDE)
      case options.ER_RECORDED_ERRORS:
        logger.log(options.LL_WARNING, "Could not load resource", url);
        passStatus.numErrors++;
    }
  }
}

function resourceTimeoutCallback(request) {
  var url = requestData.url;
  if (resourcePattern.test(url)) {
    switch (options.values.exitIf) {
      case options.ER_MISSING_RESOURCE:
        reportError(options.EC_MISSING_RESOURCE, "Resource loading timed out:", url);
      // fall through (comment just to satisfy IDE)
      case options.ER_RECORDED_ERRORS:
        passStatus.numErrors++;
    }
  }
}

function pageLoadCallback(index, status, secondAttempt) {
  var page = webPages[index];
  if (secondAttempt === false) {
    if (status === "success") {
      logger.log(options.LL_VERBOSE, "Loaded page", page.url);
      if (options.values.profileRuns === 0 && options.values.imageTemplate !== false) {
        var remains = passStatus.totalURLs - passStatus.urlsFinished;
        if (remains <= options.values.numScreenshots) {
          /*
           * the following sets up timer "interval"; as soon as the image is written, timer callback will
           * call `pageLoadCalback()` again, this time with `imagesLoaded` == `true`
           */
          saveImageAsync(page, index);
          return;
        }
      }
    } else {
      switch (options.values.exitIf) {
        case options.ER_MISSING_PAGE:
        case options.ER_MISSING_RESOURCE:
          reportError(options.EC_MISSING_PAGE, "Could not load page:", page.url);
        // fall through (comment just to satisfy IDE)
        case options.ER_RECORDED_ERRORS:
          passStatus.numErrors++;
      }
      logger.log(options.LL_WARNING, "Could not load page", page.url);
    }
  }
  if (++passStatus.urlsFinished === passStatus.totalURLs) {
    if (passStatus.isPreWarmUp) {
      logger.log("Completed profiling warm-up pass");
      printPassStats(Date.now() - passStatus.startTime);
      var delay = options.values.warmUpPass;
      logger.log("Profiling pass will start in", getDurationString(delay));
      setTimeout(function () {
        startAccessPass(false);
      }, delay);
    } else {
      var action = false;
      /*
       * we just loaded the very last URL; if it's a profiling run, *and* we were told to save a screenshot,
       * then pass respective action to the exit procedure (to be carried out after the timer is stopped)
       */
      if (options.values.profileRuns > 0 && options.values.imageTemplate !== false) {
        action = function () {
          saveImage(page);
        }
      }
      exitWarmer(options.EC_OK, action);
    }
  } else {
    var nextURL = getNextURL();
    if (nextURL !== false) {
      if (nextURL === page.url) {
        // page could be the case if, say, we do several passes over a single page
        page.reload();
      } else {
        page.open(nextURL);
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// INTERFACE
///////////////////////////////////////////////////////////////////////////////

exports.run = function () {

  // 1) Create page objects
  // ----------------------

  var numPages = options.values.queueSize;
  if (numPages > urls.getNumber()) {
    numPages = urls.getNumber();
  }
  for (var i = 0; i < numPages; i++) {
    webPages.push(createPage(i));
  }

  // 2) Build regular expression to recognize disabled resources
  // -----------------------------------------------------------

  var rx = "";
  var separator = "";
  if (!options.values.loadCSS) {
    /*
     * this also disables web fonts; even though SVG files can be used as web fonts in Chrome, Safari,
     * and Opera browsers, we treat SVGs as images as that's their most common application
     */
    rx = "css|[to]tf|eot|woff2?";
    separator = "|";
  }
  if (!options.values.loadJavaScript) {
    rx += separator + "js";
    separator = "|";
  }
  if (!options.values.loadImages) {
    /*
     * even though .ico files are used as favicons and not as regular images, we enable/disable
     * them along with regular image formats
     */
    rx += separator + "svg|ico|gif|png|jpe?g";
  }
  if (rx.length > 0) {
    disabledResourcesPattern = new RegExp(rx);
  }

  // 3) Start the [first] run
  // ------------------------

  logger.log(options.LL_VERBOSE, "Warmer/profiler concurrency:", numPages, "page requests");
  startAccessPass(options.values.profileRuns > 0 && options.values.warmUpPass !== false);
};
