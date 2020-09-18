/*
 *    Copyright 2005-2006 Intel Corporation
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
#  include <oasys-config.h>
#endif

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <unistd.h>

#ifdef HAVE_GETOPT_LONG
#include <getopt.h>
#else
#include "compat/getopt.h"
#endif

#include "Getopt.h"

namespace oasys {

//----------------------------------------------------------------------
Getopt::Getopt()
{
    memset(&opts_, 0, sizeof(opts_));
}

//----------------------------------------------------------------------
Getopt::~Getopt()
{
    while (!allopts_.empty()) {
        delete allopts_.back();
        allopts_.pop_back();
    }
}

//----------------------------------------------------------------------
void
Getopt::addopt(Opt* opt)
{
    if (opt->shortopt_ != 0) {
        int c = opt->shortopt_;
        if (opts_[c]) {
            fprintf(stderr,
                    "FATAL ERROR: multiple addopt calls for char '%c'\n", c);
            abort();
        }
        
        opts_[c] = opt;
    }
    allopts_.push_back(opt);
}

//----------------------------------------------------------------------
int
Getopt::getopt(const char* progname, int argc, char* const argv[],
               const char* extra_usage)
{
    Opt* opt;
    char short_opts[256];
    char* optstring = short_opts;
    int c, i;
    struct option* long_opts;

    int nopts = allopts_.size(); 

    // alloc two extra getopt -- one for help, one for all zeros
    long_opts = (struct option*) malloc(sizeof(struct option) * (nopts + 2));
    memset(long_opts, 0, sizeof(struct option) * (nopts + 2));
    
    for (i = 0; i < nopts; ++i)
    {
        opt = allopts_[i];
        
        if (opt->shortopt_) {
            *optstring++ = opt->shortopt_;
            if (opt->needval_) {
                *optstring++ = ':';
            }
        }

        if (opt->longopt_) {
            long_opts[i].name = const_cast<char*>(opt->longopt_);
            long_opts[i].has_arg = opt->needval_;
        } else {
            // ignore this slot
            long_opts[i].name = "help";
        }
    }
                                        
    // tack on the help option
    *optstring++ = 'h';
    *optstring++ = 'H';
    long_opts[nopts].name = "help";
    
    while (1) {
        c = ::getopt_long(argc, argv, short_opts, long_opts, &i);
        switch(c) {
        case 0:
            if (!strcmp(long_opts[i].name, "help"))
            {
                usage(progname, extra_usage);
                exit(0);
            }

            opt = allopts_[i];

            if (opt->set(optarg, optarg ? strlen(optarg) : 0) != 0) {
                fprintf(stderr, "invalid value '%s' for option '--%s'\n",
                        optarg, opt->longopt_);
                exit(1);
            }

            break;
        case ':':
            // missing value to option
            fprintf(stderr, "option %s requires a value\n", long_opts[i].name);
            usage(progname, extra_usage);
            exit(0);
            
        case '?':
        case 'h':
        case 'H':
            usage(progname, extra_usage);
            exit(0);
            
        case -1:
            // end of list
            goto done;
            
        default:
            if (c < 0 || c > 256) {
                fprintf(stderr, "FATAL ERROR: %d returned from getopt\n", c);
                abort();
            }
            opt = opts_[c];

            if (!opt) {
                fprintf(stderr, "unknown char '%c' returned from getopt\n", c);
                exit(1);
            }
                
            if (opt->set(optarg, optarg ? strlen(optarg) : 0) != 0) {
                fprintf(stderr, "invalid value '%s' for option '-%c'\n",
                        optarg, c);
                exit(1);
            }
            
            if (opt->setp_)
                *opt->setp_ = true;
            
        }
    }

 done:
    free(long_opts);
    return optind;
}

//----------------------------------------------------------------------
void
Getopt::usage(const char* progname, const char* extra_usage)
{
    OptList::iterator iter;
    char opts[128];

    const char* s = strrchr(progname, '/');
    if (s != NULL) {
        progname = s + 1;
    }
    fprintf(stderr, "usage: %s [opts] %s\n\nopts:\n",
            progname, extra_usage);

    snprintf(opts, sizeof(opts), "-h, --help");
    fprintf(stderr, "  %-24s%s\n", opts, "show usage");

    for (iter = allopts_.begin(); iter != allopts_.end(); ++iter)
    {
        Opt* opt = *iter;
        
        if (opt->shortopt_ && opt->longopt_)
        {
            snprintf(opts, sizeof(opts), "-%c, --%s %s",
                     opt->shortopt_, opt->longopt_, opt->valdesc_);
        }
        else if (opt->shortopt_)
        {
            snprintf(opts, sizeof(opts), "-%c %s",
                     opt->shortopt_, opt->valdesc_);
        } else {
            snprintf(opts, sizeof(opts), "--%s %s    ",
                     opt->longopt_, opt->valdesc_);
        }

        if (strlen(opts) <= 24) {
            fprintf(stderr, "  %-24s%s\n", opts, opt->desc_);
        } else {
            fprintf(stderr, "  %s\n", opts);
            fprintf(stderr, "                          %s\n", opt->desc_);
        }
    }
}

} // namespace oasys
