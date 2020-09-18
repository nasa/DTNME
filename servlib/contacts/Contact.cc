/*
 *    Copyright 2004-2006 Intel Corporation
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

#include "Contact.h"
#include "bundling/Bundle.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleEvent.h"
#include "bundling/BundleList.h"
#include "conv_layers/ConvergenceLayer.h"

namespace dtn {

/**
 * Constructor
 */
Contact::Contact(const LinkRef& link)
    : RefCountedObject("/dtn/contact/refs"),
      Logger("Contact", "/dtn/contact/%s",link->name()),
      link_(link.object(), "Contact"), cl_info_(NULL)
{
    start_time_.get_time();
    duration_ = 0;
    bps_         = 0;
    latency_  = 0;
    
    log_info("new contact *%p", this);
}

Contact::~Contact()
{
    ASSERT(cl_info_ == NULL);
}

/**
 * Formatting...
 */
int
Contact::format(char* buf, size_t sz) const
{
    return snprintf(buf, sz, "contact %s (started %u.%u)",
                    link_->nexthop(),
                    start_time_.sec_, start_time_.usec_);
}

void
Contact::serialize(oasys::SerializeAction *a)
{
    a->process("start_time_sec", &start_time_.sec_);
    a->process("start_time_usec", &start_time_.usec_);
    a->process("duration", &duration_);
    a->process("bps", &bps_);
    a->process("latency", &latency_);
    a->process("link", link_.object());
}

} // namespace dtn
