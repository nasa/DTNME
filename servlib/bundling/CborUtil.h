
#ifndef _CBORUTIL_H_
#define _CBORUTIL_H_

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <cbor.h>

#include <third_party/oasys/debug/Log.h>

namespace dtn {


#define CBORUTIL_SUCCESS (int)(0)
#define CBORUTIL_FAIL    (int)(-1)
#define CBORUTIL_UNEXPECTED_EOF    (int)(1)


/**
 * Class proving CBOR utility functions
 */
class CborUtil {
public:
    
    /**
     * Constructor 
     */
    CborUtil(const char* user="base");
                   
    /**
     * Virtual destructor.
     */
    virtual ~CborUtil();

    virtual void set_bundle_id(bundleid_t id) { bundle_id_ = id; }

    virtual const char* fld_name() { return fld_name_.c_str(); }
    virtual void set_fld_name(const char* f) { fld_name_ = f; }
    virtual void set_fld_name(std::string& f) { fld_name_ = f; }

    virtual const char* cborutil_logpath() { return cborutil_logpath_.c_str(); }
    virtual void set_cborutil_logpath(const char* usage) {
        cborutil_logpath_ = "/cborutil/";
        cborutil_logpath_.append(usage);
    }
    virtual void set_cborutil_logpath(std::string& usage) {
        set_cborutil_logpath(usage.c_str());
    }
    

    /**
     * Calculate and CBOR encode values
     **/
    static int64_t uint64_encoding_len(uint64_t val);
    virtual int encode_crc(CborEncoder& blockArrayEncoder, uint32_t crc_type, 
                                uint8_t* block_first_cbor_byte);
    virtual int64_t encode_byte_string_header(uint8_t* buf, uint64_t buflen, uint64_t in_use, 
                                              uint64_t bstr_len, int64_t& encoded_len);
    virtual int64_t encode_array_header(uint8_t* buf, uint64_t buflen, uint64_t in_use, 
                                        uint64_t array_size, int64_t& encoded_len);
    virtual int64_t encode_uint64(uint8_t* buf, uint64_t buflen, uint64_t in_use, 
                                  uint64_t uval, int64_t& encoded_len);
    /**
     * Check the CBOR parser to verify there is len data available to process
     */
    virtual bool  data_available_to_parse(CborValue& cvElement, uint64_t len);


    virtual int validate_cbor_fixed_array_length(CborValue& cvElement, 
                    uint64_t min_len, uint64_t max_len, uint64_t& num_elements);

    //----------------------------------------------------------------------
    // this is a wrapper around cbor_value_get_string_length() which provide an
    // error indication if there is not enough data available to extract the length!
    // <> found it was aborting on cbor_parer_init() when there were not enough bytes
    //    to fully decode the length so that may mitigate the issue for normal processing
    // -- probably needs something similar for array length if we have to worry about
    //    lengths greater than 23
    virtual int decode_length_of_fixed_string(CborValue& cvElement, uint64_t& ret_val, uint64_t& decoded_bytes);

    virtual int decode_byte_string(CborValue& cvElement, std::string& ret_val);
    virtual int decode_text_string(CborValue& cvElement, std::string& ret_val);
    virtual int decode_uint(CborValue& cvElement, uint64_t& ret_val);
    virtual int decode_int(CborValue& cvElement, int64_t& ret_val);
    virtual int decode_boolean(CborValue& cvElement, bool& ret_val);
    virtual int decode_crc_and_validate(CborValue& cvElement, uint64_t crc_type, 
                                             const uint8_t* block_first_cbor_byte, bool& validated);

    //XXX/dz  TODO - give credit and location (stackexchange) where CRC calc routines were gotten
    /**
     * CRC calculations
     * Credit to Mark Adler...
     **/
    static uint32_t crc32c_table[8][256];
    static bool crc_initialized_;
    static void crc32c_init_sw(void);

    virtual uint32_t crc32c_sw(uint32_t crci, const unsigned char* buf, size_t len);
    virtual uint16_t crc16(const unsigned char* data, size_t size);

protected:

    #define CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN \
        if (err && (err != CborErrorOutOfMemory)) \
        { \
            if (bundle_id_ != 0) { \
                log_err_p(cborutil_logpath_.c_str(), "CBOR encoding error (bundle ID: %" PRIbid "): %s : %s", \
                          bundle_id_, fld_name(), cbor_error_string(err)); \
            } else { \
                log_err_p(cborutil_logpath_.c_str(), "CBOR encoding error: %s : %s", \
                          fld_name(), cbor_error_string(err)); \
            } \
            return CBORUTIL_FAIL; \
        }

    #define CBORUTIL_CHECK_CBOR_DECODE_ERR_RETURN \
        if ((err == CborErrorAdvancePastEOF) || (err == CborErrorUnexpectedEOF)) \
        { \
          return CBORUTIL_UNEXPECTED_EOF; \
        } \
        else if (err != CborNoError) \
        { \
          log_err_p(cborutil_logpath_.c_str(), "CBOR parsing error: %s :  %s", fld_name(), cbor_error_string(err)); \
          return CBORUTIL_FAIL; \
        }

    #define CHECK_CBORUTIL_STATUS_RETURN \
        if (status != CBORUTIL_SUCCESS) \
        { \
          return status; \
        }

    virtual int64_t encode_header_and_size(uint8_t* buf, uint64_t buflen, uint64_t in_use, 
                                           uint8_t cbor_type, uint64_t uval, int64_t& encoded_len);
public:
    bundleid_t bundle_id_ = 0;

    std::string cborutil_logpath_;

    std::string fld_name_;

    
};

} // namespace dtn

#endif /* _CBORUTIL_H_ */
