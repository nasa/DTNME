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

#ifndef _BUNDLEREF_H_
#define _BUNDLEREF_H_

#include <oasys/util/Ref.h>

namespace dtn {

class Bundle;

/**
 * Class definition for a Bundle reference.
 */
typedef oasys::Ref<Bundle> BundleRef;

} // namespace dtn

#endif /* _BUNDLEREF_H_ */
