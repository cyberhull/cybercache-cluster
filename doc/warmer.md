
CyberCache Cluster Cache Warmer / Profiler
==========================================

This utility combines cache warmer and performance measurement tools; it is
available in the Enterprise Edition of the CyberCache Cluster only.

> **NOTE**: even though the list of options is quite extensive, none of them
> are mandatory, and basic usage is actually very simple; you may want to refer
> to the `Getting Started` section of this manual first, and only return to
> options' descriptions if you want to fine-tune CyberCache warmer in any way.

Usage:

    cybercachewarmer [ <option> [ <option> [...]]] <url-file> [ <ulr-file> [...]]

where <url-file> is either a site map XML file exported from Magento 2, or a
custom file with URLs (see format description below), and valid options are:

* `-p[:<num-runs>]` or `--profile[=<num-runs>]`

  Runs warmer in profiling (cache performance measurement) mode instead of the
  default cache warming mode; does <num-runs> of runs over specified URL set;
  the argument is optional and, if omitted, is assumed to be `1`.

* `-q:<num-requests>` or `--queue=<num-requests>`

  Sets number of concurrent *page* requests that will be used to load site
  pages; each page request can create further concurrent requests to load page
  resources. Must be within 1..256 range; default is 16.

* `-b:<number>` or `--begin=<number>`

  Skips first `<number>` of URLs in specified set; default is 0. If this option
  is specified more than once, second one overwrites the first.

* `-n:<number>` or `--number=<number>`

  Uses only up to `<number>` of URLs from specified set; zero <number> is
  treated as "use all"; default is 0. If this option is specified more than
  once, second one overwrites the first.

* `-o:<timestamp>` or `--older-than=<timestamp>`

  Only loads pages modified after specified timestamp. The `<timestamp>` must
  be either in `YYYY-MM-DD HH:MM:SS`, or in `<number>{s|m|h|d|w}` format; the
  latter stands for "<number> of seconds/minutes/hours/days/weeks ago". If this
  option is specified more than once, the very last one takes precedence. To
  avoid quoting timestamp, underscore ('_') can be used instead of space as
  date/time separator (i.e. timestamp can be specified as, for instance,
  `-o:2019:09:08_15:17:00`); note that space can be used as separator *without*
  quoting if the option is put into option/response file (see below). The
  timestamp should represent **local** time of the computer running CyberCache
  warmer/profiler.

* `-r:<lowest-priority>,<highest-priority>` or `--range=<lowest-priority>,<highest-priority>`

  Only load pages with priorities within specified range; the range is
  inclusive and defaults to `0.0..1.0`.

* `-s:<order>` or `--sort=<order>`

  Sorts loaded URLs before optionally selecting a subset, and starting to
  access them; valid sort orders are `low-priority-first`,
  `high-priority-first` (default in cache  warming mode), and `as-is` (default
  in profiling mode).

* `-a:<user-agent>` or `--agent=<user-agent>`

  Emulates specified user agent; possible user agents are `unknown`, `bot`,
  `warmer` (default in cache warming mode), and `user` (default in performance
  measurement mode).

* `-w:<timestamp>` or `--warm-up=<timestamp>`

  Tells profiler to do an extra non-timed warm-up run before actual profiling
  runs (in addition to the number or runs specified using `-p` option); ignored
  if `-p` is not specified. The `<timestamp>` argument has the same format as
  that of `-o`/`--older-than` option (see `Profiling The Site` section for
  format info); it makes profiler wait specified number of seconds/minutes/etc.
  after warm-up before doing actual profiler runs or, if exact timestamp is
  specified (which would represent **local** time on the computer running
  CyberCache profiler), to start profiling (after warming up) at that time.
  Also, see notes in `Profiling The Site` section (below).

* `-u:<priority>,<url>` or `--url=<priority>,<url>`

  Adds specified URL to the list of URLs that will be accessed; the
  `<priority>` argument is a floating point number in range `0.0..1.0`,
  inclusive. If the `<url>` does not begin with an `http://` or `https://`
  protocol prefix, `http://` will be used by default.

* `-i[:<boolean>]` or `--images[=<boolean>]`

  Tells warmer/profiler whether to load images and favicons; optional boolean
  can be `yes`, `no`, `true`, `false`, `on`, or `off`. Default boolean value
  (if `-i`/`--images` is specified without an argument) is `true`. If this
  option is not specified at all, the images and favicons will *not* be loaded.

* `-c[:<boolean>]` or `--css[=<boolean>]`

  Tells warmer/profiler whether to load style sheets and web fonts; optional
  boolean can be `yes`, `no`, `true`, `false`, `on`, or `off`. Default boolean
  value (if `-c`/`--css` is specified without an argument) is `true`. If this
  option is not specified at all, CSS files and web fonts will *not* be loaded.

* `-j[:<boolean>]` or `--javascript[=<boolean>]`

  Tells warmer/profiler whether to load and execute JavaScript (both external
  `.js` files and JavaScript embedded in the page); optional boolean can be
  `yes`, `no`, `true`, `false`, `on`, or `off`. Default boolean value (if
  `-j`/`--javascript` is specified without an argument) is `true`. If this
  option is not specified at all, JavaScript files *will* be loaded and
  executed (NOTE: this differs from the `-i`/`--images` and `-c`/`--css`
  options' defaults).

* `-l:<level>` or `--log-level=<level>`

  Sets log level (verbosity); recognized log levels are `none`, `error`,
  `warning`, `terse`, `normal` (the default), and `verbose`. Setting level to
  `none` disables all logging (even if `-f`/`--log-file` option is specified)
  and console output (except for error/warning messages).

* `-f:<log-file-path>` or `--log-file=<log-file-path>`

  Sets the name of the log file; default is `cybercache_warmer.log` in the
  current directory; if the file already exists, new messages will be appended
  to it. If the file cannot be opened or created, the warmer will print out
  error message and exit with code 2 ("command line processing error", see
  error codes below) to the shell.

* `-d:<mode>` or `--display=<mode>`

  Sets display mode; valid modes are `regular` (duplicates logged messages to
  the console; the default), `total-time` (prints *only* number of milliseconds
  it took to access all URLs; any other output is suppressed), `page-time`
  (like `total-time`, but prints only *average* number of milliseconds it took
  to load *one* page), and `none`.

* `-e:<reason>` or `--exit-if=<reason>`

  Sets error tolerance; valid exit reasons are `user-interrupt` (default in
  cache warming mode), `recorded-errors`, `missing-page` (default in profiling
  mode), and `missing-resource`. The reasons on this list are arranged from
  "least strict" to "most strict" (reasons appearing later on the list **do**
  include all reasons that go before them). The `recorded-errors` would cause
  warmer/profiler to complete all runs even if there are errors, but then
  return non-zero exit code to the shell. If a page cannot be loaded but this
  option's argument is not `missing-page`, then warning message will be logged.
  If a resource cannot be loaded but this option's argument is not
  `missing-resource`, then warning message will *only* be logged if
  `recordded-errors` was specified as argument.

* `-x:<num>,<width>,<height>,<path>` or `--export=<num>,<width>,<height>,<path>`

  Exports loaded page or pages to image file(s). `<num>` defines number of
  *last* loaded pages to save, and can also be specified as `all`. In profiling
  mode, the only valid `<num>` argument is `1`: it would make profiler take a
  screenshot *after* it loaded the very last page *and* stopped the timer. The
  `<width>` and `<height>` arguments set window size for page rendering, and
  either of them (or both) can be omitted (as in `-x:1,,,screenshot.png`); the
  default window size is 1024 x 768, the smallest allowed window size is
  120 x 90; if `<width>` is specified but `<height>` is omitted (or vice
  versa), the other dimension is deduced in a way that maintains 4:3 aspect
  ratio. The `<path>` specifies name of the file *and* file format that will be
  used; the format is deduced from the extension, with valid extensions being
  `.pdf`, `.png`, `.jpg`, `.jpeg`, `.bmp`, `.ppm`, and `.gif` (the latter may
  or may not be supported by the system). If `<path>` contains directory
  component, that directory will be created if it does not exist. If *file
  name* component of the `<path>` contains '@' character, that character will
  be replaced with page component of the URL upon saving. If `<num>` is greater
  than `1` but '@' placeholder is not used (or URLs happen to have same name
  components), existing files will be silently overwritten.

* `-t:<milliseconds>` or `--timeout=<milliseconds>`

  Sets timeout before page is exported to an image file. If all images on site
  pages are defined using static `<img>` tags, use of this option is not
  necessary: cache warmer can itself detect when all images are loaded (in such
  a case, it might make sense to even set `<timeout>` to just `1` millisecond).
  If, however, images are loaded using elaborate JavaScript/AJAX calls (a
  `<div>` is loaded using AJAX, then dynamically modified to first insert a
  "spinner" placeholder, then an actual image etc.), the warmer has no way to
  detect that image loading it truly complete, in which case use of
  `-t`/`--timeout` can help. Default timeout is 1000 milliseconds. This option
  has no effect during profiling runs.

* `-v` or `--version`

  Prints out version number and exits.

* `-h` or `--help`

  Prints out help information and exits.

* `@<option-file>`

  Loads and parses specified option (a.k.a. response) file. Option files may
  contain **everything** that can be specified on the command line: one option,
  option/response file, or a URL source per line. Empty lines and lines with
  first non-whitespace character being '#' are ignored (the latter are treated
  as comments). Option files can be intermixed with options, but should go
  before first URL source or `--` mark (see below), both on command line and in
  containing option files.

* `--`

  Marks end of options on the command line *or* in option (response) file; all
  subsequent arguments wil be treated as URL sources (i.e. XML site map or
  custom URL file names), even if they start with '-' or '@' characters. This
  marker is only required if it is necessary to specify a URL source file path
  starting with '-'. If `--` is put into an option file, then it affects *only*
  the rest of that file, and has no effect on nested option files, or the rest
  of the command line.

Getting Started
---------------

Basic usage is very simple. To warm up CyberCache server, use

    cybercachewarmer <url-file>

Or use

    cybercachewarmer -p <url-file>

to profile the server. The `<url-file>` is either a site map XML file exported
from Magento 2, or a custom file with URLs.

This is it. All options will be set automatically to feasible default values
*for selected mode* ("warming" or "profiling").

### Magento 2 Site Map

To generate Magento 2 site map XML file for use with CyberCache warmer/profiler,
please do the following:

1. Go to Magento 2 site admin panel.

2. Select `Stores -> Configuration -> Catalog -> XML Sitemap`.

3. Review "Priority" fields of all "Options" (see `Cache Warming` subsection of
   this manual, below, for information on how priorities affect cache warming).

4. Optionally review "Frequency" fields of all options.

   > NOTE: the "Frequency" setting does not directly affect CyberCache warmer
   > operation. However, you do need to check it if you plan to run warmer on a
   > regular basis (say, if catalog gets automatically updated daily during
   > night time, you may want to run warmer immediately after that).

5. Set "Sitemap File Limits" so that all URLs would fit one file.

6. Select `Marketing -> Site Map`.

7. For each store view of the site:

   * Select store view from the drop-down list.
   * Set name (and, optionally, path) of the XML file to be generated.
   * Click `Generate` link.

### Custom URL Files

You can specify custom URL file to CyberCache warmer instead of the XML file
exported from Magento 2. The custom file is a simple text file, having any
extension other than ".xml", with each line being either blank, or having hash
mark as first non-blank character ('#'; would be treated as comment), or have
the following format:

    <priority> <url>

where `<priority>` is a floating point number in range `0.0` to `1.0`,
inclusive (it has the same meaning as `<priority>` field in Magento XML files),
and `<url>` is a string representing URL. For instance:

    # this is a sample URL file
    0.75 http://www.magento-luma.com/index.php/fusion-backpack.html
    0.50 http://www.magento-luma.com/index.php/cruise-dual-analog-watch.html

Custom URL files can be created in great many ways (say, by crawlers), or from
great many sources (for instance, converted from Magento XML site maps).

How Does CyberCache Warmer Work
-------------------------------

> NOTE: to "just use" CyberCache warmer, you do **NOT** need to know anything
> of what follows. This information is only intended for those who want to
> fine-tune CyberCache (and thus their Magento 2 site) to the maximum, and
> squeeze every last cycle out of it.

CyberCache warmer/profiler works as follows:

1. Processes options (specified directly, or in option/response files).

   > NOTE: various options that control both cache warming and profiling are
   > set to **different** suitable default values automatically -- depending
   > upon whether or not option `-p`/`--profile` was specified.

2. Loads URLs from all the Magento site maps and custom URL files in order
   those files were specified; individual URLs added using `-u`/`--url` option
   are added to the list in order they were encountered, as if instead of the
   option a custom URL file was specified containing a single URL. If option
   `-o`/`--older-than` was set, then, *while loading* URLs from XML site maps,
   skips those that have earlier timestamps. Likewise, if option `-r`/`--range`
   was set, skips URLs with priorities not within specified range (so `-r`
   applies to everything: Magento XML file, custom URL files, and `-u`/`--url`
   options, while `-o` -- only to Magento XML files, because only the latter
   have "last modification time" information associated with URLs).
   
   > NOTE: Magento site map XML files specify dates in ISO 8601 format, with
   > time zone offset usually being `+00:00`, which essentially means that
   > Magento "last modification dates" represent UTC time. On the other hand,
   > `-o`/`--older-than` option takes timestamp that represents **local** time
   > on the computer running CyberCache warmer/profiler, and does all necessary
   > conversions internally.

3. If sorting order (`-s`/`--sort` option) is not `as-is`, sorts loaded URLs
   according to that sorting order.

4. If `-b`/`--begin` and/or `-n`/`--number` options were specified, applies
   them to select a subset of URLs to access. At this point, the warmer is
   ready to start "hitting" the site.

   > NOTE: the "from"/"number" subset of the URLs is selected **after** they
   > are filtered (by last modification date and "priority") *and* sorted (by
   > their "priorities").

5. Initializes request queue (according to `-q`/`--queue` options).

At this point, CyberCache warmer/profiler is ready to start "hitting" the site.
What happens next depends upon selected mode (warm up / profile).

### Cache Warming

In this mode, all selected URLs will be hit only once.

Default sort order in this mode is `high-priority-first`. Given that default
priorities that Magento 2 assigns to page categories (provided that you did not
change them on step #3 of the site map generation procedure described in the
`Magento 2 Site Map` section, above) are

* Categories: 0.5
* Products: 1.0
* CMS Pages: 0.25

this means that product pages will be "hit" first, and CMS pages will be hit
last; which, in turn, means that cached data associated with CMS pages will be
"warmer" in CyberCache server's internal store, and cached data associated with
product pages will bee "cooler". Why is this important? Because CyberCache
server employs true LRU eviction strategies: when it runs out of memory, it
purges "cooler" (not accessed for longer time period) data first.

If it is necessary to fine-tune cached data availability, it is possible to do
any or all of the following:

* Adjust relative categories/products/CMS pages priorities in admin panel and
  re-generate site map XML file.

* Specify different argument to the `-s`/`--sort` option.

* Convert some of the records in the site map XML file ("most important" ones)
  into records in custom URL file that is specified after the XML map (from
  which URLs were extracted); "priorities" of those records should be set to
  something like `0.1` to make sure they're accessed last. Note that you do
  *not* need to delete those URLs from the XML map file(s): newly found URLs
  silently overwrite earlier instances.

Just like priorities, the `-a`/`--agent` option also indirectly affects server
cache records' lifetimes: when CyberCache server runs out of memory, it first
purges records created by unknown users, then (if there's still not enough
space) records created by bots, then records created by its own warmer, and
only then records created by regular users.

Default user agent in cache warming mode is `warmer`. If it is desirable to
give some URLs a "boost" (i.e. increase the likelihood of their survival during
server cache purges), then they should be processed with user agent set to
`user`; in such a case, CyberCache warmer should be run twice, and it is
possible to do so with the *same* XML/configuration file [set]: by filtering
out required URLs for each run, with most convenient filtering facility being
the `-r`/`--range` option, which allows to, say, easily separate products from
categories based on the priorities assigned to them in Magento admin panel.

### Profiling The Site

In profiling mode, each page will be "hit" the number of times specified by the
argument to the `-p`/`--profile` option, plus one more time if option `-w`/
`--warm-up` was specified. That is, profiler works as follows:

* If `-p`/`--profile` option is specified:

   * If option `-w`/`--warm-up` is specified
      * Load every specified URL once but discard load time information
      * Wait [until] the time given as argument to the `-w`/`--warm-up` option

   * Repeat number of times specified as argument to the `-p`/`--profile` option:
      * Load every specified URL once and accumulate load time information

   * Divide accumulated time by number of page loads, and display the result

The number of pages to save as images (using `-x`/`--export`) option cannot be
set to anything but `1`, because it would interfere with time measurements (the
only screenshot, if requested, will be taken after profiler finished its runs
*and* stopped the timer). The `-t`/`--timeout` option is ignored.

It is *not* recommended to exclude any pages from profiling runs (using options
like `-r`/`--range`, `-o`/`--older-than` etc.): the higher the number of
*different* accessed site pages, the more accurate profiling data will be.

The use of the `-w`/`--warm-up` option in profiling mode is, contrary,
recommended; it's role is two-fold: to pre-populate the cache, and (if
`<timeout>` argument is specified) to let CyberCache server have some time to
optimize its data.

> When Magento sends data to the CyberCache server through CyberCache PHP
> extension, the latter is usually configured to employ very fast compressors,
> which do not yield particularly good comression ratios. So CyberCache server
> runs several so-called optimization threads in background to improve
> compression: those threads take stored records, one by one, and re-compress
> them using much stronger compressors (specified in server configuration
> files). This process takes some time; how much exactly can be seen using
> instrumented version of the server. In real-world scenatious, all data
> optimization will be completed long before work hours; so it makes sense to
> specify `<timeout>` argument to the `-w`/`--warm-up` option to be big enough
> to let those optimizations finish before actual profiling runs.

Even if the argument to the option `-w`/`--warm-up` represents exact timestamp,
the profiler will still convert it to time offset (between the moment the
option was parsed, and target time to start profiling). What this means is that
if exact timestamp `t` was given to `-w`/`--warm-up`, and warming up took, say,
`m` minutes, profiling will start at `t + m`.

Please note that there are actually two types of profiling.

* Default settings of the CyberCache profiler options are meant to be used to
  profile CyberCache server, and figure out relative efficiency of the settings
  that are currently in use. Because of that, loading of CSS and image files is
  turned Off by default: CyberCache does not handle them in any way, so adding
  their load times does not add anything to the overall picture. JavaScript
  loading and execution *is* On, however, because there could be AJAX requests
  that would result in extra CyberCache server accesses. If you're sure that
  your site's pages to not employ AJAX, you may want to turn off JavaScript
  loading as well, using `-j`/`--javascript` profiler option.

* If, *after* profiling and fine-tuning CyberCache server, you want to assess
  performance of your *site* (as opposed to the performance of the CyberCache
  server), you should turn On loading of CSS files and images (using `-c`/
  `--css` and `-i`/`--images` options), and run profiler again; in this case,
  doing just one run should suffice. Be wary, however, of the differences
  between development/stage and production servers' setups: the latter might
  have all static resources (including images) located on CDN, while the former
  might not; there could be other noteworthy differences (even though the best
  practice is to set up stage server exactly as production). If that's the
  case, the only way to assess site performance would be to profile production
  server outside of work hours.

Automated Testing
-----------------

CyberCache warmer/profiler has some features that can be used to facilitate
automated testing.

### Checking Site Integrity

If option `-e`/`--exit-if` is set to `missing-page` or `missing-resource`,
CyberCache warmer/profiler will exit with error code 4 or 5 if it cannot load a
page or a resource, respectively.

Alternatively, one can specify `recorded-errors` argument to the option and
have the warmer/profiler to complete all its runs even if there were errors
(missing pages, or resources, or both), but then quit to the shell with exit
code 3; all the errors would be written to the log file.

### Monitoring Site Response Times

When used with `page-time` or `total-time` arguments, the `-d`/`--display`
option does two things:

* suppresses all output to the standard console output stream,

* tells warmer/prifiler to output the time, in milliseconds, it took to access
  each (on average) or all pages in the specified URL set.

The time is printed as an integer, and can be obtained in cron jobs and other
scripts, for example, as follows:

    PER_PAGE_TIME=$(cybercachewarmer -d:page-time my_site_map.xml)

The value can then be examined by the shell script, and the latter may alert
site administrator (say, by an email) if it's not within acceptable limits.

### Exit Codes

CyberCache warmer/profiler exits to the shell with one of the following codes:

* **0** : Processing was completed successfully. No errors (that the warmer/
  profiler *was told to report*) has occurred.

* **1** : Nothing to do. Either `-v`/`--version` or `-h`/`--help` option was
  specified, or there were no URLs in specified XML/URL files, or all supplied
  URLs were excluded after processing `-n`/`--number`, `-r`/`--range` and/or
  other filtering options.

* **2** : There was an error processing command line arguments: an option was
  specified using wrong format, or an option/log/map/custom URL file could not
  be opened.

* **3** : Warmer/profiler has completed processing of all specified URLs, but
  some pages and/or resources were missing. This exit code is only returned if
  `-e`/`--exit-if` option was specified with `recorded-errors` argument.

* **4** : Warmer/profiler has aborted processing because it could not download
  a page. This exit code is only returned if `-e`/`--exit-if` option was
  specified with a `missing-page` *or* `missing-resource` argument.

* **5** : Warmer/profiler has aborted processing because it could not download
  a resource. This exit code is only returned if `-e`/`--exit-if` option was
  specified with `missing-resource` argument.

If warmer/profiler is told to print execution time (using `-d`/`--display`
option), it will do so even if errors occurred, and exit code is not zero (in
case of exit code `1`, reported time will always be zero). If there is an error
in option format, warmer/profiler will only print execution time if
`-d`/`--display` option had been parsed already.
