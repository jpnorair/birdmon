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





int cmd_schedule(dterm_handle_t* dth, uint8_t* dst, int* inbytes, uint8_t* src, size_t dstmax) {
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
        struct arg_lit* now     = arg_lit0("n","now", "force backend to run now (in addition to scheduled times)");
        struct arg_str* cronexp = arg_str0(NULL,NULL,"cron-expression",
                                            "standard cron expression, including seconds field");
        struct arg_end* end     = arg_end(4);
        void* argtable[]        = { now, cronexp, end };
        const char* error_str;

        ///@todo wrap this routine into cmdutils subroutine
        if (arg_nullcheck(argtable) != 0) {
            rc = -1;
            goto cmd_schedule_TERM;
        }
        if ((argc < 1) || (arg_parse(argc, argv, argtable) > 0)) {
            arg_print_errors(stderr, end, argv[0]);
            rc = -2;
            goto cmd_schedule_TERM;
        }
        
        if (appdata == NULL) {
            return -3;
        }
        
        pthread_mutex_lock(&appdata->ctx->data_mutex);
        if (cronexp->count > 0) {
            cron_parse_expr(cronexp->sval[0], &appdata->ctx->cronexp, &error_str);
            if (error_str != NULL) {
                strncpy((char*)dst, error_str, dstmax);
                ///@todo need to add cron_parse_expr() to error module
                rc = -4;
            }
            
        }
        if (now->count > 0) {
            pthread_mutex_lock(&appdata->ctx->cronmutex);
            appdata->ctx->croncond_pred = false;
            pthread_cond_signal(&appdata->ctx->croncond);
            pthread_mutex_unlock(&appdata->ctx->cronmutex);
        }
        pthread_mutex_unlock(&appdata->ctx->data_mutex);
        
        cmd_schedule_TERM:
        arg_freetable(argtable, sizeof(argtable)/sizeof(argtable[0]));
    }

    cmdutils_freeargv(dth->tctx, argv);
    
    return rc;
}
