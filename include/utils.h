/* Copyright 2017, JP Norair
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


#ifndef birdmon_utils_h
#define birdmon_utils_h

#include <cJSON.h>

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

///@todo move this into cJSON
cJSON* cJSON_walk(cJSON* obj, int argc, const char* argv[]);


/** @brief Download a URL via HTTP
  * @param url      (const char*) url to download
  * @param errbuf   (char*) buffer to write error messages into.  Can be NULL.
  * @param filebuf  (char*) buffer to write downloaded file into. Cannot be NULL.
  * @param bufmax   (size_t*) maximum size of the file buffer.  May be updated if reallocation happens.
  * @param buf_isdynamic (bool) buffer is dynamically allocated (and may be reallocated)
  * @retval (int)   Negative on error, else number of bytes downloaded in file
  */
int utils_downloader(const char* url, char* errbuf, uint8_t* filebuf, size_t* bufmax, bool buf_isdynamic);

#endif
