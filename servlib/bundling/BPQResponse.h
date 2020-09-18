/*
 *    Copyright 2010-2011 Trinity College Dublin
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

#ifndef _BPQRESPONSE_H_
#define _BPQRESPONSE_H_

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#ifdef BPQ_ENABLED

#include "Bundle.h"
#include "BundleProtocol.h"
#include "BPQBlockProcessor.h"

namespace dtn {
/**
 * Utility class to construct BPQ response bundles.
 */
class BPQResponse {
public:

    /**
     * Constructor-like function to create a new BPQ Response bundle
     */
    static bool create_bpq_response(Bundle*     new_response,
                                    Bundle*     query,
                                    Bundle*     cached_response,
                                    EndpointID& source_eid);
};

} // namespace dtn

#endif /* BPQ_ENABLED */

#endif /* _BPQRESPONSE_H_ */
