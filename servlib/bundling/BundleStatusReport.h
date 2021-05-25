
#ifndef _BUNDLE_STATUS_REPORT_H_
#define _BUNDLE_STATUS_REPORT_H_

#include "Bundle.h"
#include "BundleProtocol.h"
#include "CborUtilBP7.h"
#include "BundleTimestamp.h"

namespace dtn {

/**
 * Utility class to create and parse status reports.
 */
class BundleStatusReport {
public:
    typedef enum {
        STATUS_RECEIVED         = 0x01,
        STATUS_CUSTODY_ACCEPTED = 0x02,
        STATUS_FORWARDED        = 0x04,
        STATUS_DELIVERED        = 0x08,
        STATUS_DELETED          = 0x10,
        STATUS_ACKED_BY_APP     = 0x20,
        STATUS_UNUSED           = 0x40,
        STATUS_UNUSED2          = 0x80,
    } flag_t;
    
    /**
     * The reason codes are defined in the BundleProtocol class.
     */
    typedef BundleProtocol::status_report_reason_t reason_t;

    /**
     * Specification of the contents of a Bundle Status Report
     */
    struct data_t {
        bool            received_             = false;
        uint64_t        received_timestamp_   = 0;
        bool            forwarded_            = false;
        uint64_t        forwarded_timestamp_  = 0;
        bool            delivered_            = false;
        uint64_t        delivered_timestamp_  = 0;
        bool            deleted_              = false;
        uint64_t        deleted_timestamp_    = 0;

        uint64_t        reason_code_          = 0;

        EndpointID      orig_source_eid_;
        BundleTimestamp orig_creation_ts_;

        u_int64_t       orig_frag_offset_    = 0;
        u_int64_t       orig_frag_length_    = 0 ;
    };

    /**
     * Constructor-like function that fills in the bundle payload
     * buffer with the appropriate status report format.
     *
     * Although the spec allows for multiple timestamps to be set in a
     * single status report, this implementation only supports
     * creating a single timestamp per report, hence there is only
     * support for a single flag to be passed in the parameters.
     */
    static void create_status_report(Bundle*           bundle,
                                     const Bundle*     orig_bundle,
                                     const EndpointID& source,
                                     flag_t            status_flag,
                                     reason_t          reason);
    
    /**
     * Parse a byte stream containing a Status Report Payload and
     * store the fields in the given struct. Returns false if parsing
     * failed.
     */
    static bool parse_status_report(CborValue& cvPayloadElements, CborUtilBP7& cborutil, data_t& sr_data);


    /**
     * Return a string version of the reason code.
     */
    static const char* reason_to_str(u_int8_t reason);

protected:
    static bool parse_status_indication_array(CborValue& cvStatusElements, CborUtilBP7& cborutil, data_t& sr_data);


    static int64_t encode_report(uint8_t* buf, int64_t buflen, 
                                 CborUtilBP7& cborutil,
                                 const Bundle*     orig_bundle,
                                 flag_t            status_flags,
                                 reason_t          reason);

    static int encode_status_indicators(CborEncoder& rptArrayEncode, flag_t status_flags, 
                                        const bool include_timestamp);

    static int encode_indicator(CborEncoder& indicatorArrayEncoder, uint64_t indicated, 
                                const bool include_timestamp);
};

} // namespace dtn

#endif /* _BUNDLE_STATUS_REPORT_H_ */
