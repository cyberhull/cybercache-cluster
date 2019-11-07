<?php
/*
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

if (extension_loaded('cybercache')) {
  echo "CyberCache extension loaded successfully.\n";
} else {
  echo "CyberCache extension did NOT load!";
  exit(1);
}

$res1 = c3_session(["hasher" => "xxhash", "address" => "cyberhull.com"]);
var_dump($res1);
$res2 = c3_session(null);
var_dump($res2);
$res3 = c3_session();
var_dump($res3);

$message = c3_get_last_error($res1);
echo "Last error message: '$message'\n";

$caps = c3_get_capabilities();
var_dump($caps);

$functions = get_extension_funcs('cybercache');
echo "Functions exported by CyberCache extension:\n";
$index = 1;
foreach($functions as $func) {
  echo sprintf("  %02d: %s()\n", $index, $func);
  $index++;
}
