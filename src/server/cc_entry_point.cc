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
 * Server entry point.
 */

#include "c3lib/c3lib.h"
#include "cc_server.h"

using namespace CyberCache;

int main(int argc, char** argv) {
  Thread::initialize_main(&server);
  if (server.configure(argc, argv)) {
    if (server.start()) {
      server.run();
    }
    server.shutdown();
  }
  server.cleanup();
  return EXIT_SUCCESS;
}
