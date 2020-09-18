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

#ifndef _COMPLETIONNOTIFIER_H_
#define _COMPLETIONNOTIFIER_H_

#include <oasys/thread/Notifier.h>
#include <oasys/util/Singleton.h>

namespace dtn {

/**
 * Simple singleton class used by DTN commands when they need to call
 * BundleDaemon::post_and_wait().
 */
class CompletionNotifier : public oasys::Singleton<CompletionNotifier> {
public:
    CompletionNotifier()
        : notifier_("/command/completion_notifier") {}
    
    static oasys::Notifier* notifier() { return &instance()->notifier_; }

protected:
    oasys::Notifier notifier_;
};

} // namespace dtn

#endif /* _COMPLETIONNOTIFIER_H_ */
