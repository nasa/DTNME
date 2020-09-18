/*
 *    Copyright 2015 United States Government as represented by NASA
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

#ifndef _EHS_BUNDLEREF_H_
#define _EHS_BUNDLEREF_H_

#include <oasys/util/Ref.h>

#include "EhsBundle.h"

#ifdef EHSROUTER_ENABLED

namespace dtn {

class EhsBundle;

/**
 * Class definition for a EhsBundle reference.
 */
typedef oasys::Ref<EhsBundle> EhsBundleRef;



// maps of EHS Bundles
typedef std::map<std::string, EhsBundleRef> EhsBundleStrMap;
typedef std::pair<std::string, EhsBundleRef> EhsBundleStrPair;
typedef EhsBundleStrMap::iterator EhsBundleStrIterator;
typedef std::pair<EhsBundleStrIterator, bool> EhsBundleStrResult;

typedef std::map<uint64_t, EhsBundleRef> EhsBundleMap;
typedef std::pair<uint64_t, EhsBundleRef> EhsBundlePair;
typedef EhsBundleMap::iterator EhsBundleIterator;

} // namespace dtn

#endif // EHSROUTER_ENABLED

#endif /* _EHS_BUNDLEREF_H_ */
