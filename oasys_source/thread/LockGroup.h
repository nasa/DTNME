#ifndef __LOCK_GROUP_H__
#define __LOCK_GROUP_H__

#include "Lock.h"

namespace oasys {

class LockGroup {
public:
    typedef std::vector<Lock*> LockVector;

    LockGroup() {}

    LockGroup(LockGroup& other)
        : locks_(other.locks_)
    {
        other.locks_.clear();
    }

    ~LockGroup() 
    {
        release();
    }

    void add(Lock* lock) 
    { 
        locks_.push_back(lock); 
    }

    void release() 
    {
        for (LockVector::iterator i = locks_.begin();
             i != locks_.end(); ++i)
        {
            (*i)->unlock();
        }
    }

    LockGroup& operator=(LockGroup& other) 
    {
        locks_ = other.locks_;
        other.locks_.clear();

        return *this;
    }

private:
    LockVector locks_;
};

} // namespace tier

#endif // __LOCK_GROUP_H__
