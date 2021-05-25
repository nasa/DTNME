
#ifndef _CBORUTIL_BP7H_
#define _CBORUTIL_BP7H_

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <cbor.h>

#include <third_party/oasys/debug/Log.h>

#include "CborUtil.h"
#include "BundleTimestamp.h"
#include "naming/EndpointID.h"



namespace dtn {


#define CBORUTIL_SUCCESS (int)(0)
#define CBORUTIL_FAIL    (int)(-1)
#define CBORUTIL_UNEXPECTED_EOF    (int)(1)


/**
 * BP7 version adds methods to encode and decode BP classes
 */
class CborUtilBP7 : public CborUtil {
public:
    
    /**
     * Constructor 
     */
    CborUtilBP7(const char* user="CborUtilBP7");
                   
    /**
     * Virtual destructor.
     */
    virtual ~CborUtilBP7();


    /**
     * Calculate and CBOR encode values
     **/
    virtual int encode_eid_dtn_none(CborEncoder& blockArrayEncoder);
    virtual int encode_eid_dtn(CborEncoder& blockArrayEncoder, const EndpointID& eidref);
    virtual int encode_eid_ipn(CborEncoder& blockArrayEncoder, const EndpointID& eidref);
    virtual int encode_eid(CborEncoder& blockArrayEncoder, const EndpointID& eidref);
    virtual int encode_bundle_timestamp(CborEncoder& blockArrayEncoder, const BundleTimestamp& bts);

    virtual int decode_eid(CborValue& cvElement, EndpointID& eid);
    virtual int decode_bundle_timestamp(CborValue& cvElement, BundleTimestamp& timestamp);

};

} // namespace dtn

#endif /* _CBORUTIL_BP7H_ */
