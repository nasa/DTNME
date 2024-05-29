

#ifndef _MEUTILS_MSG_QUEUE_X_H_
#define _MEUTILS_MSG_QUEUE_X_H_

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>

typedef std::shared_ptr<std::mutex> SPtr_mutex;
typedef std::shared_ptr<std::condition_variable> SPtr_condition_variable;

namespace meutils {


/**
 * A producer/consumer queue for passing data between threads
 */
template<typename _elt_t>
class MsgQueueX {
public:    
    /*!
     * Constructor.
     */
    MsgQueueX();
    MsgQueueX(SPtr_mutex& sptr_mutex, SPtr_condition_variable& sptr_cond_var);
        
    /**
     * Destructor.
     */
    ~MsgQueueX();

    /**
     * Atomically add msg to the back of the queue and signal a
     * waiting thread.
     */
    void push(_elt_t msg, bool at_back = true);
    
    /**
     * Atomically add msg to the front of the queue, and signal
     * waiting threads.
     */
    void push_front(_elt_t msg)
    {
        push(msg, false);
    }

    /**
     * Atomically add msg to the back of the queue, and signal
     * waiting threads.
     */
    void push_back(_elt_t msg)
    {
        push(msg, true);
    }

    /**
     * Try to pop a msg from the queue, but don't block. Return
     * true if there was a message on the queue, false otherwise.
     */
    bool try_pop(_elt_t* eltp);

    /**
     * Wait for up to the specifed number of millisecs for a message to be queued
     * return true if message was queued or false if timeout
     */
    bool wait_for_millisecs(time_t millisecs);

    /**
     * \return Size of the queue.
     */
    size_t size()
    {
        std::lock_guard<std::mutex> l(*sptr_lock_.get());
        return queue_.size();
    }

    /**
     * \return Size of the queue.
     */
    size_t max_size()
    {
        std::lock_guard<std::mutex> l(*sptr_lock_.get());
        return max_size_;
    }

protected:
    SPtr_mutex              sptr_lock_;
    SPtr_condition_variable sptr_cond_var_;

    std::deque<_elt_t>      queue_;

    size_t                  max_size_;
};

#include "MsgQueueX.tcc"

} // namespace meutils

#endif //_MEUTILS_MSG_QUEUE_X_H_
