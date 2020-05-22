/* Copyright 2014, JP Norair
  *
  * Licensed under the OpenTag License, Version 1.0 (the "License");
  * you may not use this file except in compliance with the License.
  * You may obtain a copy of the License at
  *
  * http://www.indigresso.com/wiki/doku.php?id=opentag:license_1_0
  *
  * Unless required by applicable law or agreed to in writing, software
  * distributed under the License is distributed on an "AS IS" BASIS,
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  */

#ifndef cmds_h
#define cmds_h

// Local Headers
#include "dterm.h"

// POSIX & Standard C Libraries
#include <stdint.h>
#include <stdio.h>




#define CMD_NAMESIZE    8
#define CMD_COUNT       15

// arg1: dst buffer
// arg2: src buffer
// arg3: dst buffer max size
typedef int (*cmdaction_t)(dterm_handle_t*, uint8_t*, int*, uint8_t*, size_t);



/// Commands for generic local control
int cmd_quit(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax);
int cmd_cmdlist(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax);
int cmd_raw(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax);

/// Set/get an environment variable.  sethome is deprecated.
int cmd_var(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax);


int cmd_geoloc(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax);
int cmd_getanow(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax);
int cmd_schedule(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax);



/// Application protocol commands sent over the MPipe
///@todo These could probably be moved into m2def or something else.

// ID = 0
int app_null(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax);








#endif
