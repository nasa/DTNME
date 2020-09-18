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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#ifdef EHSROUTER_ENABLED

#if defined(XERCES_C_ENABLED) && defined(EXTERNAL_DP_ENABLED)

#include "EhsLink.h"
#include "EhsBundleTree.h"

namespace dtn {


/***********************************************************************
 *  EhsBundlePriorityQueue methods
 ***********************************************************************/

//----------------------------------------------------------------------
EhsBundlePriorityQueue::EhsBundlePriorityQueue(uint64_t source_node_id, uint64_t dest_node_id)
{
    source_node_id_ = source_node_id;
    dest_node_id_    = dest_node_id;

    total_pending_ = 0;
    total_custody_ = 0;
    total_bytes_   = 0;

    total_received_ = 0;
    total_transmitted_ = 0;
    total_rejected_ = 0;
    total_delivered_ = 0;
    total_expired_ = 0;


    total_expedited_rcv_ = 0;
    total_expedited_xmt_ = 0;

    total_pending_bulk_ = 0;
    total_pending_normal_ = 0;
    total_pending_expedited_ = 0;
    total_pending_reserved_ = 0;
}

//----------------------------------------------------------------------
EhsBundlePriorityQueue::~EhsBundlePriorityQueue()
{
    clear();
}

//----------------------------------------------------------------------
EhsBundleRef
EhsBundlePriorityQueue::pop_bundle()
{
    EhsBundleRef bref("EhsBundlePriorityQueue::pop_bundle");

    oasys::ScopeLock l(&lock_, __FUNCTION__);

    EhsBundleStrIterator iter = list_.begin();
    if (iter != list_.end()) {
        bref = iter->second;
        list_.erase(iter);

//fprintf(stderr, "... PriorityQueue - pop_bundle: %" PRIbid "\n", bref->bundleid()); fflush(stderr); //dz debug

        dec_stats(bref);
    }

    ASSERT(list_.size() == total_pending_);

    return bref;
}

//----------------------------------------------------------------------
EhsBundleRef
EhsBundlePriorityQueue::peek_bundle()
{
    EhsBundleRef bref("EhsBundlePriorityQueue::peek_bundle");

    oasys::ScopeLock l(&lock_, __FUNCTION__);

    EhsBundleStrIterator iter = list_.begin();
    if (iter != list_.end()) {
        bref = iter->second;
    }

    return bref;
}

//----------------------------------------------------------------------
std::string
EhsBundlePriorityQueue::first_bundle_priority()
{
    std::string priority;

    oasys::ScopeLock l(&lock_, __FUNCTION__);

    EhsBundleStrIterator iter = list_.begin();
    if (iter != list_.end()) {
        priority = iter->second->priority_key();
    }

    return priority;
}

//----------------------------------------------------------------------
bool
EhsBundlePriorityQueue::insert_bundle(EhsBundleRef& bref)
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    EhsBundleStrResult result = list_.insert(EhsBundleStrPair(bref->priority_key(), bref));
    if (result.second) {
        inc_stats(bref);
//fprintf(stderr, "... PriorityQueue - insert bundle: %" PRIbid "\n", bref->bundleid()); fflush(stderr); //dz debug
    }

    return result.second;
}

//----------------------------------------------------------------------
void
EhsBundlePriorityQueue::clear()
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    list_.clear();

    total_pending_ = 0;
    total_custody_ = 0;
    total_bytes_   = 0;

    total_received_ = 0;
    total_transmitted_ = 0;
    total_rejected_ = 0;
    total_delivered_ = 0;
    total_expired_ = 0;

    total_expedited_rcv_ = 0;
    total_expedited_xmt_ = 0;

    total_pending_bulk_ = 0;
    total_pending_normal_ = 0;
    total_pending_expedited_ = 0;
    total_pending_reserved_ = 0;
}


//----------------------------------------------------------------------
size_t
EhsBundlePriorityQueue::size() const
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);
    return total_pending_;
}

//----------------------------------------------------------------------
bool
EhsBundlePriorityQueue::empty() const
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);
    return (total_pending_ == 0);
}

//----------------------------------------------------------------------
void
EhsBundlePriorityQueue::inc_stats(EhsBundleRef& bref)
{
    total_pending_ += 1;
    total_bytes_   += bref->length();

    if (bref->is_cos_bulk()) {
        ++total_pending_bulk_;
    } else if (bref->is_cos_normal()) {
        ++total_pending_normal_;
    } else if (bref->is_cos_expedited()) {
        ++total_pending_expedited_;
    } else {
        ++total_pending_reserved_;
    }
}

//----------------------------------------------------------------------
void
EhsBundlePriorityQueue::dec_stats(EhsBundleRef& bref)
{
    total_pending_ -= 1;
    total_bytes_   -= bref->length();

    if (bref->is_cos_bulk()) {
        --total_pending_bulk_;
    } else if (bref->is_cos_normal()) {
        --total_pending_normal_;
    } else if (bref->is_cos_expedited()) {
        --total_pending_expedited_;
    } else {
        --total_pending_reserved_;
    }
}

//----------------------------------------------------------------------
void
EhsBundlePriorityQueue::inc_received()
{
    ++total_received_;
}

//----------------------------------------------------------------------
void
EhsBundlePriorityQueue::inc_transmitted()
{
    ++total_transmitted_;
}

//----------------------------------------------------------------------
void
EhsBundlePriorityQueue::inc_expired()
{
    ++total_expired_;
}

//----------------------------------------------------------------------
void
EhsBundlePriorityQueue::inc_delivered()
{
    ++total_delivered_;
}

//----------------------------------------------------------------------
void
EhsBundlePriorityQueue::inc_rejected()
{
    ++total_rejected_;
}

//----------------------------------------------------------------------
void
EhsBundlePriorityQueue::inc_custody()
{
    ++total_custody_;
}

//----------------------------------------------------------------------
void
EhsBundlePriorityQueue::dec_custody()
{
    --total_custody_;
}

//----------------------------------------------------------------------
void
EhsBundlePriorityQueue::dump(oasys::StringBuffer* buf)
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    buf->appendf("    %" PRIu64 " -> %" PRIu64 "  bundles: %" PRIu64 " bytes: %" PRIu64
                 " expedited: %" PRIu64 " normal: %" PRIu64 " bulk: %" PRIu64 "\n",
                 source_node_id_, dest_node_id_, total_pending_, total_bytes_, 
                 total_pending_expedited_, total_pending_normal_, total_pending_bulk_);
}


/***********************************************************************
 *  EhsBundleMapWithStats methods
 ***********************************************************************/

//----------------------------------------------------------------------
EhsBundleMapWithStats::EhsBundleMapWithStats()
{
    max_expiration_fwd_ = 8 * 60 * 60;  // 8 hours worth of seconds
    max_expiration_rtn_ = 48 * 60 * 60;  // 48 hours worth of seconds
    total_pending_ = 0;
    total_custody_ = 0;
    total_bytes_   = 0;

    total_received_ = 0;
    total_transmitted_ = 0;
    total_transmit_failed_ = 0;
    total_rejected_ = 0;
    total_delivered_ = 0;
    total_expired_ = 0;

    total_expedited_rcv_ = 0;
    total_expedited_xmt_ = 0;

    total_pending_bulk_ = 0;
    total_pending_normal_ = 0;
    total_pending_expedited_ = 0;
    total_pending_reserved_ = 0;
}

//----------------------------------------------------------------------
EhsBundleMapWithStats::~EhsBundleMapWithStats()
{
    clear();
}

//----------------------------------------------------------------------
EhsBundleRef
EhsBundleMapWithStats::find(uint64_t bundleid)
{
    EhsBundleRef bref("find");

    oasys::ScopeLock l(&lock_, __FUNCTION__);

    Iterator biter = list_.find(bundleid);
    if (biter != list_.end()) {
        bref = biter->second;
    }

    return bref;
}

//----------------------------------------------------------------------
bool
EhsBundleMapWithStats::bundle_received(EhsBundleRef& bref)
{
    bool result = false;

    oasys::ScopeLock l(&lock_, __FUNCTION__);

    Iterator biter = list_.find(bref->bundleid());
    if (biter == list_.end()) {
        list_.insert(Pair(bref->bundleid(), bref));
        inc_stats(bref);
        result = true;
    }

    return result;
}

//----------------------------------------------------------------------
void
EhsBundleMapWithStats::erase_bundle(EhsBundleRef& bref)
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    Iterator biter = list_.find(bref->bundleid());
    if (biter != list_.end()) {
        list_.erase(biter);

        dec_stats(bref);
    }
}

//----------------------------------------------------------------------
EhsBundleRef
EhsBundleMapWithStats::bundle_expired(uint64_t bundleid)
{
    EhsBundleRef bref("bundle_expired");

    oasys::ScopeLock l(&lock_, __FUNCTION__);

    Iterator biter = list_.find(bundleid);
    if (biter != list_.end()) {
        bref = biter->second;
        list_.erase(biter);

        dec_stats(bref);
        inc_expired(bref);
    }

    return bref;
}


//----------------------------------------------------------------------
void
EhsBundleMapWithStats::bundle_rejected(EhsBundleRef& bref)
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    Iterator biter = list_.find(bref->bundleid());
    if (biter != list_.end()) {
        list_.erase(biter);

        dec_stats(bref);
        inc_rejected(bref);
    }
}



//----------------------------------------------------------------------
EhsBundleRef
EhsBundleMapWithStats::bundle_custody_accepted(uint64_t bundleid)
{
    EhsBundleRef bref("bundle_custody_accepted");

    oasys::ScopeLock l(&lock_, __FUNCTION__);

    Iterator biter = list_.find(bundleid);
    if (biter != list_.end()) {
        bref = biter->second;

        inc_custody(bref);
    }

    return bref;
}

//----------------------------------------------------------------------
EhsBundleRef
EhsBundleMapWithStats::bundle_custody_released(uint64_t bundleid)
{
    EhsBundleRef bref("bundle_custody_accepted");

    oasys::ScopeLock l(&lock_, __FUNCTION__);

    Iterator biter = list_.find(bundleid);
    if (biter != list_.end()) {
        bref = biter->second;

        dec_custody(bref);
    }

    return bref;
}

//----------------------------------------------------------------------
EhsBundleRef
EhsBundleMapWithStats::bundle_transmitted(uint64_t bundleid, bool success)
{
    EhsBundleRef bref("bundle_transmitted");

    oasys::ScopeLock l(&lock_, __FUNCTION__);

    Iterator biter = list_.find(bundleid);
    if (biter != list_.end()) {
        bref = biter->second;

        if (success) 
            inc_transmitted(bref);
        else
            inc_transmit_failed(bref);
    }

    return bref;
}


//----------------------------------------------------------------------
EhsBundleRef
EhsBundleMapWithStats::bundle_delivered(uint64_t bundleid)
{
    EhsBundleRef bref("bundle_transmitted");

    oasys::ScopeLock l(&lock_, __FUNCTION__);

    Iterator biter = list_.find(bundleid);
    if (biter != list_.end()) {
        bref = biter->second;

        inc_delivered(bref);
    }

    return bref;
}





//----------------------------------------------------------------------
void
EhsBundleMapWithStats::clear()
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    EhsBundleRef bref("clear");
    Iterator biter = list_.begin();
    while (biter != list_.end()) {
        bref = biter->second;

        list_.erase(biter);

        bref->set_exiting();
        bref.release();

        biter = list_.begin();
    }
    

    StatsIterator siter = stats_map_.begin();
    while (siter != stats_map_.end()) {
        EhsBundleStats* one_stat = siter->second;
        delete one_stat;
        stats_map_.erase(siter);

        siter = stats_map_.begin();
    }

    total_pending_ = 0;
    total_custody_ = 0;
    total_bytes_ = 0;

    total_received_ = 0;
    total_transmitted_ = 0;
    total_transmit_failed_ = 0;
    total_rejected_ = 0;
    total_delivered_ = 0;
    total_expired_ = 0;

    total_expedited_rcv_ = 0;
    total_expedited_xmt_ = 0;

    total_pending_bulk_ = 0;
    total_pending_normal_ = 0;
    total_pending_expedited_ = 0;
    total_pending_reserved_ = 0;
}


//----------------------------------------------------------------------
size_t
EhsBundleMapWithStats::size() const
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);
    return list_.size();
}

//----------------------------------------------------------------------
bool
EhsBundleMapWithStats::empty() const
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);
    return list_.empty();
}

//----------------------------------------------------------------------
void
EhsBundleMapWithStats::inc_stats(EhsBundleRef& bref)
{
    EhsSrcDstKey key(bref->ipn_source_node(), bref->ipn_dest_node());
    EhsBundleStats* one_stat = NULL;

    StatsIterator siter = stats_map_.find(key);
    if (siter != stats_map_.end()) {
        one_stat = siter->second;
    } else {
        one_stat = new EhsBundleStats();
        one_stat->source_node_id_ = bref->ipn_source_node();
        one_stat->dest_node_id_ = bref->ipn_dest_node();
        one_stat->interval_stats_ = new EhsFwdLinkIntervalStats();
        one_stat->interval_stats_->source_node_id_ = bref->ipn_source_node();
        one_stat->interval_stats_->dest_node_id_ = bref->ipn_dest_node();
        stats_map_.insert(StatsPair(key, one_stat));
    }
    one_stat->total_received_ += 1;
    one_stat->total_pending_ += 1;
    one_stat->total_bytes_ += bref->length();


    total_received_ += 1;
    total_pending_ += 1;
    total_bytes_   += bref->length();

    if (bref->local_custody()) {
        one_stat->total_custody_ += 1;
        total_custody_ += 1;
    }

    if (bref->is_fwd_link_destination()) {
        if (bref->expiration() > max_expiration_fwd_) {
            one_stat->total_ttl_abuse_ += 1;
        }
    } else {
        if (bref->expiration() > max_expiration_rtn_) {
            one_stat->total_ttl_abuse_ += 1;
        }
    }

    if (bref->is_cos_bulk()) {
        one_stat->total_pending_bulk_ += 1;
        total_pending_bulk_ += 1;
    } else if (bref->is_cos_normal()) {
        one_stat->total_pending_normal_ += 1;
        total_pending_normal_ += 1;
    } else if (bref->is_cos_expedited()) {
        one_stat->total_pending_expedited_ += 1;
        one_stat->total_expedited_rcv_ += 1;
        total_pending_expedited_ += 1;
    } else {
        one_stat->total_pending_reserved_ += 1;
        total_pending_reserved_ += 1;
    }

    if (NULL != one_stat->interval_stats_) {
        one_stat->interval_stats_->bundles_received_ += 1;
        one_stat->interval_stats_->bytes_received_ += bref->length();
    }
}

//----------------------------------------------------------------------
void
EhsBundleMapWithStats::dec_stats(EhsBundleRef& bref)
{
    EhsSrcDstKey key(bref->ipn_source_node(), bref->ipn_dest_node());
    EhsBundleStats* one_stat = NULL;

    StatsIterator siter = stats_map_.find(key);
    if (siter != stats_map_.end()) {
        one_stat = siter->second;
    } else {
        one_stat = new EhsBundleStats();
        one_stat->interval_stats_ = new EhsFwdLinkIntervalStats();
    }
    one_stat->total_pending_ -= 1;
    one_stat->total_bytes_ -= bref->length();

    total_pending_ -= 1;
    total_bytes_   -= bref->length();

    if (bref->local_custody()) {
        one_stat->total_custody_ -= 1;
        total_custody_ -= 1;
    }

    if (bref->is_cos_bulk()) {
        one_stat->total_pending_bulk_ -= 1;
        total_pending_bulk_ -= 1;
    } else if (bref->is_cos_normal()) {
        one_stat->total_pending_normal_ -= 1;
        total_pending_normal_ -= 1;
    } else if (bref->is_cos_expedited()) {
        one_stat->total_pending_expedited_ -= 1;
        total_pending_expedited_ -= 1;
    } else {
        one_stat->total_pending_reserved_ -= 1;
        total_pending_reserved_ -= 1;
    }
}

//----------------------------------------------------------------------
void
EhsBundleMapWithStats::inc_transmitted(EhsBundleRef& bref)
{
    EhsSrcDstKey key(bref->ipn_source_node(), bref->ipn_dest_node());
    EhsBundleStats* one_stat = NULL;

    StatsIterator siter = stats_map_.find(key);
    if (siter != stats_map_.end()) {
        one_stat = siter->second;
        one_stat->total_transmitted_++;

        if (bref->is_cos_expedited()) {
            one_stat->total_expedited_xmt_ += 1;
        }

        if (NULL != one_stat->interval_stats_) {
            one_stat->interval_stats_->bundles_transmitted_ += 1;
            one_stat->interval_stats_->bytes_transmitted_ += bref->length();
        }
    }

    ++total_transmitted_;
}

//----------------------------------------------------------------------
void
EhsBundleMapWithStats::inc_transmit_failed(EhsBundleRef& bref)
{
    EhsSrcDstKey key(bref->ipn_source_node(), bref->ipn_dest_node());
    EhsBundleStats* one_stat = NULL;

    StatsIterator siter = stats_map_.find(key);
    if (siter != stats_map_.end()) {
        one_stat = siter->second;
        one_stat->total_transmit_failed_++;
    }

    ++total_transmit_failed_;
}

//----------------------------------------------------------------------
void
EhsBundleMapWithStats::inc_expired(EhsBundleRef& bref)
{
    EhsSrcDstKey key(bref->ipn_source_node(), bref->ipn_dest_node());
    EhsBundleStats* one_stat = NULL;

    StatsIterator siter = stats_map_.find(key);
    if (siter != stats_map_.end()) {
        one_stat = siter->second;
        one_stat->total_expired_++;
    }

    ++total_expired_;
}

//----------------------------------------------------------------------
void
EhsBundleMapWithStats::inc_delivered(EhsBundleRef& bref)
{
    EhsSrcDstKey key(bref->ipn_source_node(), bref->ipn_dest_node());
    EhsBundleStats* one_stat = NULL;

    StatsIterator siter = stats_map_.find(key);
    if (siter != stats_map_.end()) {
        one_stat = siter->second;
        one_stat->total_delivered_++;
    }

    ++total_delivered_;
}

//----------------------------------------------------------------------
void
EhsBundleMapWithStats::inc_rejected(EhsBundleRef& bref)
{
    EhsSrcDstKey key(bref->ipn_source_node(), bref->ipn_dest_node());
    EhsBundleStats* one_stat = NULL;

    StatsIterator siter = stats_map_.find(key);
    if (siter != stats_map_.end()) {
        one_stat = siter->second;
        one_stat->total_rejected_++;
    }

    ++total_rejected_;
}

//----------------------------------------------------------------------
void
EhsBundleMapWithStats::inc_custody(EhsBundleRef& bref)
{
    EhsSrcDstKey key(bref->ipn_source_node(), bref->ipn_dest_node());
    EhsBundleStats* one_stat = NULL;

    StatsIterator siter = stats_map_.find(key);
    if (siter != stats_map_.end()) {
        one_stat = siter->second;
        one_stat->total_custody_++;
    }

    ++total_custody_;
}

//----------------------------------------------------------------------
void
EhsBundleMapWithStats::dec_custody(EhsBundleRef& bref)
{
    EhsSrcDstKey key(bref->ipn_source_node(), bref->ipn_dest_node());
    EhsBundleStats* one_stat = NULL;

    StatsIterator siter = stats_map_.find(key);
    if (siter != stats_map_.end()) {
        one_stat = siter->second;
        one_stat->total_custody_--;
    }

    --total_custody_;
}


//----------------------------------------------------------------------
void
EhsBundleMapWithStats::bundle_list(oasys::StringBuffer* buf)
{
    EhsBundleRef bref("bundle_list");
    int cnt = 0;

    lock_.lock("bundle_list");
    Iterator iter = list_.begin();
    lock_.unlock();

    while (iter != list_.end()) {
        bref = iter->second;
        bref->bundle_list(buf);

        ++cnt;

        lock_.lock("bundle_list");
        ++iter;
        lock_.unlock();
    }

    if (0 == cnt) {
        buf->append("Bundle list: No bundles\n\n");
    }
}

//----------------------------------------------------------------------
void
EhsBundleMapWithStats::bundle_id_list(EhsBundleMap& bidlist, uint64_t source_node_id, uint64_t dest_node_id)
{
    bidlist.clear();

    EhsBundleRef bref("bundle_id_list");

    lock_.lock("bundle_id_list");
    Iterator iter = list_.begin();
    lock_.unlock();

    while (iter != list_.end()) {
        bref = iter->second;

        if (bref->ipn_source_node() == source_node_id && bref->ipn_dest_node() == dest_node_id) {
            bidlist.insert(EhsBundlePair(bref->bundleid(), bref));
        }

        lock_.lock("bundle_id_list");
        ++iter;
        lock_.unlock();
    }
}

//----------------------------------------------------------------------
void
EhsBundleMapWithStats::bundle_id_list(EhsBundleMap& bidlist)
{
    bidlist.clear();

    EhsBundleRef bref("bundle_id_list");

    lock_.lock("bundle_id_list");
    Iterator iter = list_.begin();
    lock_.unlock();

    while (iter != list_.end()) {
        bref = iter->second;

        bidlist.insert(EhsBundlePair(bref->bundleid(), bref));

        lock_.lock("bundle_id_list");
        ++iter;
        lock_.unlock();
    }
}

//----------------------------------------------------------------------
void
EhsBundleMapWithStats::bundle_stats(oasys::StringBuffer* buf, 
                                    uint64_t undelivered, uint64_t unrouted)
{
    buf->appendf("Pending: %" PRIu64 " Custody: %" PRIu64 " Undelivered: %" PRIu64
                 " Unrouted: %" PRIu64 " Received: %" PRIu64 " Transmitted: %" PRIu64
                 " XmtFailed: %" PRIu64 " Delivered: %" PRIu64 " Rejected: %" PRIu64,
                 list_.size(), total_custody_, 
                 undelivered, unrouted,
                 total_received_, total_transmitted_, total_transmit_failed_,
                 total_delivered_, total_rejected_);
}

//----------------------------------------------------------------------
void
EhsBundleMapWithStats::get_bundle_stats(uint64_t* received, uint64_t* transmitted, uint64_t* transmit_failed,
                                        uint64_t* delivered, uint64_t* rejected,
                                        uint64_t* pending, uint64_t* custody)
{
    *received = total_received_;
    *transmitted = total_transmitted_;
    *transmit_failed = total_transmit_failed_;
    *delivered = total_delivered_;
    *rejected = total_rejected_;
    *pending = list_.size();
    *custody = total_custody_;
}

//----------------------------------------------------------------------
void
EhsBundleMapWithStats::dump(oasys::StringBuffer* buf)
{
    (void) buf;
//    oasys::ScopeLock l(&lock_, __FUNCTION__);

//    if (!list_.empty()) {
//        Iterator iter = list_.begin();
//        while (iter != list_.end()) {
//            EhsBundlePriorityQueue* blist = iter->second;
//    
//            buf->appendf("    %" PRIu64 " -> %" PRIu64 "  # bundles: %zu  bytes: %" PRIu64 "\n", 
//                         blist->source_node_id_, blist->dest_node_id_, 
//                         blist->size(), blist->total_bytes_);
//            ++iter;
//        }
//    }
//
//    buf->appendf("\n    Total Bundles: %" PRIu64 "  Total Bytes: %" PRIu64 "\n",
//                 total_pending_, total_bytes_);
}

//----------------------------------------------------------------------
void
EhsBundleMapWithStats::bundle_stats_by_src_dst(int* count, EhsBundleStats** stats)
{
    EhsBundleStats* one_stat = NULL;

    oasys::ScopeLock l(&lock_, __FUNCTION__);

    *stats = (EhsBundleStats*) realloc(*stats, (*count + stats_map_.size()) * sizeof(EhsBundleStats));

    StatsIterator siter = stats_map_.begin();
    while (siter != stats_map_.end()) {
        one_stat = siter->second;
        
        (*stats)[*count].source_node_id_ = one_stat->source_node_id_;
        (*stats)[*count].dest_node_id_ = one_stat->dest_node_id_;

        (*stats)[*count].total_received_ = one_stat->total_received_;
        (*stats)[*count].total_transmitted_ = one_stat->total_transmitted_;
        (*stats)[*count].total_transmit_failed_ = one_stat->total_transmit_failed_;
        (*stats)[*count].total_rejected_ = one_stat->total_rejected_;
        (*stats)[*count].total_delivered_ = one_stat->total_delivered_;
        (*stats)[*count].total_expired_ = one_stat->total_expired_;

        (*stats)[*count].total_pending_ = one_stat->total_pending_;
        (*stats)[*count].total_custody_ = one_stat->total_custody_;
        (*stats)[*count].total_bytes_ = one_stat->total_bytes_;

        (*stats)[*count].total_expedited_rcv_ = one_stat->total_expedited_rcv_;
        (*stats)[*count].total_expedited_xmt_ = one_stat->total_expedited_xmt_;
        (*stats)[*count].total_ttl_abuse_ = one_stat->total_ttl_abuse_;

        (*stats)[*count].total_pending_bulk_ = one_stat->total_pending_bulk_;
        (*stats)[*count].total_pending_normal_ = one_stat->total_pending_normal_;
        (*stats)[*count].total_pending_expedited_ = one_stat->total_pending_expedited_;
        (*stats)[*count].total_pending_reserved_ = one_stat->total_pending_reserved_;

        (*stats)[*count].interval_stats_ = NULL;

        *count += 1;

        ++siter;
    }
}

//----------------------------------------------------------------------
void
EhsBundleMapWithStats::fwdlink_interval_stats(int* count, EhsFwdLinkIntervalStats** stats)
{
    EhsBundleStats* one_stat = NULL;

    oasys::ScopeLock l(&lock_, __FUNCTION__);

    *stats = (EhsFwdLinkIntervalStats*) realloc(*stats, (*count + stats_map_.size()) * sizeof(EhsFwdLinkIntervalStats));

    StatsIterator siter = stats_map_.begin();
    while (siter != stats_map_.end()) {
        one_stat = siter->second;

        if (NULL != one_stat->interval_stats_ && one_stat->interval_stats_->bundles_transmitted_ > 0) {
            (*stats)[*count].source_node_id_ = one_stat->source_node_id_;
            (*stats)[*count].dest_node_id_ = one_stat->dest_node_id_;

            (*stats)[*count].bundles_received_ = one_stat->interval_stats_->bundles_received_;
            (*stats)[*count].bytes_received_ = one_stat->interval_stats_->bytes_received_;
            (*stats)[*count].bundles_transmitted_ = one_stat->interval_stats_->bundles_transmitted_;
            (*stats)[*count].bytes_transmitted_ = one_stat->interval_stats_->bytes_transmitted_;

            one_stat->interval_stats_->bundles_received_ = 0;
            one_stat->interval_stats_->bytes_received_ = 0;
            one_stat->interval_stats_->bundles_transmitted_ = 0;
            one_stat->interval_stats_->bytes_transmitted_ = 0;

            *count += 1;
        }

        ++siter;
    }

    // Trim allocated space to remove unused space
    *stats = (EhsFwdLinkIntervalStats*) realloc(*stats, (*count * sizeof(EhsFwdLinkIntervalStats)));
}




/***********************************************************************
 *  EhsBundleTree methods
 ***********************************************************************/

//----------------------------------------------------------------------
EhsBundleTree::EhsBundleTree()
{
    total_pending_ = 0;
    total_custody_ = 0;
    total_bytes_   = 0;

    total_received_ = 0;
    total_transmitted_ = 0;
    total_rejected_ = 0;
    total_delivered_ = 0;
    total_expired_ = 0;

    total_expedited_rcv_ = 0;
    total_expedited_xmt_ = 0;

    total_pending_bulk_ = 0;
    total_pending_normal_ = 0;
    total_pending_expedited_ = 0;
    total_pending_reserved_ = 0;
}

//----------------------------------------------------------------------
EhsBundleTree::~EhsBundleTree()
{
}

//----------------------------------------------------------------------
EhsBundlePriorityQueue* 
EhsBundleTree::extract_bundle_list(uint64_t source_node_id, uint64_t dest_node_id)
{
    EhsBundlePriorityQueue* blist = NULL;

    EhsSrcDstKey key(source_node_id, dest_node_id);

    oasys::ScopeLock l(&lock_, __FUNCTION__);

    Iterator iter = list_.find(key);
    if (iter != list_.end()) {
        blist = iter->second;

        dec_stats(blist);

        list_.erase(iter);
    }

    return blist;
}

//----------------------------------------------------------------------
void
EhsBundleTree::insert_bundle(EhsBundleRef& bref)
{
    EhsSrcDstKey key(bref->ipn_source_node(), bref->ipn_dest_node());

    oasys::ScopeLock l(&lock_, __FUNCTION__);

    EhsBundlePriorityQueue* blist = NULL;

    Iterator iter = list_.find(key);
    if (iter != list_.end()) {
        blist = iter->second;
    } else {
        blist = new EhsBundlePriorityQueue(bref->ipn_source_node(), bref->ipn_dest_node());
        list_[key] = blist;
    }

    blist->insert_bundle(bref);

    inc_stats(bref);
}

//----------------------------------------------------------------------
void 
EhsBundleTree::insert_queue(EhsBundlePriorityQueue* queue)
{
    EhsSrcDstKey key(queue->source_node_id_, queue->dest_node_id_);

    oasys::ScopeLock l(&lock_, __FUNCTION__);

    EhsBundlePriorityQueue* blist = NULL;

    // quick bump the stats at the queue level
    inc_stats(queue);

    Iterator iter = list_.find(key);
    if (iter != list_.end()) {
        blist = iter->second;

        //XXX/dz TODO: Check to see which is bigger and use it as the master??
        EhsBundleRef bref("insert_queue");
        bref = queue->pop_bundle();
        while (bref != NULL) {
            blist->insert_bundle(bref);
            bref = queue->pop_bundle();
        }

        // all bundles removed from this queue so it can be deleted
        delete queue;
    } else {
        list_[key] = queue;
    }


}

//----------------------------------------------------------------------
uint64_t
EhsBundleTree::route_to_link(EhsLink* el, EhsSrcDstWildBoolMap& fwdlink_xmt_enabled)
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    uint64_t num_routed_bundles = 0;

    bool okay_to_route;

    Iterator del_iter;
    Iterator iter = list_.begin();
    while (iter != list_.end()) {
        EhsBundlePriorityQueue* blist = iter->second;

        okay_to_route = false;
        if (!el->is_fwdlnk() ||
            fwdlink_xmt_enabled.check_pair(blist->source_node_id_, blist->dest_node_id_)) {
            okay_to_route = el->is_node_reachable(blist->dest_node_id_);
        }

        if (okay_to_route) {   
            del_iter = iter;
            ++iter;
            
            num_routed_bundles += blist->size();

            dec_stats(blist);

            el->post_event(new EhsRouteBundleListReq(blist));

            list_.erase(del_iter);
        } else {
            ++iter;
        }
    }

    return num_routed_bundles;
}

//----------------------------------------------------------------------
void
EhsBundleTree::clear()
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    Iterator iter = list_.begin();
    while (iter != list_.end()) {
        EhsBundlePriorityQueue* blist = iter->second;
        delete blist;
        list_.erase(iter);

        iter = list_.begin();
    }

    total_pending_ = 0;
    total_custody_ = 0;
    total_bytes_ = 0;

    total_received_ = 0;
    total_transmitted_ = 0;
    total_rejected_ = 0;
    total_delivered_ = 0;
    total_expired_ = 0;

    total_expedited_rcv_ = 0;
    total_expedited_xmt_ = 0;

    total_pending_bulk_ = 0;
    total_pending_normal_ = 0;
    total_pending_expedited_ = 0;
    total_pending_reserved_ = 0;
}


//----------------------------------------------------------------------
size_t
EhsBundleTree::size() const
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);
    return total_pending_;
}

//----------------------------------------------------------------------
bool
EhsBundleTree::empty() const
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);
    return (0 == total_pending_);
}

//----------------------------------------------------------------------
void
EhsBundleTree::inc_stats(EhsBundleRef& bref)
{
    total_pending_ += 1;
    total_bytes_   += bref->length();

    if (bref->is_cos_bulk()) {
        ++total_pending_bulk_;
    } else if (bref->is_cos_normal()) {
        ++total_pending_normal_;
    } else if (bref->is_cos_expedited()) {
        ++total_pending_expedited_;
        ++total_expedited_rcv_;
    } else {
        ++total_pending_reserved_;
    }
}

//----------------------------------------------------------------------
void
EhsBundleTree::inc_stats(EhsBundlePriorityQueue* blist)
{
    total_pending_ += blist->size();
    total_bytes_ += blist->total_bytes_;

    total_pending_bulk_ += blist->total_pending_bulk_;
    total_pending_normal_ += blist->total_pending_normal_;
    total_pending_expedited_ += blist->total_pending_expedited_;
    total_expedited_rcv_ += blist->total_expedited_rcv_;
    total_expedited_xmt_ += blist->total_expedited_xmt_;
    total_pending_reserved_ += blist->total_pending_reserved_;
}

//----------------------------------------------------------------------
void
EhsBundleTree::dec_stats(EhsBundlePriorityQueue* blist)
{
    total_pending_ -= blist->size();
    total_bytes_ -= blist->total_bytes_;

    total_pending_bulk_ -= blist->total_pending_bulk_;
    total_pending_normal_ -= blist->total_pending_normal_;
    total_pending_expedited_ -= blist->total_pending_expedited_;
    total_expedited_rcv_ -= blist->total_expedited_rcv_;
    total_expedited_xmt_ -= blist->total_expedited_xmt_;
    total_pending_reserved_ -= blist->total_pending_reserved_;
}

//----------------------------------------------------------------------
void
EhsBundleTree::inc_received(EhsBundleRef& bref)
{
    EhsSrcDstKey key(bref->ipn_source_node(), bref->ipn_dest_node());
    EhsBundlePriorityQueue* blist = NULL;

    oasys::ScopeLock l(&lock_, __FUNCTION__);

    Iterator iter = list_.find(key);
    if (iter != list_.end()) {
        blist = iter->second;
        blist->inc_received();
    }

    ++total_received_;
}

//----------------------------------------------------------------------
void
EhsBundleTree::inc_transmitted(EhsBundleRef& bref)
{
    EhsSrcDstKey key(bref->ipn_source_node(), bref->ipn_dest_node());
    EhsBundlePriorityQueue* blist = NULL;

    oasys::ScopeLock l(&lock_, __FUNCTION__);

    Iterator iter = list_.find(key);
    if (iter != list_.end()) {
        blist = iter->second;
        blist->inc_transmitted();
    }

    ++total_transmitted_;
}

//----------------------------------------------------------------------
void
EhsBundleTree::inc_expired(EhsBundleRef& bref)
{
    EhsSrcDstKey key(bref->ipn_source_node(), bref->ipn_dest_node());
    EhsBundlePriorityQueue* blist = NULL;

    oasys::ScopeLock l(&lock_, __FUNCTION__);

    Iterator iter = list_.find(key);
    if (iter != list_.end()) {
        blist = iter->second;
        blist->inc_expired();
    }

    ++total_expired_;
}

//----------------------------------------------------------------------
void
EhsBundleTree::inc_delivered(EhsBundleRef& bref)
{
    EhsSrcDstKey key(bref->ipn_source_node(), bref->ipn_dest_node());
    EhsBundlePriorityQueue* blist = NULL;

    oasys::ScopeLock l(&lock_, __FUNCTION__);

    Iterator iter = list_.find(key);
    if (iter != list_.end()) {
        blist = iter->second;
        blist->inc_delivered();
    }

    ++total_delivered_;
}

//----------------------------------------------------------------------
void
EhsBundleTree::inc_rejected(EhsBundleRef& bref)
{
    EhsSrcDstKey key(bref->ipn_source_node(), bref->ipn_dest_node());
    EhsBundlePriorityQueue* blist = NULL;

    oasys::ScopeLock l(&lock_, __FUNCTION__);

    Iterator iter = list_.find(key);
    if (iter != list_.end()) {
        blist = iter->second;
        blist->inc_rejected();
    }

    ++total_rejected_;
}

//----------------------------------------------------------------------
void
EhsBundleTree::inc_custody(EhsBundle* eb)
{
    EhsSrcDstKey key(eb->ipn_source_node(), eb->ipn_dest_node());
    EhsBundlePriorityQueue* blist = NULL;

    oasys::ScopeLock l(&lock_, __FUNCTION__);

    Iterator iter = list_.find(key);
    if (iter != list_.end()) {
        blist = iter->second;
        blist->inc_custody();
    }

    ++total_custody_;
}

//----------------------------------------------------------------------
void
EhsBundleTree::dec_custody(EhsBundle* eb)
{
    EhsSrcDstKey key(eb->ipn_source_node(), eb->ipn_dest_node());
    EhsBundlePriorityQueue* blist = NULL;

    oasys::ScopeLock l(&lock_, __FUNCTION__);

    Iterator iter = list_.find(key);
    if (iter != list_.end()) {
        blist = iter->second;
        blist->dec_custody();
    }

    --total_custody_;
}




//----------------------------------------------------------------------
void
EhsBundleTree::dump(oasys::StringBuffer* buf)
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    if (!list_.empty()) {
        Iterator iter = list_.begin();
        while (iter != list_.end()) {
            EhsBundlePriorityQueue* blist = iter->second;
    
            buf->appendf("    %" PRIu64 " -> %" PRIu64 "  # bundles: %zu  bytes: %" PRIu64 "\n", 
                         blist->source_node_id_, blist->dest_node_id_, 
                         blist->size(), blist->total_bytes_);
            ++iter;
        }
    }

    buf->appendf("\n    Total Bundles: %" PRIu64 "  Total Bytes: %" PRIu64 "\n",
                 total_pending_, total_bytes_);
}




/***********************************************************************
 *  NodePriorityMap methods
 ***********************************************************************/

//----------------------------------------------------------------------
NodePriorityMap::NodePriorityMap()
{
    default_priority_ = 500;
}

//----------------------------------------------------------------------
NodePriorityMap::~NodePriorityMap()
{
    clear();
}


//----------------------------------------------------------------------
void
NodePriorityMap::clear()
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);
    priority_map_.clear();
}

//----------------------------------------------------------------------
size_t
NodePriorityMap::size() const
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);
    return priority_map_.size();
}


//----------------------------------------------------------------------
bool
NodePriorityMap::empty() const
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);
    return priority_map_.empty();
}


//----------------------------------------------------------------------
void
NodePriorityMap::set_default_priority(int priority)
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);
    default_priority_ = priority;
}

//----------------------------------------------------------------------
void
NodePriorityMap::set_node_priority(uint64_t node_id, int priority)
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);
    priority_map_[node_id] = priority;
}

//----------------------------------------------------------------------
int
NodePriorityMap::get_node_priority(uint64_t node_id)
{
    int result = default_priority_;

    oasys::ScopeLock l(&lock_, __FUNCTION__);

    Iterator iter = priority_map_.find(node_id);
    if (iter != priority_map_.end()) {
        result = iter->second;
    }

    return result;
}

//----------------------------------------------------------------------
void
NodePriorityMap::dump(oasys::StringBuffer* buf)
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    if (!priority_map_.empty()) {
        Iterator iter = priority_map_.begin();
        while (iter != priority_map_.end()) {
            buf->appendf("    %" PRIu64 " : %d\n", iter->first, iter->second);
            ++iter;
        }
    }

    buf->appendf("    Default priority: %d\n", default_priority_);
}


/***********************************************************************
 *  EhsBundlePriorityTree methods
 ***********************************************************************/

//----------------------------------------------------------------------
EhsBundlePriorityTree::EhsBundlePriorityTree()
{
    total_pending_ = 0;
    total_bytes_   = 0;

    total_pending_bulk_ = 0;
    total_pending_normal_ = 0;
    total_pending_expedited_ = 0;
    total_pending_reserved_ = 0;
}

//----------------------------------------------------------------------
EhsBundlePriorityTree::~EhsBundlePriorityTree()
{
    clear();
}

//----------------------------------------------------------------------
char*
EhsBundlePriorityTree::build_key(EhsBundlePriorityQueue* blist)
{
    // Insert the bundle list into the priority map
    int rev_src_priority = 999 - src_priority_.get_node_priority(blist->source_node_id_);
    int rev_dst_priority = 999 - dst_priority_.get_node_priority(blist->dest_node_id_);
        
    snprintf(key_buf_, sizeof(key_buf_), "%3.3d~%3.3d~%s",
             rev_src_priority, rev_dst_priority, blist->first_bundle_priority().c_str());
    return key_buf_;
}


//----------------------------------------------------------------------
void 
EhsBundlePriorityTree::set_src_node_priority(uint64_t node_id, int priority)
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);
    
    EhsBundlePriorityQueue* blist = NULL;

    int cnt = 0;

    // remove any priority map entry that we are about to change
    PriorityIterator del_priority_iter;
    PriorityIterator priority_iter = priority_map_.begin();
    while (priority_iter != priority_map_.end()) {
        blist = priority_iter->second;
        if (blist->source_node_id_ == node_id) {
            del_priority_iter = priority_iter;
            ++priority_iter;
            ++cnt;
        } else {
            ++priority_iter;
        }
    }

    // the source node ID's priority
    if (priority > 999) priority = 999;
    if (priority < 0) priority = 0;
    src_priority_.set_node_priority(node_id, priority);

    // reinsert the changed entries into the prority map    
    if (0 != cnt) {
        std::string pkey;
        SrcDstIterator src_dst_iter = src_dst_map_.begin();
        while (src_dst_iter != src_dst_map_.end()) {
            blist = src_dst_iter->second;

            if (blist->source_node_id_ == node_id && !blist->empty()) {
                pkey = build_key(blist);
                priority_map_.insert(PriorityPair(pkey, blist));
            }

            ++ src_dst_iter;
        }
    }
}

//----------------------------------------------------------------------
void 
EhsBundlePriorityTree::set_dst_node_priority(uint64_t node_id, int priority)
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);
    
    EhsBundlePriorityQueue* blist = NULL;

    int cnt = 0;

    // remove any priority map entry that we are about to change
    PriorityIterator del_priority_iter;
    PriorityIterator priority_iter = priority_map_.begin();
    while (priority_iter != priority_map_.end()) {
        blist = priority_iter->second;
        if (blist->dest_node_id_ == node_id) {
            del_priority_iter = priority_iter;
            ++priority_iter;
            ++cnt;
        } else {
            ++priority_iter;
        }
    }

    // the source node ID's priority
    if (priority > 999) priority = 999;
    if (priority < 0) priority = 0;
    dst_priority_.set_node_priority(node_id, priority);

    // reinsert the changed entries into the prority map    
    if (0 != cnt) {
        std::string pkey;
        SrcDstIterator src_dst_iter = src_dst_map_.begin();
        while (src_dst_iter != src_dst_map_.end()) {
            blist = src_dst_iter->second;

            if (blist->dest_node_id_ == node_id && !blist->empty()) {
                pkey = build_key(blist);
                priority_map_.insert(PriorityPair(pkey, blist));
            }

            ++ src_dst_iter;
        }
    }
}

//----------------------------------------------------------------------
void
EhsBundlePriorityTree::insert_bundle(EhsBundleRef& bref)
{
    std::string pkey;
    EhsBundlePriorityQueue* blist = NULL;

    EhsSrcDstKey key(bref->ipn_source_node(), bref->ipn_dest_node());

    oasys::ScopeLock l(&lock_, __FUNCTION__);

    SrcDstIterator src_dst_iter = src_dst_map_.find(key);
    if (src_dst_iter != src_dst_map_.end()) {
        blist = src_dst_iter->second;

        if (!blist->empty()) {
            pkey = build_key(blist);
            PriorityIterator priority_iter = priority_map_.find(pkey);
            if (priority_iter != priority_map_.end()) {
                priority_map_.erase(priority_iter);
            }
        }
    } else {
        blist = new EhsBundlePriorityQueue(bref->ipn_source_node(), bref->ipn_dest_node());
        src_dst_map_[key] = blist;
    }

    if (blist->insert_bundle(bref)) {
        inc_stats(bref);
//fprintf(stderr, "PriorityTree - insert bundle: %" PRIbid "\n", bref->bundleid()); fflush(stderr); //dz debug
    }

    pkey = build_key(blist);
    priority_map_.insert(PriorityPair(pkey, blist));

//dz debug
//fprintf(stderr, "EhsBundlePriorityTree [%" PRIu64 "-%" PRIu64 "] - insert_bundle - blist.size: %zu pending: %" PRIu64 " -- next bundle: %s\n",
//        blist->source_node_id_, blist->dest_node_id_, blist->size(), total_pending_, pkey.c_str());
//fflush(stderr);


}

//----------------------------------------------------------------------
void 
EhsBundlePriorityTree::insert_queue(EhsBundlePriorityQueue* queue)
{
    std::string pkey;
    EhsBundlePriorityQueue* blist = NULL;

    EhsSrcDstKey key(queue->source_node_id_, queue->dest_node_id_);

    oasys::ScopeLock l(&lock_, __FUNCTION__);

    // quick bump the stats at the queue level
    //XXX/dz - could be duplicates in the lists---     inc_stats(queue);

    SrcDstIterator src_dst_iter = src_dst_map_.find(key);
    if (src_dst_iter != src_dst_map_.end()) {
        blist = src_dst_iter->second;

        if (blist->empty()) {
            delete blist;
            src_dst_iter->second = queue;
            blist = queue;
        } else {
            pkey = build_key(blist);

            PriorityIterator priority_iter = priority_map_.find(pkey);
            if (priority_iter != priority_map_.end()) {
                priority_map_.erase(priority_iter);
            }
            
            EhsBundleRef bref("insert_queue");
            bref = queue->pop_bundle();
            while (bref != NULL) {
                if (blist->insert_bundle(bref)) {
                    inc_stats(bref);
                }
                bref = queue->pop_bundle();
            }

            // all bundles removed from this queue so it can be deleted
            delete queue;
        }
    } else {
        src_dst_map_[key] = queue;
        blist = queue;
        inc_stats(blist);
    }

    // Insert the bundle list into the priority map
    pkey = build_key(blist);
    priority_map_.insert(PriorityPair(pkey, blist));

//dz debug
//fprintf(stderr, "EhsBundlePriorityTree [%" PRIu64 "-%" PRIu64 "] - insert_queue - blist.size: %zu pending: %" PRIu64 " -- next bundle: %s\n",
//        blist->source_node_id_, blist->dest_node_id_, blist->size(), total_pending_, pkey.c_str());
//fflush(stderr);

}

//----------------------------------------------------------------------
EhsBundleRef
EhsBundlePriorityTree::pop_bundle()
{
    EhsBundleRef bref("EhsBundlePriorityTree::pop_bundle");
    EhsBundlePriorityQueue* blist = NULL;
    std::string pkey = "";

    oasys::ScopeLock l(&lock_, __FUNCTION__);

    PriorityIterator priority_iter = priority_map_.begin();
    if (priority_iter != priority_map_.end()) {
        blist = priority_iter->second;
        priority_map_.erase(priority_iter);

        bref = blist->pop_bundle();

        if (bref != NULL) {
            dec_stats(bref);
        }

        // If the bundle list still has bundles then 
        // add it back into the priority mapping with its new key
        if (!blist->empty()) {
            std::string pkey = build_key(blist);
            priority_map_.insert(PriorityPair(pkey, blist));
        }

//dz debug
//fprintf(stderr, "EhsBundlePriorityTree [%" PRIu64 "-%" PRIu64 "] - pop_bundle - blist.size: %zu pending: %" PRIu64 " -- next bundle: %s\n",
//        blist->source_node_id_, blist->dest_node_id_, blist->size(), total_pending_, pkey.c_str());
//fflush(stderr);

    }

    return bref;
}

//----------------------------------------------------------------------
uint64_t
EhsBundlePriorityTree::return_disabled_bundles(EhsBundleTree& tree, 
                                               EhsSrcDstWildBoolMap& fwdlink_xmt_enabled)
{
    uint64_t num_returned = 0;
    std::string pkey = "";

    oasys::ScopeLock l(&lock_, __FUNCTION__);

    SrcDstIterator del_src_dst_iter = src_dst_map_.begin();
    SrcDstIterator src_dst_iter = src_dst_map_.begin();
    while (src_dst_iter != src_dst_map_.end()) {
        EhsBundlePriorityQueue* blist = src_dst_iter->second;
        if (!fwdlink_xmt_enabled.check_pair(blist->source_node_id_, blist->dest_node_id_)) {

            if (!blist->empty()) {
                pkey = build_key(blist);

                PriorityIterator priority_iter = priority_map_.find(pkey);
                if (priority_iter != priority_map_.end()) {
                    priority_map_.erase(priority_iter);
                }

                num_returned += blist->total_pending_;
            
                dec_stats(blist);

//dz debug
//fprintf(stderr, "EhsBundlePriorityTree [%" PRIu64 "-%" PRIu64 "] - return_disabled_bundles - blist.size: %zu pending: %" PRIu64 "\n",
//        blist->source_node_id_, blist->dest_node_id_, blist->size(), total_pending_);
//fflush(stderr);

                tree.insert_queue(blist);
            } else {
                delete blist;
            }

            del_src_dst_iter = src_dst_iter;
            ++src_dst_iter;

            src_dst_map_.erase(del_src_dst_iter);
        } else {
            ++src_dst_iter;
        }
    }

    return num_returned;    
}

//----------------------------------------------------------------------
uint64_t
EhsBundlePriorityTree::return_all_bundles(EhsBundleTree& tree)
{
    uint64_t num_returned = 0;

    oasys::ScopeLock l(&lock_, __FUNCTION__);

    priority_map_.clear();

    SrcDstIterator src_dst_iter = src_dst_map_.begin();
    while (src_dst_iter != src_dst_map_.end()) {
        EhsBundlePriorityQueue* blist = src_dst_iter->second;

        if (!blist->empty()) {
            num_returned += blist->total_pending_;
        
            dec_stats(blist);

//dz debug
//fprintf(stderr, "EhsBundlePriorityTree [%" PRIu64 "-%" PRIu64 "] - return_all_bundles - blist.size: %zu ,pending: %" PRIu64 "\n",
//        blist->source_node_id_, blist->dest_node_id_, blist->size(), total_pending_);
//fflush(stderr);

            tree.insert_queue(blist);
        } else {
            delete blist;
        }

        src_dst_map_.erase(src_dst_iter);

        src_dst_iter = src_dst_map_.begin();
    }

    return num_returned;    
}

//----------------------------------------------------------------------
void
EhsBundlePriorityTree::clear()
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    priority_map_.clear();

    SrcDstIterator src_dst_iter = src_dst_map_.begin();
    while (src_dst_iter != src_dst_map_.end()) {
        EhsBundlePriorityQueue* blist = src_dst_iter->second;
        blist->clear();
        delete blist;
        src_dst_map_.erase(src_dst_iter);

        src_dst_iter = src_dst_map_.begin();
    }

    total_pending_ = 0;
    total_bytes_ = 0;

    total_pending_bulk_ = 0;
    total_pending_normal_ = 0;
    total_pending_expedited_ = 0;
    total_pending_reserved_ = 0;
}


//----------------------------------------------------------------------
size_t
EhsBundlePriorityTree::size() const
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);
    return total_pending_;
}

//----------------------------------------------------------------------
bool
EhsBundlePriorityTree::empty() const
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);
    return (0 == total_pending_);
}

//----------------------------------------------------------------------
void
EhsBundlePriorityTree::inc_stats(EhsBundleRef& bref)
{
    total_pending_ += 1;
    total_bytes_   += bref->length();

    if (bref->is_cos_bulk()) {
        ++total_pending_bulk_;
    } else if (bref->is_cos_normal()) {
        ++total_pending_normal_;
    } else if (bref->is_cos_expedited()) {
        ++total_pending_expedited_;
    } else {
        ++total_pending_reserved_;
    }
}

//----------------------------------------------------------------------
void
EhsBundlePriorityTree::dec_stats(EhsBundleRef& bref)
{
    total_pending_ -= 1;
    total_bytes_   -= bref->length();

    if (bref->is_cos_bulk()) {
        --total_pending_bulk_;
    } else if (bref->is_cos_normal()) {
        --total_pending_normal_;
    } else if (bref->is_cos_expedited()) {
        --total_pending_expedited_;
    } else {
        --total_pending_reserved_;
    }
}

//----------------------------------------------------------------------
void
EhsBundlePriorityTree::inc_stats(EhsBundlePriorityQueue* blist)
{
    total_pending_ += blist->size();
    total_bytes_ += blist->total_bytes_;

    total_pending_bulk_ += blist->total_pending_bulk_;
    total_pending_normal_ += blist->total_pending_normal_;
    total_pending_expedited_ += blist->total_pending_expedited_;
    total_pending_reserved_ += blist->total_pending_reserved_;
}

//----------------------------------------------------------------------
void
EhsBundlePriorityTree::dec_stats(EhsBundlePriorityQueue* blist)
{
    total_pending_ -= blist->size();
    total_bytes_ -= blist->total_bytes_;

    total_pending_bulk_ -= blist->total_pending_bulk_;
    total_pending_normal_ -= blist->total_pending_normal_;
    total_pending_expedited_ -= blist->total_pending_expedited_;
    total_pending_reserved_ -= blist->total_pending_reserved_;
}

//----------------------------------------------------------------------
void
EhsBundlePriorityTree::dump(oasys::StringBuffer* buf)
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    if (!src_dst_map_.empty()) {
        SrcDstIterator src_dst_iter = src_dst_map_.begin();
        while (src_dst_iter != src_dst_map_.end()) {
            EhsBundlePriorityQueue* blist = src_dst_iter->second;
    
            buf->appendf("    %" PRIu64 " -> %" PRIu64 "  # bundles: %zu  bytes: %" PRIu64 "\n", 
                         blist->source_node_id_, blist->dest_node_id_, 
                         blist->size(), blist->total_bytes_);
            ++src_dst_iter;
        }
    }

    buf->appendf("\n    Total Bundles: %" PRIu64 "  Total Bytes: %" PRIu64 "\n",
                 total_pending_, total_bytes_);
}


/***********************************************************************
 *  EhsBundleList methods
 ***********************************************************************/

//----------------------------------------------------------------------
EhsBundleList::EhsBundleList()
{
}

//----------------------------------------------------------------------
EhsBundleList::~EhsBundleList()
{
    clear();
}

//----------------------------------------------------------------------
bool
EhsBundleList::insert_bundle(EhsBundleRef& bref)
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    EhsBundleStrResult result = list_.insert(EhsBundleStrPair(bref->gbofid_str(), bref));

    return result.second;
}

//----------------------------------------------------------------------
void
EhsBundleList::delete_bundle(std::string& key)
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    list_.erase(key);
}

//----------------------------------------------------------------------
void
EhsBundleList::clear()
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);

    list_.clear();
}

//----------------------------------------------------------------------
void
EhsBundleList::clear_expired_bundles()
{
    EhsBundleRef bref(__FUNCTION__);

    oasys::ScopeLock l(&lock_, __FUNCTION__);


    EhsBundleStrIterator del_itr;
    EhsBundleStrIterator itr = list_.begin();
    while (itr != list_.end()) {
        bref = itr->second;

        if (bref->expired()) {
            del_itr = itr;
            ++itr;

            list_.erase(del_itr);
        } else {
            ++itr;
        }
    }
}

//----------------------------------------------------------------------
size_t
EhsBundleList::size() const
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);
    return list_.size();
}

//----------------------------------------------------------------------
bool
EhsBundleList::empty() const
{
    oasys::ScopeLock l(&lock_, __FUNCTION__);
    return list_.empty();
}

//----------------------------------------------------------------------
void
EhsBundleList::dump(oasys::StringBuffer* buf)
{
    (void) buf;
    oasys::ScopeLock l(&lock_, __FUNCTION__);

//    buf->appendf("    %" PRIu64 " -> %" PRIu64 "  bundles: %" PRIu64 " bytes: %" PRIu64
//                 " expedited: %" PRIu64 " normal: %" PRIu64 " bulk: %" PRIu64 "\n",
//                 source_node_id_, dest_node_id_, total_pending_, total_bytes_, 
//                 total_pending_expedited_, total_pending_normal_, total_pending_bulk_);
}


} // namespace dtn

#endif // defined(XERCES_C_ENABLED) && defined(EXTERNAL_DP_ENABLED)

#endif // EHSROUTER_ENABLED

