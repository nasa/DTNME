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

#ifndef _STATIC_BUNDLE_ROUTER_H_
#define _STATIC_BUNDLE_ROUTER_H_

#include "TableBasedRouter.h"

namespace dtn {

class RouteTable;

/**
 * This is the implementation of the basic bundle routing algorithm
 * that only does static routing. Routes can be parsed from a
 * configuration file or injected via the command line and/or
 * management interfaces.
 *
 * As a result, the class simply uses the default event handlers from
 * the table based router implementation.
 */
 
class StaticBundleRouter : public TableBasedRouter {
public:
    StaticBundleRouter() : TableBasedRouter("StaticBundleRouter", "static") {}
};

} // namespace dtn

#endif /* _STATIC_BUNDLE_ROUTER_H_ */
