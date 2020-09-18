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

#include <stdlib.h>
#include <stdio.h>

#if TCLREADLINE_ENABLED
#ifdef READLINE_IS_EDITLINE

#include "readline/readline.h"

int ding()
{
    return 0;
}

void rl_extend_line_buffer(int len)
{
    rl_line_buffer = realloc(rl_line_buffer, len);
}

int history_truncate_file(const char* fname, int lines)
{
    (void)fname;
    (void)lines;
    return 0;
}

#endif // READLINE_IS_EDITLINE
#endif // TCLREADLINE_ENABLED
