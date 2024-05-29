
/*!
 * \file
 *
 * NOTE: This file is included by MsgQueueX.h and should _not_ be
 * included in the regular Makefile build because of template
 * instantiation issues. As of this time, g++ does not have a good,
 * intelligent way of managing template instantiations. The other
 * route to go is to use -fno-implicit-templates and manually
 * instantiate template types. That however would require changing a
 * lot of the existing code, so we will just bear the price of
 * redundant instantiations for now.
 */

template<typename _elt_t>
MsgQueueX<_elt_t>::MsgQueueX()
    : max_size_(0)
{
    sptr_lock_ = std::make_shared<std::mutex>();
    sptr_cond_var_ = std::make_shared<std::condition_variable>();
}

template<typename _elt_t>
MsgQueueX<_elt_t>::MsgQueueX(SPtr_mutex& sptr_mutex, SPtr_condition_variable& sptr_cond_var)
    : sptr_lock_(sptr_mutex),
      sptr_cond_var_(sptr_cond_var),
      max_size_(0)
{
    if (!sptr_lock_) {
        sptr_lock_ = std::make_shared<std::mutex>();
    }
    if (!sptr_cond_var_) {
        sptr_cond_var_ = std::make_shared<std::condition_variable>();
    }
}

template<typename _elt_t>
MsgQueueX<_elt_t>::~MsgQueueX()
{
}

template<typename _elt_t> 
void MsgQueueX<_elt_t>::push(_elt_t msg, bool at_back)
{
    std::lock_guard<std::mutex> l(*sptr_lock_.get());

    if (at_back)
        queue_.push_back(msg);
    else
        queue_.push_front(msg);


    if (queue_.size() > max_size_) {
        max_size_ = queue_.size();
    }

    sptr_cond_var_->notify_all();
}

template<typename _elt_t> 
bool MsgQueueX<_elt_t>::try_pop(_elt_t* eltp)
{
    std::lock_guard<std::mutex> l(*sptr_lock_.get());

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
bool MsgQueueX<_elt_t>::wait_for_millisecs(time_t millisecs)
{
    std::unique_lock<std::mutex> qlok(*sptr_lock_.get());

    return sptr_cond_var_->wait_for(qlok, std::chrono::milliseconds(millisecs)) != std::cv_status::timeout;

}

