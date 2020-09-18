/*
 * Copyright 2007 BBN Technologies Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License. You
 * may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied.
 */

/*
 * $Id$
 */

#ifndef _SPD_H_
#define _SPD_H_

#ifdef BSP_ENABLED

#include <oasys/util/Singleton.h>
#include "bundling/Bundle.h"
#include "bundling/BlockInfo.h"
#include "contacts/Link.h"

namespace dtn {

/**
 * Not a real (IPsec-like) SPD, just a placeholder that contains:
 *   - global BAB on/off setting
 *   - global PIB on/off setting
 *   - global PCB on/off setting
 *   - preshared secret for BAB
 *   - public keys for PIB and PCB
 */
class SPD : public oasys::Singleton<SPD, false> {
public:

    /**
     * Constructor (called at startup).
     */
    SPD();

    /**
     * Destructor (called at shutdown).
     */
    ~SPD();

    /**
     * Boot time initializer.
     */
    static void init();

    /**
     * Add the security blocks required by security policy for the
     * given outbound bundle.
     */
    static int prepare_out_blocks(const Bundle* bundle,
                                   const LinkRef& link,
                                   BlockInfoVec* xmit_blocks);

    /**
     * Check whether sequence of BP_Tags created during input processing
     * meets the security policy for this bundle.
     */
    static bool verify_in_policy(const Bundle* bundle);

private:
    static bool verify_one_ciphersuite(set<int> *cs, const Bundle *bundle, const BlockInfoVec *recv_blocks);

};

} // namespace dtn

#endif /* BSP_ENABLED */

#endif /* _SPD_H_ */
