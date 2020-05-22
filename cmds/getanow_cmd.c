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
#include <search.h>
#include <stdint.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

// Libcurl for hitting HTTP
#include <curl/curl.h>
#include <math.h>




/* Map struct ubx_header to a human-readable string. */
typedef struct {
    const ubx_header_t head;
    const char * const symbol;
} ubx_header_map_t;



/* Comparison function for lfind(3) */
static int straw_cmp(const void *x, const void *y) {
    const ubx_header_map_t* m1 = x;
    const ubx_header_map_t* m2 = y;
    return memcmp(&m1->head, &m2->head, sizeof(m1->head));
}



/* Get a human-readable name of a UBX packet considering its header values.
 *
 * Upon success, return a pointer to a string with UBX packet name, or
 * "mga-unknown" otherwise. */
const char *ubx_mga_symbol(ubx_header_t* h) {
    static const ubx_header_map_t haystack[] = {
        {{0xB5, 0x62, 0x13, 0x40, 24}, "mga-ini-time"},
        {{0xB5, 0x62, 0x13, 0x40, 20}, "mga-ini-pos"},
        {{0xB5, 0x62, 0x13, 0x40, 72}, "mga-ini-eop"},

        {{0xB5, 0x62, 0x13, 0x00, 68}, "mga-gps-eph"},
        {{0xB5, 0x62, 0x13, 0x00, 36}, "mga-gps-alm"},
        {{0xB5, 0x62, 0x13, 0x00, 40}, "mga-gps-health"},
        {{0xB5, 0x62, 0x13, 0x00, 20}, "mga-gps-utc"},
        {{0xB5, 0x62, 0x13, 0x00, 16}, "mga-gps-iono"},

        {{0xB5, 0x62, 0x13, 0x02, 76}, "mga-gal-eph"},
        {{0xB5, 0x62, 0x13, 0x02, 32}, "mga-gal-alm"},
        {{0xB5, 0x62, 0x13, 0x02, 12}, "mga-gal-timeoffset"},
        {{0xB5, 0x62, 0x13, 0x02, 20}, "mga-gal-utc"},

        {{0xB5, 0x62, 0x13, 0x03, 88}, "mga-bds-eph"},
        {{0xB5, 0x62, 0x13, 0x03, 40}, "mga-bds-alm"},
        {{0xB5, 0x62, 0x13, 0x03, 68}, "mga-bds-health"},
        {{0xB5, 0x62, 0x13, 0x03, 16}, "mga-bds-iono"},
        {{0xB5, 0x62, 0x13, 0x03, 20}, "mga-bds-utc"},
        
        {{0xB5, 0x62, 0x13, 0x05, 68}, "mga-qzss-eph"},
        {{0xB5, 0x62, 0x13, 0x05, 36}, "mga-qzss-alm"},
        {{0xB5, 0x62, 0x13, 0x05, 12}, "mga-qzss-health"},
        
        {{0xB5, 0x62, 0x13, 0x06, 48}, "mga-glo-eph"},
        {{0xB5, 0x62, 0x13, 0x06, 36}, "mga-glo-alm"},
        {{0xB5, 0x62, 0x13, 0x06, 20}, "mga-glo-timeoffset"},
    };
    const ubx_header_map_t needle = {*h, NULL};
    const ubx_header_map_t* found;
    size_t strawsz;
    size_t haystacksz;

    strawsz     = sizeof(haystack[0]);
    haystacksz  = sizeof(haystack) / sizeof(haystack[0]);
    found       = lfind(&needle, haystack, &haystacksz, strawsz, straw_cmp);

    return found ? found->symbol : "mga-unknown";
}



int cmd_getanow(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
    char** argv;
    int argc;
    int rc = 0;
    birdmon_app_t* appdata;
    
    if (dth == NULL) {
        return 0;
    }
    
    INPUT_SANITIZE();

    appdata = dth->ext;
    argc = cmdutils_parsestring(dth->tctx, &argv, "getanow", (char*)src, (size_t)*inbytes);
    if (argc <= 0) {
        rc = -256 + argc;
    }
    else {
        struct arg_int* age = arg_int0("a","age","seconds", 
                                            "Maximum allowable age of returned AGNSS data (default = infinity)");
        struct arg_end* end = arg_end(4);
        void* argtable[]    = { age, end };
        assistnow_t* anow;
        char* dcurs;

        ///@todo wrap this routine into cmdutils subroutine
        if (arg_nullcheck(argtable) != 0) {
            rc = -1;
            goto cmd_getanow_TERM;
        }
        if ((argc < 1) || (arg_parse(argc, argv, argtable) > 0)) {
            arg_print_errors(stderr, end, argv[0]);
            rc = -2;
            goto cmd_getanow_TERM;
        }

        // If a max-age parameter is provided, we need to make sure the agnss
        // file is acceptably fresh.  This is done by signalling the backend,
        // and then waiting for it to release the data.
        if (age->count > 0) {
            time_t now;
            time_t delta_s;
            
            now     = time(NULL);
            pthread_mutex_lock(&appdata->ctx->data_mutex);
            delta_s = appdata->ctx->assistnow.timestamp;
            pthread_mutex_unlock(&appdata->ctx->data_mutex);
            delta_s = (delta_s < now) ? (now - delta_s) : 0;
            
            if (delta_s > (time_t)age->ival[0]) {
                pthread_mutex_lock(&appdata->ctx->cronmutex);
                appdata->ctx->croncond_pred = false;
                pthread_cond_signal(&appdata->ctx->croncond);
                pthread_mutex_unlock(&appdata->ctx->cronmutex);
                
                pthread_mutex_lock(&appdata->ctx->readymutex);
                appdata->ctx->readycond_cnt++;
                while (appdata->ctx->readycond_cnt > 0) {
                    pthread_cond_wait(&appdata->ctx->readycond, &appdata->ctx->readymutex);
                }
                pthread_mutex_unlock(&appdata->ctx->readymutex);
            }
        }
        
        /// Process the data for the client.  Right now, the output format is
        /// controlled by the "fmt" command line option, but potentially in the
        /// future we could override this with specific flags to this command.
        pthread_mutex_lock(&appdata->ctx->data_mutex);
        
        // Print the front material
        anow    = &appdata->ctx->assistnow;
        dcurs   = (char*)dst;
        switch (cliopt_getformat()) {
            default: break;
            
            // At present, JSON and JSONHex treatment is the same, because 
            // a full-featured ubx data parser isn't implemented yet.
            case FORMAT_Json:
            case FORMAT_JsonHex: 
                dcurs += snprintf(dcurs, dstmax, 
                            "{\"ubx_anow\":{\"timestamp\":%lu, \"pkt\":[", 
                            anow->timestamp);
                break;
            
            // For hexdump, it all happens here
            case FORMAT_Hex: {
                uint8_t* scurs  = anow->buf;
                size_t accum    = 0;
                fmt_printhex((uint8_t*)dcurs, &accum, &scurs, anow->bufsize, 0);
                dcurs += accum;
                strcpy(dcurs, "\n");
                dcurs++;
            } goto cmd_getanow_END;
        }
        
        // Print each packet
        for (int i=0; i<anow->numpkts; i++) {
            const char* pkt_type    = ubx_mga_symbol(&anow->pkt[i].hdr);
            uint8_t* scurs          = anow->pkt[i].data;
            size_t accum            = 0;
            int loadlen             = (int)((uint8_t*)dcurs-dst);
            
            if (loadlen > dstmax) {
                break;
            }
            dstmax -= loadlen;
        
            switch (cliopt_getformat()) {
                default:
                case FORMAT_Default: {
                    dcurs += snprintf(dcurs, dstmax, "ubx %s\n", pkt_type);
                    fmt_printhex((uint8_t*)dcurs, &accum, &scurs, anow->pkt[i].len, 16);
                    dcurs += accum;
                } break;
                
                // At present, JSON and JSONHex treatment is the same, because 
                // a full-featured ubx data parser isn't implemented yet.
                case FORMAT_Json:
                case FORMAT_JsonHex: {
                    dcurs += snprintf(dcurs, dstmax, 
                                    "{\"type\":\"%s\", \"size\":%u, \"dat\":",
                                    pkt_type, (unsigned int)anow->pkt[i].len);
                    fmt_printhex((uint8_t*)dcurs, &accum, &scurs, anow->pkt[i].len, 0);
                    dcurs += accum;
                    strcpy(dcurs, "},");
                    dcurs += 2;
                } break;
                
                case FORMAT_Bintex: {
                    dcurs += snprintf(dcurs, dstmax, "\"ubx %s\"", pkt_type);
                    fmt_printhex((uint8_t*)dcurs, &accum, &scurs, anow->pkt[i].len, 0);
                    dcurs += accum;
                    strcpy(dcurs, "\n");
                    dcurs++;
                } break;
                
                case FORMAT_Hex: {
                    
                } break;
            }
        }
        
        // Print the back material
        switch (cliopt_getformat()) {
            default: break;
            case FORMAT_Json:
            case FORMAT_JsonHex: 
                dcurs--;    // eat last comma
                strcpy(dcurs, "]}}\n");
                dcurs += 4;
                break;
        }
        
        rc = (int)((uint8_t*)dcurs - dst);
        
        cmd_getanow_END:
        pthread_mutex_unlock(&appdata->ctx->data_mutex);
        
        cmd_getanow_TERM:
        arg_freetable(argtable, sizeof(argtable)/sizeof(argtable[0]));
    }

    cmdutils_freeargv(dth->tctx, argv);
    
    return rc;
}
