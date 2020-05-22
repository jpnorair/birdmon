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


#include "utils.h"

#include <cJSON.h>

#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <curl/curl.h>


cJSON* cJSON_walk(cJSON* obj, int argc, const char* argv[]) {
    for (int i=0; (i<argc) && (obj!=NULL); i++) {
        char* endptr;
        unsigned long index;
        
        index = strtoul(argv[i], &endptr, 0);
        if (*endptr == '\0') {
            if (cJSON_IsArray(obj)) {
                obj = cJSON_GetArrayItem(obj, (int)index);
            }
            else {
                obj = NULL;
            }
        }
        else if (cJSON_IsObject(obj)) {
            obj = cJSON_GetObjectItemCaseSensitive(obj, argv[i]);
        }
        else {
            obj = NULL;
        }
    }

    return obj;
}



typedef struct {
    uint8_t* data;
    size_t   size;
    size_t   max;
    bool     resizable;
} memblock_t;

static size_t data_loader(void *contents, size_t size, size_t nmemb, void *userp) {
    memblock_t* mem     = (memblock_t*)userp;
    size_t chunksize    = size * nmemb;
    size_t newalloc     = mem->size + chunksize + 1;
    
    if (mem->max < newalloc) {
        if (mem->resizable) {
            uint8_t* ptr;
            size_t newmax;
            size_t dblmax  = mem->max*2;
            
            newmax = (dblmax > newalloc) ? dblmax : newalloc;
            ptr = realloc(mem->data, newmax);
            if (ptr == NULL) {
                return 0;   // out of memory
            }
            mem->max    = newmax;
            mem->data   = ptr;
        }
        else {
            // not enough room in buffer
            return 0;
        }
    }

    memcpy( &(mem->data[mem->size]), contents, chunksize );
    mem->size += chunksize;
    // Zero term for text retrieval (EOF)
    mem->data[mem->size] = 0;
    
    return chunksize;
}



//static size_t data_loader(void *ptr, size_t size, size_t nmemb, void *stream) {
//  size_t written = fwrite(ptr, size, nmemb, (FILE *)stream);
//  return written;
//}


int utils_downloader(const char* url, char* errbuf, uint8_t* filebuf, size_t* bufmax, bool buf_isdynamic) {
    CURL* curl_handle;
    CURLcode curl_rc;
    memblock_t memblock;
    const char* err     = NULL;
    const char* errinit = "downloader could not be initialized";
    
    int rc = 0;
    
    if ((url == NULL) || (filebuf == NULL) || (bufmax == NULL)) {
        return -1;
    }
    if (*bufmax == 0) {
        return -1;
    }
    
    // Use libcurl to download the information from google
    curl_handle = curl_easy_init();
    if (curl_handle == NULL) {
        err = errinit;
        rc  = -2;
        goto download_TERM;
    }
    
    memblock.data       = filebuf;
    memblock.max        = *bufmax;
    memblock.size       = 0;
    memblock.resizable  = buf_isdynamic;
    
    for (int i=0; true; i++) {
        switch (i) {
        case 0: curl_rc = curl_easy_setopt(curl_handle, CURLOPT_PROTOCOLS, CURLPROTO_ALL);  break;
        case 1: curl_rc = curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);    break;
        case 2: curl_rc = curl_easy_setopt(curl_handle, CURLOPT_AUTOREFERER, 1L);   break;
        case 3: curl_rc = curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");    break;
        case 4: curl_rc = curl_easy_setopt(curl_handle, CURLOPT_URL, url);  break;
        case 5: curl_rc = curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, &data_loader);   break;
        case 6: curl_rc = curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void*)&memblock);   break;
        case 7: curl_rc = curl_easy_perform(curl_handle);   break;
        default:    
                *bufmax = memblock.max;
                rc      = (int)memblock.size;
                goto download_TERM;
        }
        
        if (curl_rc != 0) {
            err = curl_easy_strerror(curl_rc);
            rc = -2 - i;
            goto download_TERM;
        }
    }
    
    download_TERM:
    curl_easy_cleanup(curl_handle);
    
    if ((errbuf != NULL) && (err != NULL)) {
        strcpy(errbuf, err);
    }
    return rc;
}

