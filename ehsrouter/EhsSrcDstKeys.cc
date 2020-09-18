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

#include <inttypes.h>

#include "EhsSrcDstKeys.h"

namespace dtn {

/***********************************************************************
 *  Source - Destination Combo Key without Wildcards
 ***********************************************************************/
//----------------------------------------------------------------------
EhsSrcDstKey::EhsSrcDstKey()
{
    source_node_id_ = 0;
    dest_node_id_   = 0;

    // convenience stats vars built into the key
    total_bundles_ = 0;
    total_bytes_ = 0;
}

//----------------------------------------------------------------------
EhsSrcDstKey::EhsSrcDstKey(uint64_t source_node_id, uint64_t dest_node_id)
{
    source_node_id_ = source_node_id;
    dest_node_id_ = dest_node_id;

    // convenience stats vars built into the key
    total_bundles_ = 0;
    total_bytes_ = 0;
}

//----------------------------------------------------------------------
EhsSrcDstKey::~EhsSrcDstKey()
{
}

//----------------------------------------------------------------------
bool 
EhsSrcDstKey::lt (const EhsSrcDstKey* other) const
{
    bool result = ((source_node_id_ < other->source_node_id_) ||
                   ((source_node_id_ == other->source_node_id_) &&
                    (dest_node_id_ < other->dest_node_id_)));

    return result;
}

//----------------------------------------------------------------------
bool 
EhsSrcDstKey::lt (const EhsSrcDstKey& other) const
{
    return lt(&other);
}

//----------------------------------------------------------------------
bool 
EhsSrcDstKey::operator< (const EhsSrcDstKey* other) const
{
    return lt(other);
}

//----------------------------------------------------------------------
bool 
EhsSrcDstKey::operator< (const EhsSrcDstKey& other) const
{
    return lt(&other);
}

//----------------------------------------------------------------------
bool 
EhsSrcDstKey::equals (const EhsSrcDstKey* other) const
{
    bool result = ((source_node_id_ == other->source_node_id_) &&
                   (dest_node_id_ == other->dest_node_id_));
    
    return result;
}

//----------------------------------------------------------------------
bool 
EhsSrcDstKey::equals (const EhsSrcDstKey& other) const
{
    return equals(&other);
}

//----------------------------------------------------------------------
bool 
EhsSrcDstKey::operator= (const EhsSrcDstKey* other) const
{
    return equals(other);
}

//----------------------------------------------------------------------
bool 
EhsSrcDstKey::operator= (const EhsSrcDstKey& other) const
{
    return equals(&other);
}



/***********************************************************************
 *  Source - Destination Combo Key with Wildcards
 ***********************************************************************/
//----------------------------------------------------------------------
EhsSrcDstWildKey::EhsSrcDstWildKey()
{
    source_node_id_ = 0;
    dest_node_id_ = 0;
    wildcard_source_ = false;
    wildcard_dest_ = false;

    // convenience stats vars built into the key
    total_bundles_ = 0;
    total_bytes_ = 0;
}

//----------------------------------------------------------------------
EhsSrcDstWildKey::EhsSrcDstWildKey(uint64_t source_node_id, uint64_t dest_node_id)
{
    source_node_id_ = source_node_id;
    dest_node_id_ = dest_node_id;
    wildcard_source_ = false;
    wildcard_dest_ = false;
}

//----------------------------------------------------------------------
EhsSrcDstWildKey::EhsSrcDstWildKey(uint64_t source_node_id, bool wildcard)
{
    source_node_id_ = source_node_id;
    dest_node_id_ = 0;
    wildcard_source_ = false;
    wildcard_dest_ = wildcard;  // should be true!
}

//----------------------------------------------------------------------
EhsSrcDstWildKey::EhsSrcDstWildKey(bool wildcard, uint64_t dest_node_id)
{
    source_node_id_ = 0;
    dest_node_id_ = dest_node_id;
    wildcard_source_ = wildcard;  // should be true!
    wildcard_dest_ = false;
}

//----------------------------------------------------------------------
EhsSrcDstWildKey::EhsSrcDstWildKey(bool wildcard_source, bool wildcard_dest)
{
    source_node_id_ = 0;
    dest_node_id_ = 0;
    wildcard_source_ = wildcard_source;  // should be true!
    wildcard_dest_ = wildcard_dest;      // should be true!
}

//----------------------------------------------------------------------
EhsSrcDstWildKey::~EhsSrcDstWildKey()
{
}

//----------------------------------------------------------------------
bool 
EhsSrcDstWildKey::lt (const EhsSrcDstWildKey* other) const
{
    bool result;
    if (wildcard_source_ && wildcard_dest_) {
        result = false; //not less than any possible "other"
    } else if (other->wildcard_source_ && other->wildcard_dest_) {
        result = true; //any non-double-wildcard is less than an "other" double-wildcard
    } else if (wildcard_source_) {
        if (other->wildcard_source_) {
            result = dest_node_id_ < other->dest_node_id_;
        } else {
            result = false; 
        }
    } else if (wildcard_dest_) {
        if (other->wildcard_dest_) {
            result = source_node_id_ < other->source_node_id_;
        } else if (other->wildcard_source_) {
            result = true;
        } else {
            result = source_node_id_ < other->source_node_id_;
        }

    } else if (other->wildcard_source_) {
        result = true; // this non-wildcard source is less then the "other" wildcard source
    } else if (other->wildcard_dest_) {
        result = source_node_id_ < other->source_node_id_;

    } else {
        // no wildcards
        result = ((source_node_id_ < other->source_node_id_) ||
                  ((source_node_id_ == other->source_node_id_) &&
                    (dest_node_id_ < other->dest_node_id_)));
    }

    return result;
}

//----------------------------------------------------------------------
bool 
EhsSrcDstWildKey::lt (const EhsSrcDstWildKey& other) const
{
    return lt(&other);
}

//----------------------------------------------------------------------
bool 
EhsSrcDstWildKey::operator< (const EhsSrcDstWildKey* other) const
{
    return lt(other);
}

//----------------------------------------------------------------------
bool 
EhsSrcDstWildKey::operator< (const EhsSrcDstWildKey& other) const
{
    return lt(&other);
}

//----------------------------------------------------------------------
bool 
EhsSrcDstWildKey::equals (const EhsSrcDstWildKey* other) const
{
    bool result;
    if (wildcard_source_ && wildcard_dest_) {
        result = other->wildcard_source_ && other->wildcard_dest_;
    } else if (other->wildcard_source_ && other->wildcard_dest_) {
        result = false;
    } else if (wildcard_source_) {
        if (other->wildcard_source_) {
             result = dest_node_id_ == other->dest_node_id_;
         } else {
             result = false;
        }
    } else if (wildcard_dest_) {
        if (other->wildcard_dest_) {
            result = source_node_id_ == other->source_node_id_;
        } else {
            result = false; 
        }
    } else {
        // no wildcards
        result = ((source_node_id_ == other->source_node_id_) &&
                  (dest_node_id_ == other->dest_node_id_));
    }
    
    return result;
}

//----------------------------------------------------------------------
bool 
EhsSrcDstWildKey::equals (const EhsSrcDstWildKey& other) const
{
    return equals(&other);
}

//----------------------------------------------------------------------
bool 
EhsSrcDstWildKey::operator= (const EhsSrcDstWildKey* other) const
{
    return equals(other);
}

//----------------------------------------------------------------------
bool 
EhsSrcDstWildKey::operator= (const EhsSrcDstWildKey& other) const
{
    return equals(&other);
}

} // namespace dtn


#endif // EHSROUTER_ENABLED
