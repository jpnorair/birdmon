/* Copyright 2019, JP Norair
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
/**
  * @file       main.c
  * @author     JP Norair
  * @version    R100
  * @date       25 August 2019
  * @brief      BirdMon main() function and global data declarations
  * @defgroup   BirdMon
  * @ingroup    BirdMon
  *
  * See http://wiki.indigresso.com for more information and documentation.
  * 
  ******************************************************************************
  */

// Top Level birdmon application headers
#include "birdmon_app.h"

// Application Sub-Headers
#include "cliopt.h"
#include "cmd_api.h"
#include "cmdhistory.h"
#include "debug.h"

// DTerm to be extricated into its own library
#include "dterm.h"

// Local Libraries
#include <otvar.h>
#include <argtable3.h>
#include <cJSON.h>
#include <cmdtab.h>

// Package Libraries
#include <talloc.h>

// Standard C & POSIX Libraries
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <assert.h>


// tty specifier data type
typedef struct {
    char* ttyfile;
    int baudrate;
    int enc_bits;
    int enc_parity;
    int enc_stopbits;
} ttyspec_t;



// CLI Data Type deals with kill signals
typedef struct {
    int             exitcode;
    volatile bool   kill_cond_inactive;
    pthread_mutex_t kill_mutex;
    pthread_cond_t  kill_cond;
} cli_struct;

static cliopt_t cliopts;
cli_struct cli;










/** main functions & signal handlers <BR>
  * ========================================================================<BR>
  * 
  */

static void sub_assign_signal(int sigcode, void (*sighandler)(int), bool is_critical) {
    if (signal(sigcode, sighandler) != 0) {
        perror("");
        if (is_critical) {
            fprintf(stderr, "--> Error assigning signal (%d): Fatal, exiting\n", sigcode);
            exit(EXIT_FAILURE);
        }
        else {
            fprintf(stderr, "--> Error assigning signal (%d): Ignoring\n", sigcode);
        }
    }
}

static void sigint_handler(int sigcode) {
    cli.exitcode = 0;
    cli.kill_cond_inactive = false;
    pthread_cond_signal(&cli.kill_cond);
}

static void sigquit_handler(int sigcode) {
    cli.exitcode = -1;
    cli.kill_cond_inactive = false;
    pthread_cond_signal(&cli.kill_cond);
}



int birdmon_main( INTF_Type intf,
                char* cronstr,
                char* gmaps_key,
                char* ubxanow_key,
                char* socket,
                bool quiet,
                const char* initfile
            ); 



static INTF_Type sub_intf_cmp(const char* s1) {
    INTF_Type selected_intf;

    if (strcmp(s1, "pipe") == 0) {
        selected_intf = INTF_pipe;
    }
    else if (strcmp(s1, "socket") == 0) {
        selected_intf = INTF_socket;
    }
    else {
        selected_intf = INTF_interactive;
    }
    
    return selected_intf;
}


static FORMAT_Type sub_fmt_cmp(const char* s1) {
    FORMAT_Type selected_fmt;
    
    if (strcmp(s1, "default") == 0) {
        selected_fmt = FORMAT_Default;
    }
    else if (strcmp(s1, "json") == 0) {
        selected_fmt = FORMAT_Json;
    }
    else if (strcmp(s1, "jsonhex") == 0) {
        selected_fmt = FORMAT_JsonHex;
    }
    else if (strcmp(s1, "bintex") == 0) {
        selected_fmt = FORMAT_Bintex;
    }
    else if (strcmp(s1, "hex") == 0) {
        selected_fmt = FORMAT_Hex;
    }
    else {
        selected_fmt = FORMAT_Default;
    }
    
    return selected_fmt;
}






int main(int argc, char* argv[]) {
/// ArgTable params: These define the input argument behavior
#   define FILL_FILEARG(ARGITEM, VAR)   do { \
        size_t str_sz = strlen(ARGITEM->filename[0]) + 1;   \
        if (VAR != NULL) free(VAR);                         \
        VAR = malloc(str_sz);                               \
        if (VAR == NULL) goto main_FINISH;                  \
        memcpy(VAR, ARGITEM->filename[0], str_sz);          \
    } while(0);
    
#   define FILL_STRINGARG(ARGITEM, VAR)   do { \
        size_t str_sz = strlen(ARGITEM->sval[0]) + 1;   \
        if (VAR != NULL) free(VAR);                         \
        VAR = malloc(str_sz);                               \
        if (VAR == NULL) goto main_FINISH;                  \
        memcpy(VAR, ARGITEM->sval[0], str_sz);          \
    } while(0);

    // Generic
    struct arg_lit  *verbose = arg_lit0("v","verbose",                  "Use verbose mode");
    struct arg_lit  *debug   = arg_lit0("d","debug",                    "Set debug mode on (requires compiling for debug)");
    struct arg_lit  *quiet   = arg_lit0("q","quiet",                    "Supress reporting of errors");
    struct arg_lit  *help    = arg_lit0(NULL,"help",                    "Print this help and exit");
    struct arg_lit  *version = arg_lit0(NULL,"version",                 "Print version information and exit");
    // BirdMon-Specific
    struct arg_str  *fmt     = arg_str0("f", "fmt", "format",           "\"default\", \"json\", \"jsonhex\", \"bintex\", \"hex\"");
    struct arg_str  *intf    = arg_str0("i","intf", "interactive|pipe|socket", "Interface select.  Default: interactive");
    struct arg_file *socket  = arg_file0("S","socket","path/addr",      "Socket path/address to use for birdmon daemon");
    struct arg_file *initfile= arg_file0("I","init","path",             "Path to initialization routine to run at startup");
    struct arg_str  *cronstr = arg_str0("C","cron","cron-expr",         "Cron expression (with seconds) to use for AGNSS scheduling");
    struct arg_str  *gmaps   = arg_str0("G","gmapskey", "apikey",       "API key string to google maps");
    struct arg_str  *ubxanow = arg_str0("U","ubxkey", "apikey",         "API key string to UBlox AssistNow");
    // Terminator
    struct arg_end  *end     = arg_end(20);
    
    void* argtable[] = { verbose, debug, quiet, help, version, fmt, intf, cronstr, socket, initfile, gmaps, ubxanow, end };
    const char* progname = BIRDMON_PARAM(NAME);
    int nerrors;
    bool bailout        = true;
    int exitcode        = 0;
    char* gmaps_val     = NULL;
    char* ubxanow_val   = NULL;
    char* initfile_val  = NULL;
    char* cronstr_val   = NULL;
    FORMAT_Type fmt_val = FORMAT_Default;
    INTF_Type intf_val  = INTF_interactive;
    char* socket_val    = NULL;
    bool quiet_val      = false;
    bool verbose_val    = false;

    if (arg_nullcheck(argtable) != 0) {
        /// NULL entries were detected, some allocations must have failed 
        fprintf(stderr, "%s: insufficient memory\n", progname);
        exitcode = 1;
        goto main_FINISH;
    }

    /* Parse the command line as defined by argtable[] */
    nerrors = arg_parse(argc, argv, argtable);

    /// special case: '--help' takes precedence over error reporting
    if (help->count > 0) {
        printf("Usage: %s", progname);
        arg_print_syntax(stdout, argtable, "\n");
        arg_print_glossary(stdout, argtable, "  %-25s %s\n");
        exitcode = 0;
        goto main_FINISH;
    }

    /// special case: '--version' takes precedence error reporting 
    if (version->count > 0) {
        printf("%s -- %s\n", BIRDMON_PARAM_VERSION, BIRDMON_PARAM_DATE);
        printf("Commit-ID: %s\n", BIRDMON_PARAM_GITHEAD);
        printf("Designed by %s\n", BIRDMON_PARAM_BYLINE);
        printf("Based on birdmon by JP Norair (indigresso.com)\n");
        exitcode = 0;
        goto main_FINISH;
    }

    /// If the parser returned any errors then display them and exit
    /// - Display the error details contained in the arg_end struct.
    if (nerrors > 0) {
        arg_print_errors(stdout,end,progname);
        printf("Try '%s --help' for more information.\n", progname);
        exitcode = 1;
        goto main_FINISH;
    }

    /// special case: with no command line options induces brief help 
    if (argc==1) {
        printf("Try '%s --help' for more information.\n",progname);
        exitcode = 0;
        goto main_FINISH;
    }
    
    if (fmt->count != 0) {
        fmt_val = sub_fmt_cmp(fmt->sval[0]);
    }
    if (intf->count != 0) {
        intf_val = sub_intf_cmp(intf->sval[0]);
    }
    if (cronstr->count != 0) {
        FILL_STRINGARG(cronstr, cronstr_val);
    }
    if (gmaps->count != 0) {
        FILL_STRINGARG(gmaps, gmaps_val);
    }
    if (ubxanow->count != 0) {
        FILL_STRINGARG(ubxanow, ubxanow_val);
    }
    if (socket->count != 0) {
        FILL_FILEARG(socket, socket_val);
    }
    if (quiet->count != 0) {
        quiet_val = true;
    }
    if (initfile->count != 0) {
        FILL_FILEARG(initfile, initfile_val);
    }
    if (verbose->count != 0) {
        verbose_val = true;
    }

    // override interface value if socket address is provided
    if (socket_val != NULL) {
        intf_val = INTF_socket;
    }

    /// Client Options.  These are read-only from internal modules
    cliopts.mempool_size = BIRDMON_PARAM_MMAP_PAGESIZE;
    cliopts.intf        = intf_val;
    cliopts.format      = fmt_val;
    cliopts.verbose_on  = verbose_val;
    cliopts.debug_on    = (debug->count != 0) ? true : false;
    cliopts.quiet_on    = quiet_val;
    cliopt_init(&cliopts);

    /// All configuration is done.
    /// Here is where final checks can be done to make sure we have all the data we need.
    ///@note Right now there's only UBX AssistNow support, so it is mandatory
    ///      to supply a UBX key.  That might change in the future if other
    ///      sources for assistive data are supported.
    if (ubxanow_val == NULL) {
        printf("For current versions, a UBX AssistNow key must be supplied.\n");
        goto main_FINISH;
    }
    
    /// At this point, we're dedicated to running the software.
    /// gotos to "main_FINISH" will cause the software to deallocate and exit.
    bailout = false;
    
    main_FINISH:
    arg_freetable(argtable, sizeof(argtable)/sizeof(argtable[0]));
    
    if (bailout == false) {
        exitcode = birdmon_main(intf_val,
                                cronstr_val,
                                gmaps_val,
                                ubxanow_val,
                                socket_val,
                                quiet_val,
                                (const char*)initfile_val
                            );
    }

    free(socket_val);
    free(initfile_val);
    free(cronstr_val);
    free(gmaps_val);
    free(ubxanow_val);

    return exitcode;
}




///@todo copy initialization of dterm from otdb_main()
/// What this should do is start two threads, one for the character I/O on
/// the dterm side, and one for the serial I/O.
int birdmon_main( INTF_Type intf,
                char* cronstr,
                char* gmaps_key,
                char* ubxanow_key,
                char* socket,
                bool quiet,
                const char* initfile) {    
    int rc;
    
    // DTerm Datastructs
    dterm_handle_t dth;
    
    // Application data container
    birdmon_app_t appdata;
    
    // Thread Instances
    pthread_t   thr_backend;
    void*       (*dterm_fn)(void* args);
    pthread_t   thr_dterm;

    // 0 is the success code
    cli.exitcode = 0;
    
    /// Initialize Application Data
    DEBUG_PRINTF("Initializing Application Data\n");
    bzero(&appdata, sizeof(birdmon_app_t));
    
    if (pthread_mutex_init(&cli.kill_mutex, NULL) != 0) {
        cli.exitcode = 1;
        goto birdmon_main_EXIT;
    }
    if (pthread_cond_init(&cli.kill_cond, NULL) != 0) {
        cli.exitcode = 2;
        goto birdmon_main_EXIT;
    }
    if (backend_init((backend_handle_t*)&appdata.ctx, cronstr) != 0) {
        cli.exitcode = 3;
        goto birdmon_main_EXIT;
    }
    
    /// Initialize Environment Variables.
    /// This must be the first module to be initialized.
    ///@todo in the future, let's pull this from an initialization file or
    ///      something dynamic as such.
    
    /// Initialize DTerm data objects
    /// Non intrinsic dterm elements (cmdtab, devtab, etc) get attached
    /// following initialization
    DEBUG_PRINTF("Initializing DTerm ...\n");
    if (dterm_init(&dth, &appdata, NULL, intf) != 0) {
        cli.exitcode = 4;
        goto birdmon_main_EXIT;
    }
    DEBUG_PRINTF("--> done\n");
    
    /// Initialize command search table.
    ///@todo in the future, let's pull this from an initialization file or
    ///      something dynamic as such.
    DEBUG_PRINTF("Initializing commands ...\n");
    if (dterm_init_cmdtab(&dth, NULL) < 0) {
        fprintf(stderr, "Err: command table cannot be initialized.\n");
        cli.exitcode = 5;
        goto birdmon_main_EXIT;
    }
    DEBUG_PRINTF("--> done\n");
    
    /// Initialize environment variables, and set them to defaults.
    DEBUG_PRINTF("Initializing environment variables ...\n");
    if (dterm_init_vardict(&dth) < 0) {
        fprintf(stderr, "Err: variable dictionary cannot be initialized.\n");
        cli.exitcode = 6;
        goto birdmon_main_EXIT;
    }
    else {
        otvar_add(dth.vardict, "gmaps_uri", VAR_String, "https://maps.googleapis.com/maps/api/geocode/json?key=");
        otvar_add(dth.vardict, "gmaps_key", VAR_String, "");
        otvar_add(dth.vardict, "ubx_uri", VAR_String, "https://online-live1.services.u-blox.com/GetOnlineData.ashx");
        otvar_add(dth.vardict, "ubx_key", VAR_String, "");
        otvar_add(dth.vardict, "ubx_datatype", VAR_String, "eph,alm,aux");
        otvar_add(dth.vardict, "ubx_format", VAR_String, "mga");
        otvar_add(dth.vardict, "ubx_gnss", VAR_String, "gps");
        otvar_add(dth.vardict, "lat", VAR_Float, 37.7769487);
        otvar_add(dth.vardict, "lon", VAR_Float, -122.3978603);
        otvar_add(dth.vardict, "alt", VAR_Int, 17);
        otvar_add(dth.vardict, "pacc", VAR_Int, 20);
        otvar_add(dth.vardict, "tacc", VAR_Int, 10);
        otvar_add(dth.vardict, "latency", VAR_Int, 10*60);
    }
    DEBUG_PRINTF("--> done\n");
    //https://online-live1.services.u-blox.com/GetOnlineData.ashx?token=OaiZ3dS_eUyYXJRAi4V90Q
    /// Link Remaining object data into the Application container
    /// Link threads variable instances into appdata
    appdata.dterm_parent = &dth;
    
    /// Open DTerm interface & Setup DTerm threads
    /// If sockets are not used, by design socket_path will be NULL.
    DEBUG_PRINTF("Opening DTerm on %s ...\n", socket);
    dterm_fn = dterm_open(appdata.dterm_parent, socket);
    if (dterm_fn == NULL) {
        cli.exitcode = 7;
        goto birdmon_main_EXIT;
    }
    DEBUG_PRINTF("--> done\n");
    
    // -----------------------------------------------------------------------
    DEBUG_PRINTF("Finished setup of birdmon modules. Now creating app threads.\n");
    // -----------------------------------------------------------------------
    
    /// Invoke the child threads below.  All of the child threads run
    /// indefinitely until an error occurs or until the user quits.  Quit can 
    /// be via Ctl+C or Ctl+\, or potentially also through a dterm command.  
    /// Each thread must be be implemented to raise SIGQUIT or SIGINT on exit
    /// i.e. raise(SIGINT).
    DEBUG_PRINTF("Creating Backend theads\n");
    if (appdata.ctx != NULL) {
        pthread_create(&thr_backend, NULL, &agnss_reader, (void*)&appdata);
    }
    
    /// Before doing any interactions, run a command file if it exists.
    DEBUG_PRINTF("Running Initialization Command File\n");
    if (initfile != NULL) {
        //VERBOSE_PRINTF("Running init file: %s\n", initfile);
        if (dterm_cmdfile(appdata.dterm_parent, initfile) < 0) {
            //fprintf(stderr, ERRMARK"Could not run initialization file.\n");
            fprintf(stderr, "Could not run initialization file.\n");
        }
        else {
            //VERBOSE_PRINTF("Init file finished successfully\n");
        }
    }
    DEBUG_PRINTF("--> done\n");
    
    /// Initialize the signal handlers for this process.
    /// These are activated by Ctl+C (SIGINT) and Ctl+\ (SIGQUIT) as is
    /// typical in POSIX apps.  When activated, the threads are halted and
    /// Application is shutdown.
    DEBUG_PRINTF("Assign Kill Signals\n");
    sub_assign_signal(SIGTERM, &sigint_handler, true);
    sub_assign_signal(SIGINT, &sigint_handler, false);
    DEBUG_PRINTF("--> done\n");
    
    DEBUG_PRINTF("Creating Dterm theads\n");
    ///@todo thread_active should be handled internally, maybe in dterm_open?
    dth.thread_active = true;
    pthread_create(&thr_dterm, NULL, dterm_fn, (void*)&dth);
    DEBUG_PRINTF("--> done\n");
    
// ----------------------------------------------------------------------------
    
    /// Threads are now running.  The rest of the main() code, below, is
    /// blocked by pthread_cond_wait() until the kill_cond is sent by one of
    /// the child threads.  This will cause the program to quit.
    
// ----------------------------------------------------------------------------
    
    /// Wait for kill cond.  Basic functionality is in the dterm thread(s).
    cli.kill_cond_inactive = true;
    while (cli.kill_cond_inactive) {
        pthread_cond_wait(&cli.kill_cond, &cli.kill_mutex);
    }

    ///@todo hack: wait for dterm thread to close on its own
    usleep(100000);

    DEBUG_PRINTF("Cancelling Theads\n");
    pthread_cancel(thr_dterm);

    if (appdata.ctx != NULL) {
        pthread_cancel(thr_backend);
    }

    /// Close the drivers/files and deinitialize all submodules
    birdmon_main_EXIT:
    
    // Return cJSON and argtable to generic context allocators
    cJSON_InitHooks(NULL);
    arg_set_allocators(NULL, NULL);
    
    switch (cli.exitcode) {
       default:
        case 7: 
        case 6: 
        case 5: DEBUG_PRINTF("Deinitializing DTerm\n");
                dterm_deinit(&dth);

        case 4: // Failure on backend_opentty()
                DEBUG_PRINTF("Deinitializing Backend\n");
                backend_deinit(appdata.ctx);
            
        case 3: DEBUG_PRINTF("Destroying cli.kill_cond\n");
                pthread_cond_destroy(&cli.kill_cond);
            
        case 2: DEBUG_PRINTF("Destroying cli.kill_mutex\n");
                pthread_mutex_unlock(&cli.kill_mutex);
                pthread_mutex_destroy(&cli.kill_mutex);
        
        case 1: break;
    }

    DEBUG_PRINTF("Exiting cleanly and flushing output buffers\n");
    fflush(stdout);
    fflush(stderr);

    return cli.exitcode;
}





