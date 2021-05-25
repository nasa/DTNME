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


#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include "FormatUtils.h"

namespace fmtutil {

//----------------------------------------------------------------------
std::string
format_num_with_magnitude(size_t val)
{
    char buf[8];
    const char* magnitude = " ";
    const char* plus_sign = " ";

    uint64_t raw_val = val;  // just in case size_t is only 32 bytes
    uint64_t mag_val;

    if (raw_val < 1000) {
        mag_val = raw_val;
    } else if (raw_val < 1000000) {
        mag_val = raw_val / 1000;
        if ((raw_val % 1000) > 0) {
            plus_sign = "+";
        }
        magnitude = "K";

    } else if (raw_val < 1000000000) {
        mag_val = raw_val / 1000000;
        if ((raw_val % 1000000) > 0) {
            plus_sign = "+";
        }
        magnitude = "M";

    } else if (raw_val < 1000000000000) {
        mag_val = raw_val / 1000000000;
        if ((raw_val % 1000000000) > 0) {
            plus_sign = "+";
        }
        magnitude = "G";

    } else if (raw_val < 1000000000000000) {
        mag_val = raw_val / 1000000000000;
        if ((raw_val % 1000000000000) > 0) {
            plus_sign = "+";
        }
        magnitude = "T";

    } else if (raw_val < 1000000000000000000) {
        mag_val = raw_val / 1000000000000000;
        if ((raw_val % 1000000000000000) > 0) {
            plus_sign = "+";
        }
        magnitude = "P";

    } else {
        // max out at 999P for friendly display purposes
        mag_val = 999;
        plus_sign = "+";
        magnitude = "P";
    }

    snprintf(buf, sizeof(buf), "%3" PRIu64 "%s%s", mag_val, magnitude, plus_sign);

    std::string result(buf);
    return result;
};

//----------------------------------------------------------------------
std::string
format_num_as_rate(uint64_t val)
{
    char buf[12];
    const char* magnitude = " ";
    const char* plus_sign = " ";

    uint64_t raw_val = val;
    uint64_t mag_val;

    if (raw_val < 1000) {
        mag_val = raw_val;
    } else if (raw_val < 1000000) {
        mag_val = raw_val / 1000;
        if ((raw_val % 1000) > 0) {
            plus_sign = "+";
        }
        magnitude = "Kbps";

    } else if (raw_val < 1000000000) {
        mag_val = raw_val / 1000000;
        if ((raw_val % 1000000) > 0) {
            plus_sign = "+";
        }
        magnitude = "Mbps";

    } else if (raw_val < 1000000000000) {
        mag_val = raw_val / 1000000000;
        if ((raw_val % 1000000000) > 0) {
            plus_sign = "+";
        }
        magnitude = "Gbps";

    } else {
        // max out at 999Gbps for friendly display purposes
        mag_val = 999;
        plus_sign = "+";
        magnitude = "Gbps";
    }

    snprintf(buf, sizeof(buf), "%3" PRIu64 "%s%s", mag_val, magnitude, plus_sign);

    std::string result(buf);
    return result;
};

} // namespace fmtutil


