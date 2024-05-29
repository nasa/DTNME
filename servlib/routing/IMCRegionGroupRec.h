/*
 *    Copyright 2022 United States Government as represented by NASA
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

#ifndef _IMC_REGION_GROUP_REC_H_
#define _IMC_REGION_GROUP_REC_H_

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <third_party/oasys/serialize/Serialize.h>

#include <map>
#include <mutex>
#include <memory>

namespace dtn {

class IMCRegionGroupRec;


typedef std::shared_ptr<IMCRegionGroupRec> SPtr_IMCRegionGroupRec;

typedef std::map<size_t, SPtr_IMCRegionGroupRec> IMC_REGION_MAP;       /// key = node number
typedef std::shared_ptr<IMC_REGION_MAP> SPtr_IMC_REGION_MAP;
typedef std::map<size_t, SPtr_IMC_REGION_MAP> MAP_OF_IMC_REGION_MAPS;  /// key = region number

typedef std::map<size_t, SPtr_IMCRegionGroupRec> IMC_GROUP_MAP;        /// key = node number
typedef std::shared_ptr<IMC_GROUP_MAP> SPtr_IMC_GROUP_MAP;
typedef std::map<size_t, SPtr_IMC_GROUP_MAP> MAP_OF_IMC_GROUP_MAPS;    /// key = group number

typedef std::map<std::string, SPtr_IMCRegionGroupRec> IMCRecsMap;  /// key = text key from the record
typedef std::pair<std::string, SPtr_IMCRegionGroupRec> IMCRecsPair;
typedef IMCRecsMap::iterator IMCRecsIterator;
typedef std::pair<IMCRecsIterator, bool> IMCRecsInsertResult;


/**
 * IMC Region or Group Record
 */
class IMCRegionGroupRec : public oasys::SerializableObject {
public:


    /**
     * Constructor
     */
    IMCRegionGroupRec(ssize_t rec_type=0);

    /**
     * Constructor for deserialization
     */
    IMCRegionGroupRec(const oasys::Builder&);

    /**
     * Destructor.
     */
    ~IMCRegionGroupRec ();

    /**
     * Virtual from SerializableObject.
     */
    void serialize(oasys::SerializeAction* a);

    /**
     * Hook for the generic durable table implementation to know what
     * the key is for the database.
     */
    std::string durable_key();

    /**
     * lock to serialize access
     */
    std::recursive_mutex lock_;

protected:
    /**
     * Used internally to set the key string before access which is usually 
     * to update the database
     */
    void set_key_string();

public:

    /// Record types:
    ///     1 = IMC Region rec
    ///     2 = IMC Group rec
    ///     3 = IMC Manual Join for Delivery rec
    ///    -1 = IMC Region DB Clear rec - used to record the last counter used in a db clear command
    ///         to allow the startup config to clear the db once but not every time it is recycled
    ///         so that updates via AMP or the console are retained
    ///    -2 = IMC Group DB Clear rec - used to record the last counter used in a db clear command
    ///         to allow the startup config to clear the db once but not every time it is recycled
    ///         so that updates via AMP or the console are retained
    ///    -3 = IMC Manual Join for Delivery DB Clear rec - used to record the last counter used in a db clear command
    ///         to allow the startup config to clear the db once but not every time it is recycled
    ///         so that updates via AMP or the console are retained
    ///  -999 = IMC Home Region rec
    ssize_t rec_type_ = 0;

    /// Numeric definitions for the rec types
    #define IMC_REC_TYPE_REGION                            1
    #define IMC_REC_TYPE_GROUP                             2
    #define IMC_REC_TYPE_MANUAL_JOIN                       3
    #define IMC_REC_TYPE_REGION_DB_CLEAR                  -1
    #define IMC_REC_TYPE_GROUP_DB_CLEAR                   -2
    #define IMC_REC_TYPE_MANUAL_JOIN_DB_CLEAR             -3
    #define INC_REC_TYPE_HOME_REGION                    -999  
    /// String versions of the rec_type to determine if a key string is of the desired type
    #define IMC_REC_TYPE_REGION_STR                      "1_"
    #define IMC_REC_TYPE_GROUP_STR                       "2_"
    #define IMC_REC_TYPE_MANUAL_JOIN_STR                 "3_"
    #define IMC_REC_HOME_REGION_KEY_STR                  "home_region"
    #define IMC_REC_CLEAR_REGION_DB_KEY_STR              "clear_region_db"
    #define IMC_REC_CLEAR_GROUP_DB_KEY_STR               "clear_group_db"
    #define IMC_REC_CLEAR_MANUAL_JOIN_DB_KEY_STR         "clear_manual_join_db"

    /// For IMC Region and Group records this is the indicated Region/Group number;
    /// For IMC Manual Join records this is Group number of the IMC Destination EID
    /// For DB Clear recs this value is always zero
    /// For Home Region rec this value is the home region number
    size_t region_or_group_num_ = 0;

    /// For IMC Region and Group records this is The node number to add/remove from the indicated Region/Group;
    /// For IMC Manual Join records this is Service ID/number of the IMC Destination EID
    /// For DB clear records this is the last used ID#
    /// For Home Region rec this value is not used
    size_t node_or_id_num_ = 0;

    /// For IMC Region and Group recs this indicates whether this record is an override to
    /// add or remove the node from the Region/Group (1 = add, 2 = remove);
    /// Not used for DB clear recs
    /// For Home Region rec this value is not used
    uint32_t operation_ = 0;

    /// Numeric definitions for the operations
    #define IMC_REC_OPERATION_ADD        1
    #define IMC_REC_OPERATION_REMOVE     2

    /// flag indicating whether this node is an IMC Router Node
    bool is_router_node_ = false;

    /// flag indicating whether this record is in the database
    bool in_datastore_ = false;

private:
    /// unique key consisting of rec_type, region_group_num and node_num
    std::string key_;

};

} // namespace dtn


#endif /* _IMC_REGION_GROUP_REC_H_ */
