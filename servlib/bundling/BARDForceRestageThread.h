
#ifndef _BARD_FORCE_RESTAGE_THREAD_H_
#define _BARD_FORCE_RESTAGE_THREAD_H_

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif


#ifdef BARD_ENABLED

#include <third_party/oasys/thread/Thread.h>

#include "BARDRestageCLIF.h"
#include "Bundle.h"

namespace dtn {

class EndpointID;
typedef std::shared_ptr<EndpointID> SPtr_EID;


/**
 * Class/thread that runs through the pending bundles and routes
 * specified bundles that are over quota to the restage CL instance.
 */
class BARDForceRestageThread : public oasys::Thread
{
public:
    /**
     * Constructor.
     */
    BARDForceRestageThread(bard_quota_type_t quota_type, bard_quota_naming_schemes_t scheme,
                           std::string& nodename, uint64_t node_number,
                           size_t bundles_to_restage, size_t bytes_to_restage,
                           std::string restage_link_name);

    /**
     * Destructor (called at shutdown time).
     */
    virtual ~BARDForceRestageThread();

protected:
    /**
     * Main thread function that dispatches events.
     */
    virtual void run();

    /**
     * Find bundles to restage
     */
    virtual void scan_pending_bundles_map();

    /**
     * Determine if the bundle matches the criteria to restage
     */
    virtual bool is_bundle_to_restage(BundleRef& bref);

    /**
     * Determine if the endpoint matches the criteria to restage
     */
    virtual bool is_bundle_to_restage(const SPtr_EID& sptr_eid);

protected:
    bard_quota_type_t quota_type_;         ///< Quota Type (based on Source EID or Destination EID) of bundles to restage
    bard_quota_naming_schemes_t scheme_;   ///< Naming Scheme of the EID of bundles to restage (IPN, DTN or IMC)
    std::string nodename_;                 ///< Node name of the EID of bundles to restage
    uint64_t node_number_;                 ///< Node number of the EID of bundles to restage (applicable to IPN an IMC)

    size_t bundles_to_restage_;            ///< Number of bundles to restage
    size_t bytes_to_restage_;              ///< Number of payload bytes to restage
    std::string restage_link_name_;        ///< Restage Convergence Layer instance to restage bundles
};

} // namespace dtn

#endif  // BARD_ENABLED

#endif // _BARD_FORCE_RESTAGE_THREAD_H_
