/*
 *    Copyright 2006 Intel Corporation
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

#include <cstdio>

#include "../Log.h"
#include "LogConfigParser.h"

namespace oasys {

//----------------------------------------------------------------------------
LogConfigParser::LogConfigParser(const char* filename, 
                                 RuleSet*    rs,
                                 Option*     opts)
    : filename_(filename),
      rs_(rs),
      opts_(opts)
{}

//----------------------------------------------------------------------------
int
LogConfigParser::parse()
{
    FILE* fp = fopen(filename_, "r");
    if (fp == 0) { 
        return;
    }

    char buf[256];
    int linenum = 0;

    while (!feof(fp)) 
    {
        if (fgets(buf, sizeof(buf), fp) > 0) 
        {
            char *line = buf;
            char *logpath;
            char *level;
            char *rest;

            ++linenum;
            logpath = line;

            // skip leading whitespace
            while (*logpath && isspace(*logpath)) 
            {
                ++logpath;
            }

            if (! *logpath) 
            {
                // blank line
                continue;
            }

            // skip comment lines
            if (logpath[0] == '#') 
            {
                continue;
            }

            // options
            if (logpath[0] == '%') 
            {
                for (Option* opt = opts_; opt->option_str_ != 0; ++opt)
                {
                    if (strstr(logpath, opt->option_str_) != 0) {
                        // If the option starts with no, then we
                        // disable the flag
                        if (opt->option_str[0] == 'n' &&
                            opt->option_str[1] == 'o')
                        {
                            flags_ &= ~opt->flag_value_;
                        }
                        else
                        {
                            flags_ |= opt->flag_value_;
                        }
                    }
                }
                continue;
            }

            // find the end of path and null terminate
            level = logpath;
            while (*level && !isspace(*level)) 
            {
                ++level;
            }
            *level = '\0';
            ++level;

            // skip any other whitespace
            while (level && isspace(*level)) 
            {
                ++level;
            }

            if (!level) 
            {
                goto parse_err;
            }

            // null terminate the level
            rest = level;
            while (rest && !isspace(*rest)) 
            {
                ++rest;
            }

            int priority = 1000;
            if (rest) 
            {
                *rest = '\0';
                ++rest;
                // Handle glob expressions with optional priorities
                if (logpath[0] == '=') 
                {
                    priority = atoi(rest);
                    if (priority == 0) 
                    {
                        priority = 1000;
                    }
                }
            }

            log_level_t threshold = Log::str2level(level);
            if (threshold == LOG_INVALID) 
            {
                goto parse_err;
            }

#ifdef NDEBUG
            if (threshold == LOG_DEBUG) 
            {
                fprintf(stderr, "WARNING: debug level log rule for path %s "
                        "ignored in non-debugging build\n",
                        logpath);
                continue;
            }
#endif
            
            if (logpath[0] == '=')
            {
                rs_->add_glob_rule(logpath + 1, threshold, priority);
            }
            else
            {
                rs_->add_prefix_rule(logpath, threshold);
            }
        }

      parse_err:
        fprintf(stderr, "Error in log configuration %s line %d\n",
                debug_path, linenum);
    } // while(feof...)

    fclose(fp);
}

} // namespace oasys
