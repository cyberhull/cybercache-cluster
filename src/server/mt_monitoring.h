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
 * Multithreading support: defines that conditionally enable/disable interlock monitoring.
 */
#ifndef _MT_MONITORING_H
#define _MT_MONITORING_H

// CyberCache Cluster Lock Monitoring flag
#ifndef C3LM_ENABLED
#define C3LM_ENABLED 0
#endif

#if C3LM_ENABLED
  #define C3LM_DISABLED 0
  #define C3LM_DEF(stmt) stmt;
  #define C3LM_ON(stmt) stmt
  #define C3LM_OFF(stmt)
  #define C3LM_IF(stmt1,stmt2) stmt1
#else
  #define C3LM_DISABLED 1
  #define C3LM_DEF(stmt)
  #define C3LM_ON(stmt)
  #define C3LM_OFF(stmt) stmt
  #define C3LM_IF(stmt1,stmt2) stmt2
#endif

#endif // _MT_MONITORING_H
