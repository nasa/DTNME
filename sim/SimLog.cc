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

/*
 *    Modifications made to this file by the patch file dtnme_mfs-33289-1.patch
 *    are Copyright 2015 United States Government as represented by NASA
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
#include <dtn-config.h>
#endif

#include <oasys/io/FileIOClient.h>
#include "bundling/Bundle.h"
#include "Node.h"
#include "SimLog.h"
#include "Simulator.h"

namespace oasys {
    template <> dtnsim::SimLog* oasys::Singleton<dtnsim::SimLog>::instance_ = NULL;
}

namespace dtnsim {

//----------------------------------------------------------------------
SimLog::SimLog()
{
    file_ = new oasys::FileIOClient();
    int err = 0;
    if (file_->open("./dtnsim_log.txt",
                    O_CREAT | O_RDWR | O_TRUNC, 0644, &err) < 0) {
        log_crit_p("/dtn/sim/log",
                   "ERROR opening sim log file: %s", strerror(err));
    }
}

//----------------------------------------------------------------------
void
SimLog::flush()
{
    file_->close();
}

//----------------------------------------------------------------------
void
SimLog::log_entry(const char* what, Node* node, Bundle* bundle)
{
    u_int64_t now = BundleTimestamp::get_current_time();

    buf_.appendf("%f\t%s\t%s\t%s\t%s\t%" PRIu64 ",%" PRIu64 "\t%zu\t%" PRIu64 "\n",
                 Simulator::time(),
                 node->name(),
                 what,
                 bundle->source().c_str(),
                 bundle->dest().c_str(),
                 bundle->creation_ts().seconds_,
                 bundle->creation_ts().seqno_,
                 bundle->payload().length(),
                 now - bundle->creation_ts().seconds_);

    file_->write(buf_.data(), buf_.length());
    buf_.trim(buf_.length());
}

} // namespace dtnsim
