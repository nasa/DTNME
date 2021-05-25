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

#ifndef __CSTRING_H__
#define __CSTRING_H__

namespace oasys {

//
// Byte oriented functions
// 

/*!
 * Copy src to dest. Copies at most dest_size - 1 characters and NULL
 * terminates the result in dest. Returns the number of characters
 * copied. If src or dest is null, then nothing is done. src is
 * assumed to be null terminated.
 *
 * NB. strncpy behavior is fairly broken if you think about it.
 */
int cstring_copy(char* dest, size_t dest_size, const char* src)
{
    if (dest == 0 || src == 0) 
    {
        return 0;
    }

    int cc = 0;
    while (dest_size > 1 && *src != '\0') 
    {
        *dest = *src;
        ++dest;
        ++src;
        --dest_size;
        ++cc;
    }

    *dest = '\0';

    return cc;
}

//
// Wide-Character oriented functions (XXX/bowei - TODO: support for
// wide chars)
// 

};

#endif /* __CSTRING_H__ */
