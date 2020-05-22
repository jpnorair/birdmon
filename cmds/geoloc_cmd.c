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

// Local Headers
#include "cmds.h"
#include "cmdutils.h"

///@todo change to hbutils/debug.h
#include "debug.h"

#include "cliopt.h"
#include "cmds.h"
#include "dterm.h"
#include "birdmon_app.h"
#include "birdmon_cfg.h"
#include "utils.h"
//#include "test.h"

#include <argtable3.h>
#include <bintex.h>
#include <cJSON.h>
#include <otvar.h>

// Standard C & POSIX Libraries
#include <stdint.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

// Libcurl for hitting HTTP
#include <curl/curl.h>
#include <math.h>





int cmd_geoloc(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    char** argv;
    int argc;
    int rc = 0;
    birdmon_app_t* appdata;
    
    if (dth == NULL) {
        return 0;
    }
    
    INPUT_SANITIZE();

    appdata = dth->ext;
    argc = cmdutils_parsestring(dth->tctx, &argv, "geoloc", (char*)src, (size_t)*inbytes);
    if (argc <= 0) {
        rc = -256 + argc;
    }
    else {
        struct arg_lit* echo    = arg_lit0("e","echo",
                                            "echo results of geolocation operation");
        struct arg_str* mode    = arg_str0("m","mode","geo|postal|wlan|ip", 
                                            "Mode to use for location info. Default: geo");
        struct arg_str* loc     = arg_str1(NULL,NULL,"location info",
                                            "geo:lat/lon, postal:\"mailing address\", wlan:if, ip:ipaddr");
        struct arg_end* end     = arg_end(4);
        void* argtable[]        = { echo, mode, loc, end };
        int mode_type;
        double lat = NAN;
        double lon = NAN;

        ///@todo wrap this routine into cmdutils subroutine
        if (arg_nullcheck(argtable) != 0) {
            rc = -1;
            goto cmd_geoloc_TERM;
        }
        if ((argc <= 1) || (arg_parse(argc, argv, argtable) > 0)) {
            arg_print_errors(stderr, end, argv[0]);
            rc = -2;
            goto cmd_geoloc_TERM;
        }

        /// Get mode_type
        /// 0: geo mode
        /// 1: postal mode
        /// 2: wlan mode
        /// 3: ip mode
        if (mode->count <= 0)                           mode_type = 0;
        else if (strcmp(mode->sval[0], "geo") == 0)     mode_type = 0;
        else if (strcmp(mode->sval[0], "postal") == 0)  mode_type = 1;
        else if (strcmp(mode->sval[0], "wlan") == 0)    mode_type = 2;
        else if (strcmp(mode->sval[0], "ip") == 0)      mode_type = 3;
        else {
            rc = -3;
            sprintf((char*)dst, "mode type \"%s\" not supported", mode->sval[0]);
            goto cmd_geoloc_TERM;
        }
        
        /// Get location based on mode type
        switch (mode_type) {
        
        // Geolocation (lat / lon)
        ///@todo Error code specification is now just -512
        case 0: {
            char* latstr;
            char* lonstr;
            char* lasts;
            
            latstr = strtok_r((char*)loc->sval[0], "/:;,", &lasts);
            if (latstr == NULL) {
                rc = -512 -4;
                sprintf((char*)dst, "latitude not supplied per --mode=geo");
                goto cmd_geoloc_TERM;
            }
            
            lonstr = strtok_r(NULL, "/:;,", &lasts);
            if (lonstr == NULL) {
                rc = -512 -5;
                sprintf((char*)dst, "longitude not supplied per --mode=geo");
                goto cmd_geoloc_TERM;
            }
            
            lat = strtod(latstr, &lasts);
            if ((lat == 0.) && (lasts == latstr)) {
                rc = -512 -6;
                sprintf((char*)dst, "latitude not a number per --mode=geo");
                goto cmd_geoloc_TERM;
            }
            
            lon = strtod(lonstr, &lasts);
            if ((lat == 0.) && (lasts == lonstr)) {
                rc = -512 -7;
                sprintf((char*)dst, "longitude not a number per --mode=geo");
                goto cmd_geoloc_TERM;
            }
        } break;
            
        // Postal Address: just a string that gets checked against google maps
        // Return value is JSON that gets parsed and evaluated.
        case 1: {
            char gmaps_qstr[1024];
            uint8_t filebuf[4096];
            size_t filebuf_max = sizeof(filebuf);
            const char* gmaps_key;
            cJSON* top_obj;
            cJSON* obj;
            const char* walkv[] = {"results", "0", "geometry", "location"};
            
            // Build URI Querystring with API Key
            gmaps_key = otvar_get_string(dth->vardict, "gmaps_key");
            if (gmaps_key == NULL) {
                rc = -512 -8;
                sprintf((char*)dst, "Google Maps API key is not supplied");
                goto cmd_geoloc_TERM;
            }
            
            snprintf(gmaps_qstr, sizeof(gmaps_qstr), "%s%s", 
                        otvar_get_string(dth->vardict, "gmaps_uri"),
                        gmaps_key);
            
            rc = utils_downloader(gmaps_qstr, (char*)dst, filebuf, &filebuf_max, false);
            if (rc < 0) {
                rc -= 512;
                goto cmd_geoloc_TERM;
            }
            
            // Download of file succeeded.  Parse it as JSON.
            // -- "status" must be "OK"
            // -- results are in (dictionary format):
            //    ['results'][0]['geometry']['location']['lat']
            //    ['results'][0]['geometry']['location']['lng']
            DEBUG_PRINTF("Google Maps API download:\n%s\n", (const char*)filebuf);
            
            top_obj = cJSON_Parse((const char*)filebuf);
            obj     = top_obj;
            if (cJSON_IsObject(obj)) {
                obj = cJSON_GetObjectItemCaseSensitive(obj, "status");
                if (cJSON_IsString(obj) && (strcmp(obj->valuestring, "OK") == 0)) {
                    obj = cJSON_walk(top_obj, 4, walkv);
                    if (cJSON_IsObject(obj)) {
                        cJSON* data;
                        data = cJSON_GetObjectItemCaseSensitive(obj, "lat");
                        if (cJSON_IsNumber(data)) {
                            lat = data->valuedouble;
                        }
                        data = cJSON_GetObjectItemCaseSensitive(obj, "lng");
                        if (cJSON_IsNumber(data)) {
                            lon = data->valuedouble;
                        }
                    }
                }
            }
            cJSON_free(top_obj);
            
        } break;
        
        ///@todo WLAN SSID check not yet supported
        // WLAN SSIDs: Look at local WLAN SSIDs and ship to Google API
        // Return value is JSON that gets parsed and evaluated.
        case 2:
            break;
        
        ///@todo IP address check not yet supported
        case 3:
            break;
        
        default:
            break;
        }

        /// If lat and lon variables are available (not NAN), then the location
        /// resolution succeeded.
        if ((lat != NAN) && (lon != NAN)) {
            pthread_mutex_lock(&appdata->ctx->data_mutex);
            appdata->ctx->client_lat = lat;
            appdata->ctx->client_lon = lon;
            pthread_mutex_unlock(&appdata->ctx->data_mutex);
            rc = 0;
        }
        else {
            sprintf((char*)dst, "lat/lon could not be resolved");
            rc = -512 -32;
        }
        
        cmd_geoloc_TERM:
        arg_freetable(argtable, sizeof(argtable)/sizeof(argtable[0]));
    }

    cmdutils_freeargv(dth->tctx, argv);
    
    return rc;
}
