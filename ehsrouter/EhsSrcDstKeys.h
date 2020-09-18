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

#ifndef _EHS_SRC_DST_KEYS_H_
#define _EHS_SRC_DST_KEYS_H_


#ifdef EHSROUTER_ENABLED

#include <map>



namespace dtn {

/***********************************************************************
 *  Source - Destination Combo Key without Wildcards
 ***********************************************************************/
class EhsSrcDstKey
{
public:

    // this class provides an STL <map> "strictly less than" comparison operator
    class mapcompare
    {
    public:
        virtual ~mapcompare() {}

        virtual bool operator() (const EhsSrcDstKey* key1, const EhsSrcDstKey* key2) const
        {
            return key1->lt(key2);
        }

        virtual bool operator() (const EhsSrcDstKey& key1, const EhsSrcDstKey& key2) const
        {
             return key1.lt(key2);
        }

     };

public:
    /**
     * Constructors
     */
    EhsSrcDstKey();
    EhsSrcDstKey(uint64_t source_node_id, uint64_t dest_node_id);

    /**
     * Destructor.
     */
    virtual ~EhsSrcDstKey();

    /**
     * Strictly less than comparison
     */
    virtual bool lt (const EhsSrcDstKey* other) const;
    virtual bool lt (const EhsSrcDstKey& other) const;

    virtual bool operator< (const EhsSrcDstKey* other) const;
    virtual bool operator< (const EhsSrcDstKey& other) const;

    /**
     * Equals comparison
     */
    virtual bool equals (const EhsSrcDstKey* other) const;
    virtual bool equals (const EhsSrcDstKey& other) const;

    virtual bool operator= (const EhsSrcDstKey* other) const;
    virtual bool operator= (const EhsSrcDstKey& other) const;

public:
    uint64_t source_node_id_;
    uint64_t dest_node_id_;

    // convenience stats vars built into the key
    uint64_t total_bundles_;
    uint64_t total_bytes_;
};




/***********************************************************************
 *  Source - Destination Combo Key with Wildcards
 ***********************************************************************/
class EhsSrcDstWildKey
{
public:
    // this class provides an STL <map> "strictly less than" comparison operator
    class mapcompare
    {
    public:
        virtual ~mapcompare() {}
 
        virtual bool operator() (const EhsSrcDstWildKey* key1, const EhsSrcDstWildKey* key2) const
        {
            return key1->lt(key2);
        }

        virtual bool operator() (const EhsSrcDstWildKey& key1, const EhsSrcDstWildKey& key2) const
        {
            return key1.lt(key2);
        }

     };

public:
    /**
     * Constructors
     */
    EhsSrcDstWildKey();
    EhsSrcDstWildKey(uint64_t source_node_id, uint64_t dest_node_id);
    EhsSrcDstWildKey(uint64_t source_node_id, bool wildcard);               // wildcard must be true
    EhsSrcDstWildKey(bool wildcard, uint64_t dest_node_id); // wildcard must be true
    EhsSrcDstWildKey(bool wildcard_source, bool wildcard_dest); // wildcards must be true

    /**
     * Destructor.
     */
    virtual ~EhsSrcDstWildKey();

    /**
     * Strictly less than comparison
     */
    virtual bool lt (const EhsSrcDstWildKey* other) const;
    virtual bool lt (const EhsSrcDstWildKey& other) const;

    virtual bool operator< (const EhsSrcDstWildKey* other) const;
    virtual bool operator< (const EhsSrcDstWildKey& other) const;

    /**
     * Equals comparison
     */
    virtual bool equals (const EhsSrcDstWildKey* other) const;
    virtual bool equals (const EhsSrcDstWildKey& other) const;

    virtual bool operator= (const EhsSrcDstWildKey* other) const;
    virtual bool operator= (const EhsSrcDstWildKey& other) const;

public:
    uint64_t source_node_id_;
    uint64_t dest_node_id_;
    bool     wildcard_source_;
    bool     wildcard_dest_;

    // convenience stats vars built into the key
    uint64_t total_bundles_;
    uint64_t total_bytes_;
};



} // namespace dtn


#endif // EHSROUTER_ENABLED

#endif /* _EHS_SRC_DST_KEYS_H_ */
