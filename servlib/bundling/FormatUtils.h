/*
 *    Copyright 2021 United States Government as represented by NASA
 *       Marshall Space Flight Center. All Rights Reserved.
 *
 *    Released under the NASA Open Source Software Agreement version 1.3;
 *    You may obtain a copy of the Agreement at:
 * 
 *        http://ti.arc.nasa.gov/opensource/nosa/
 * 
 *    The subject software is provided "AS IS" WITHOUT ANY WARRANTY of any kind,
 *    either expressed, implied or statutory and this agreement does not,
 *    in any manner, constitute an endorsement by government agency of any
 *    results, designs or products resulting from use of the subject software.
 *    See the Agreement for the specific language governing permissions and
 *    limitations.
 */

#ifndef _FORNMAT_UTILS_H_
#define _FORNMAT_UTILS_H_

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <string>

namespace fmtutil {


    /**
     * functon to format a number as a 3 digit number with a magnitdue character and a plus sign 
     * if larger the value is larger than the pure magnitude number. 
     * FORMAT_WITH_MAG can be used as shorthand for fmtutil::format_num_with_magnitude
     *
     * @param val the value to format
     *
     * @return string containing the formatted value
     */
    std::string format_num_with_magnitude(size_t val);

    #define FORMAT_WITH_MAG fmtutil::format_num_with_magnitude

    std::string format_num_as_rate(uint64_t val);

    #define FORMAT_AS_RATE fmtutil::format_num_as_rate


} // namespace fmtutil

#endif /* _FORNMAT_UTILS_H_ */

