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

// Application Includes
//#include "crc_calc_block.h"
#include "debug.h"
#include "birdmon_app.h"
#include "backend.h"
#include "formatters.h"
#include "utils.h"

// Local Libraries/Includes
#include <bintex.h>
#include <ccronexpr.h>
#include <dterm.h>
#include <hbutils/timespec.h>


#include <curl/curl.h>

// Standard C & POSIX libraries
#include <fcntl.h>
#include <pthread.h>
#include <search.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>





int backend_init(backend_handle_t* handle, const char* cronstr) {
/// Initialize the interface table based on the num_intf parameter.  If it's
/// zero then it is considered to be 1.
    CURLcode curl_rc;
    backend_ctx_t* ctx;
    int rc = -3;
    const char* cron_err;
    const char* cronstr_def = "0 0 0/1 * * *";

    if (handle == NULL) {
        return -1;
    }

    ctx = malloc(sizeof(backend_ctx_t));
    if (ctx == NULL) {
        return -2;
    }
    
    bzero(&ctx->assistnow, sizeof(assistnow_t));
    
    if (pthread_mutex_init(&ctx->data_mutex, NULL) != 0) {
        goto backend_init_ERR1;
    }
    if (pthread_cond_init(&ctx->croncond, NULL) != 0) {
        goto backend_init_ERR2;
    }
    if (pthread_mutex_init(&ctx->cronmutex, NULL) != 0) {
        goto backend_init_ERR3;
    }
    if (pthread_cond_init(&ctx->readycond, NULL) != 0) {
        goto backend_init_ERR4;
    }
    if (pthread_mutex_init(&ctx->readymutex, NULL) != 0) {
        goto backend_init_ERR5;
    }
    
    curl_rc = curl_global_init(CURL_GLOBAL_ALL);
    if (curl_rc != 0) {
        goto backend_init_ERR6;
    }
    
    bzero(&ctx->cronexp, sizeof(cron_expr));
    if (cronstr == NULL) {
        cronstr = cronstr_def;
    }
    cron_parse_expr(cronstr, &ctx->cronexp, &cron_err);
    ctx->cronbasis = 0;
    
    *handle = (backend_handle_t)ctx;
    return 0;
    
    // Error handling destructors
    backend_init_ERR6:
    pthread_mutex_destroy(&ctx->readymutex);    rc--;
    backend_init_ERR5:
    pthread_cond_destroy(&ctx->readycond);      rc--;
    backend_init_ERR4:
    pthread_mutex_destroy(&ctx->cronmutex);     rc--;
    backend_init_ERR3:
    pthread_cond_destroy(&ctx->croncond);       rc--;
    backend_init_ERR2:
    pthread_mutex_destroy(&ctx->data_mutex);    rc--;
    backend_init_ERR1:
    free(ctx);
    
    return rc;
}



void backend_deinit(backend_handle_t handle) {
    backend_ctx_t* ctx;
    
    if (handle != NULL) {
        ctx = (backend_ctx_t*)handle;
        
        curl_global_cleanup();
        
        pthread_mutex_destroy(&ctx->readymutex);
        pthread_cond_destroy(&ctx->readycond);
        pthread_mutex_destroy(&ctx->cronmutex);
        pthread_cond_destroy(&ctx->croncond);
        pthread_mutex_destroy(&ctx->data_mutex);
        
        free(ctx);
    }
}








/** Backend Threads <BR>
  * ========================================================================<BR>
  * <LI> agnss_reader() : reads AGNSS information from various HTTP servers
  *         (namely Ublox AssistNow), and stores it to local cache. </LI>
  */
typedef struct {
    ubx_header_t header;
    uint8_t* front;
    uint8_t* payload;
    uint8_t* checksum;
} ubx_packet_t;


static int sub_validate_ubx(ubx_packet_t* pkt) {
    union {
        uint8_t ubyte[2];
        uint16_t ushort;
    } fletcher;
    union {
        uint8_t ubyte[2];
        uint16_t ushort;
    } ck;
    int length;
    uint8_t* cursor;
    uint16_t output;
    

    fletcher.ubyte[0] = pkt->checksum[0];
    fletcher.ubyte[1] = pkt->checksum[1];

//HEX_DUMP(pkt->front, (pkt->header.plen+8), "packet-total: \n");
//HEX_DUMP((uint8_t*)&pkt->header, sizeof(ubx_header_t), "packet-header: \n");
//HEX_DUMP((uint8_t*)&fletcher.ubyte[0], 2, "packet-checksum: \n");
    
    ck.ushort = 0;
    cursor = &pkt->front[2];
    length = 4 + pkt->header.plen;
    
    while (length > 0) {
        length--;
        ck.ubyte[0] += *cursor++;
        ck.ubyte[1] += ck.ubyte[0];
    }

    output = fletcher.ushort - ck.ushort;

    return (int)output;
}





void* agnss_reader(void* args) {
    birdmon_app_t* appdata  = args;
    dterm_handle_t* dth;
    const char* assistnow_apikey;
    char tmpbuf[256];
    char assistnow_url[1024];
    uint8_t assistnow_buf[65536];
    size_t assistnow_bufmax = sizeof(assistnow_buf);
    
    if (args == NULL) {
        goto agnss_reader_TERM;
    }
    dth = appdata->dterm_parent;
    if (dth == NULL) {
        goto agnss_reader_TERM;
    }

    while (1) {
        struct timespec next = {.tv_nsec=0, .tv_sec=0};
        
        pthread_mutex_lock(&appdata->ctx->data_mutex);
        next.tv_sec = cron_next(&appdata->ctx->cronexp, appdata->ctx->cronbasis);
        pthread_mutex_unlock(&appdata->ctx->data_mutex);
        
        /// Error case: invalid cron, wait until a resynchronization event
        if (next.tv_sec < 0) {
            const char* logmsg = "Cron string provided to backend is invalid -- waiting for resync event.";
            dterm_send_log(dth, logmsg, strlen(logmsg));
            
            pthread_mutex_lock(&appdata->ctx->cronmutex);
            appdata->ctx->croncond_pred = true;
            while (appdata->ctx->croncond_pred) {
                pthread_cond_wait(&appdata->ctx->croncond, &appdata->ctx->cronmutex);
            }
            pthread_mutex_unlock(&appdata->ctx->cronmutex);
            
            continue;
        }
        
        /// Case where need to wait N seconds before doing actions
        if (next.tv_sec > 0) {     
            int rc = 0;
            
            // Need to make next.tv_sec compatible with REALTIME clock
            next.tv_sec += time(NULL);
            
            pthread_mutex_lock(&appdata->ctx->cronmutex);
            appdata->ctx->croncond_pred = true;
            while (appdata->ctx->croncond_pred && (rc == 0)) {
                rc = pthread_cond_timedwait(&appdata->ctx->croncond, &appdata->ctx->cronmutex, &next);
            }
            pthread_mutex_unlock(&appdata->ctx->cronmutex);
        }
        
        /// Download some stuff from the assistnow server
        assistnow_apikey = otvar_get_string(dth->vardict, "ubx_key");
        if (assistnow_apikey == NULL) {
            const char* logmsg = "UBX AssistNow API Key is not provided";
            dterm_send_log(dth, logmsg, strlen(logmsg));
        }
        else {
            ubx_packet_t packet;
            int dlbytes;
            uint8_t* cursor;
            int accum;
            int fileerror;
            int numpkts;
            ubx_pkt_t* plist;

// Example 
// https://online-live1.services.u-blox.com/GetOnlineData.ashx?token=OaiZ3dS_eUyYXJRAi4V90Q;datatype=eph,alm,aux;format=mga;gnss=gps;lat=37.7769487;lon=-122.3978603;alt=17;pacc=20;tacc=10;latency=600;
            snprintf(assistnow_url, sizeof(assistnow_url), 
                            "%s?"
                            "token=%s;"
                            "datatype=%s;"
                            "format=%s;"
                            "gnss=%s;"
                            "lat=%lf;"
                            "lon=%lf;"
                            "alt=%i;"
                            "pacc=%i;"
                            "tacc=%i;"
                            "latency=%i;"
                            "filteronpos;"
                            ,
                            otvar_get_string(dth->vardict, "ubx_uri"),
                            assistnow_apikey,
                            otvar_get_string(dth->vardict, "ubx_datatype"),
                            otvar_get_string(dth->vardict, "ubx_format"),
                            otvar_get_string(dth->vardict, "ubx_gnss"),
                            otvar_get_number(dth->vardict, "lat"),
                            otvar_get_number(dth->vardict, "lon"),
                            (int)otvar_get_integer(dth->vardict, "alt"),
                            (int)otvar_get_integer(dth->vardict, "pacc"),
                            (int)otvar_get_integer(dth->vardict, "tacc"),
                            (int)otvar_get_integer(dth->vardict, "latency")
                        );
            
            /// backend context data mutex protects the context elements.
            /// Commands don't need to worry about it, because it is subordinate
            /// to the iso-mutex.
            pthread_mutex_lock(&appdata->ctx->data_mutex);
            dlbytes = utils_downloader(assistnow_url, tmpbuf, assistnow_buf, &assistnow_bufmax, false);
            if (dlbytes <= 0) {
                dterm_send_log(dth, tmpbuf, strlen(tmpbuf));
                goto agnss_parse_EXIT;
            }
            
            fileerror = 4;
            accum = 0;
            numpkts = 0;
            cursor = assistnow_buf;
            while (accum < dlbytes) {
                int filesize;
                packet.header.sync1     = cursor[0];
                packet.header.sync2     = cursor[1];
                packet.header.msg_class = cursor[2];
                packet.header.msg_id    = cursor[3];
                packet.header.plen      = ((uint16_t)cursor[5] * 256) + cursor[4];    //little endian
                
                packet.front            = (uint8_t*)cursor;
                packet.payload          = (uint8_t*)&cursor[6];
                packet.checksum         = packet.payload + packet.header.plen;
                filesize                = 6 + packet.header.plen + 2;
                
                if (sub_validate_ubx(&packet) != 0) {
                    //checksum error
                    fileerror = 1;
                    break;
                }   
                if ((packet.header.sync1 != 0xB5) || (packet.header.sync2 != 0x62)) {
                    //header error
                    fileerror = 2;
                    break;
                }
                if (packet.header.msg_class != 0x13) {
                    //not an MGA packet
                    fileerror = 3;
                    break;
                }
                
                numpkts++;
                accum += filesize;
                cursor += filesize;
            }
            
            if (accum == dlbytes) {
                fileerror = 0;
            }
            else {
                const char* null_err = "Unknown error";
                const char* checksum_err = "Checksum failed";
                const char* header_err = "Header not B562";
                const char* mga_err = "Not an MGA Packet";
                const char* frame_err = "Framing error";
                const char* errmsg[5] = {null_err, checksum_err, header_err, mga_err, frame_err};
                
                snprintf(tmpbuf, sizeof(tmpbuf), "UBX AssistNow packet integrity error on byte offset %i (%s)", 
                            accum, errmsg[fileerror]);
                dterm_send_log(dth, tmpbuf, strlen(tmpbuf));
                goto agnss_parse_EXIT;
            }
            
            // UBX file is validated.  Only remaining error is malloc.
            ///@todo this really should be done via a talloc context.
            plist = malloc(sizeof(ubx_pkt_t) * numpkts);
            if (plist == NULL) {
                const char* logmsg = "UBX AssistNow downloaded, but could not be stored (malloc error)";
                dterm_send_log(dth, logmsg, strlen(logmsg));
                goto agnss_parse_EXIT;
            }
            
            // Clear the old packet list and link it to the new one.
            free(appdata->ctx->assistnow.pkt);
            appdata->ctx->assistnow.numpkts     = numpkts;
            appdata->ctx->assistnow.pkt         = plist;
            appdata->ctx->assistnow.buf         = (uint8_t*)assistnow_buf;
            appdata->ctx->assistnow.bufsize     = dlbytes;
            appdata->ctx->assistnow.timestamp   = time(NULL);
            
            accum = 0;
            cursor = assistnow_buf;
            for (int i=0; i<numpkts; i++) {
                int filesize;
                appdata->ctx->assistnow.pkt[i].hdr.sync1    = cursor[0];
                appdata->ctx->assistnow.pkt[i].hdr.sync2    = cursor[1];
                appdata->ctx->assistnow.pkt[i].hdr.msg_class= cursor[2];
                appdata->ctx->assistnow.pkt[i].hdr.msg_id   = cursor[3];
                appdata->ctx->assistnow.pkt[i].hdr.plen     = ((uint16_t)cursor[5] * 256) + cursor[4];    //little endian
                
                appdata->ctx->assistnow.pkt[i].data         = appdata->ctx->assistnow.buf + accum + 6;
                appdata->ctx->assistnow.pkt[i].len          = appdata->ctx->assistnow.pkt[i].hdr.plen;
                
                filesize    = 6 + (int)appdata->ctx->assistnow.pkt[i].len + 2;
                accum      += filesize;
                cursor     += filesize;
            }

            // Signal any listeners that data is ready
            pthread_mutex_lock(&appdata->ctx->readymutex);
            appdata->ctx->readycond_cnt = 0;
            pthread_cond_broadcast(&appdata->ctx->readycond);
            pthread_mutex_unlock(&appdata->ctx->readymutex);

            DEBUG_PRINTF("UBX Assistnow Package Downloaded on: %s\n", fmt_time(&appdata->ctx->assistnow.timestamp, NULL));

            agnss_parse_EXIT:
            pthread_mutex_unlock(&appdata->ctx->data_mutex);
        }
    }
    
    agnss_reader_TERM:
    
    /// This occurs on uncorrected errors, such as case 4 from above, or other 
    /// unknown errors.
    raise(SIGTERM);
    return NULL;
}







