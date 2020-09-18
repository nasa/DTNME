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

#ifndef _CACHECAPACITYHELPER_H_
#define _CACHECAPACITYHELPER_H_

namespace oasys {

/*!
 * Helper class that provides a simple capacity-based helper for the
 * Cache utility clase.
 */
template<typename _Key, typename _Val>
class CacheCapacityHelper {
public:
    CacheCapacityHelper(size_t capacity)
        : capacity_(capacity), count_(0) {}
              
    bool over_limit(const _Key& key, const _Val& val)
    {
        (void)key; (void)val;
        return count_ + 1 > capacity_;
    }
        
    void put(const _Key& key, const _Val& val)
    {
        (void)key; (void)val;
        count_++;
    }
    void cleanup(const _Key& key, const _Val& val)
    {
        (void)key; (void)val;
        count_--;
    }
        
protected:
    size_t capacity_;
    size_t count_;
};

} // namespace oasys

#endif /* _CACHECAPACITYHELPER_H_ */
