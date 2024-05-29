
/*!
 * \file
 *
 * NOTE: This file is included by MsgQueue.h and should _not_ be
 * included in the regular Makefile build because of template
 * instantiation issues. As of this time, g++ does not have a good,
 * intelligent way of managing template instantiations. The other
 * route to go is to use -fno-implicit-templates and manually
 * instantiate template types. That however would require changing a
 * lot of the existing code, so we will just bear the price of
 * redundant instantiations for now.
 */

template<typename _elt_t>
MsgQueue<_elt_t>::MsgQueue()
    : max_size_(0)
{
}

template<typename _elt_t>
MsgQueue<_elt_t>::~MsgQueue()
{
}

template<typename _elt_t> 
void MsgQueue<_elt_t>::push(_elt_t msg, bool at_back)
{
    std::lock_guard<std::mutex> l(lock_);

    if (at_back)
        queue_.push_back(msg);
    else
        queue_.push_front(msg);


    if (queue_.size() > max_size_) {
        max_size_ = queue_.size();
    }

    cond_var_.notify_all();
}

template<typename _elt_t> 
bool MsgQueue<_elt_t>::try_pop(_elt_t* eltp)
{
    std::lock_guard<std::mutex> l(lock_);

    // nothing in the queue, nothing we can do...
    if (queue_.size() == 0) {
        return false;
    }
    
    // but if there is something in the queue, then return it
    *eltp = queue_.front();
    queue_.pop_front();
    
    return true;
}

template<typename _elt_t> 
bool MsgQueue<_elt_t>::wait_for_millisecs(time_t millisecs)
{
    std::unique_lock<std::mutex> qlok(lock_);

    return cond_var_.wait_for(qlok, std::chrono::milliseconds(millisecs)) != std::cv_status::timeout;

}

