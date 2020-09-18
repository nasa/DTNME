/*
 *    Copyright 2007 Intel Corporation
 * 
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 * 
 *        http://www.apache.org/licenses/LICENSE-2.0
 * 
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */


#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <strings.h>
#include <string.h>
#include <stdlib.h>

#include "sdnv-c.h"

char* progname;
char* numstr;
char* end;
int len;
u_char buf[1024];
u_int64_t val;

int mode = 0;
#define ENCODE 1
#define DECODE 2

char hex[] = "0123456789abcdef";
#define HEXTONUM(x) ((x) < 'a' ? (x) - '0' : x - 'a' + 10)

int main(int argc, char* argv[])
{
    size_t i;
    memset(buf, 0, sizeof(buf));
    
    progname = strrchr(argv[0], '/');
    if (progname == 0) {
        progname = argv[0];
    } else {
        progname++;
    }
    
    if (!strcmp(progname, "num2sdnv")) {
        mode = ENCODE;
    } else if (!strcmp(progname, "sdnv2num")) {
        mode = DECODE;
    }

    argv++;
    argc--;
    
    while (argc > 1) {
        char* arg = argv[0];
        argv++;
        argc--;
        
        if (!strcmp(arg, "-e")) {
            mode = ENCODE;
        } else if (!strcmp(arg, "-d")) {
            mode = DECODE;
        } else {
            fprintf(stderr, "unknown argument %s\n", arg);
            exit(1);
        }
    }

    if (argc != 1 || mode == 0) {
        fprintf(stderr, "usage: %s [-de] <num>\n"
                " -e   encode number to sdnv\n"
                " -d   decode sdnv to number\n",
                progname);
        exit(1);
    }

    numstr = argv[0];
    if (mode == ENCODE) {
        if (numstr[0] == '0' && numstr[1] == 'x') {
            val = strtoull(numstr + 2, &end, 16);
        } else {
            val = strtoull(numstr, &end, 10);
        }
        if (*end != '\0') {
            fprintf(stderr, "invalid number %s\n", numstr);
            exit(1);
        }
        
        len = sdnv_encode(val, buf, sizeof(buf));
    } else {
        if (numstr[0] == '0' && numstr[1] == 'x') {
            numstr += 2;
        }

        if ((strlen(numstr) % 2) != 0) {
            fprintf(stderr, "number string %s must contain full bytes\n",
                    numstr);
            exit(1);
        }
        
        for (i = 0; i < strlen(numstr) / 2; ++i) {
            buf[i] = (HEXTONUM(numstr[2*i]) << 4) +
                     HEXTONUM(numstr[2*i + 1]);
        }
        len = sdnv_decode(buf, strlen(numstr)/2, &val);
    }

    printf("val:  %llu (0x%llx)\n", (unsigned long long)val, (unsigned long long)val);
    printf("len:  %d\n", len);
    printf("sdnv: ");
    if (len > 0) {
        for (i = 0; i < (size_t)len; ++i) {
            printf("%c%c", hex[(buf[i] >> 4) & 0xf], hex[buf[i] & 0xf]);
        }
    }
    printf("\n");

    return 0;
}
