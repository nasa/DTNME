
#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <arpa/inet.h>

#include <cbor.h>

#include <third_party/oasys/debug/Log.h>

#include "CborUtil.h"



namespace dtn {

//----------------------------------------------------------------------
CborUtil::CborUtil(const char* usage)
{
    set_cborutil_logpath(usage);
    fld_name_= "";
}

//----------------------------------------------------------------------
CborUtil::~CborUtil()
{
}


//----------------------------------------------------------------------
int
CborUtil::encode_crc(CborEncoder& blockArrayEncoder, uint32_t crc_type, 
                              uint8_t* block_first_cbor_byte)
{
    static const uint8_t zeroes[4] = {0,0,0,0};

    CborError err;

    if (crc_type == 1) 
    {
        // 16 bit CRC
        if (block_first_cbor_byte == nullptr) 
        {
            // encode the CRC byte string to allow determination of encoding length
            err = cbor_encode_byte_string(&blockArrayEncoder, zeroes, 2);
            CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN
        }
        else
        {
            if (blockArrayEncoder.end > blockArrayEncoder.data.ptr)
            {
                size_t available_bytes = blockArrayEncoder.end - blockArrayEncoder.data.ptr;
                // need 3 bytes of buffer space available - 1 CBOR byte and 2 data bytes
                if (available_bytes >= 3)
                {
                    // there is enough buffer space to hold the CBOR'd CRC16 value
                    // initialize it to zeros
                    CborEncoder dummyEncoder;
                    memcpy(&dummyEncoder, &blockArrayEncoder, sizeof(CborEncoder));

                    err = cbor_encode_byte_string(&dummyEncoder, zeroes, 2);
                    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

                    // calculate the CRC on the CBOR data
                    size_t block_cbor_length = dummyEncoder.data.ptr - block_first_cbor_byte;
                    uint16_t crc = crc16(block_first_cbor_byte, block_cbor_length);

                    // convert CRC to network byte order
                    crc = htons(crc);

                    // CBOR encode the CRC
                    err = cbor_encode_byte_string(&blockArrayEncoder, (const uint8_t*) &crc, 2);
                    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN
                }
                else
                {
                    // encode the CRC byte string to allow determination of encoding length
                    err = cbor_encode_byte_string(&blockArrayEncoder, zeroes, 2);
                    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN
                }
            }
            else
            {
                // encode the CRC byte string to allow determination of encoding length
                err = cbor_encode_byte_string(&blockArrayEncoder, zeroes, 2);
                CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN
            }
        }
    }
    else if (crc_type == 2)
    {
        // 32 bit CRC
        if (block_first_cbor_byte == nullptr) 
        {
            // encode the CRC byte string to allow determination of encoding length
            err = cbor_encode_byte_string(&blockArrayEncoder, zeroes, 4);
            CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN
        }
        else
        {
            if (blockArrayEncoder.end > blockArrayEncoder.data.ptr)
            {
                size_t available_bytes = blockArrayEncoder.end - blockArrayEncoder.data.ptr;
                // need 5 bytes of buffer space available - 1 CBOR byte and 4 data bytes
                if (available_bytes >= 5)
                {
                    // there is enough buffer space to hold the CBOR'd CRC32c value
                    // initialize it to zeros
                    CborEncoder dummyEncoder;
                    memcpy(&dummyEncoder, &blockArrayEncoder, sizeof(CborEncoder));

                    err = cbor_encode_byte_string(&dummyEncoder, zeroes, 4);
                    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN

                    // calculate the CRC on the CBOR data
                    size_t block_cbor_length = dummyEncoder.data.ptr - block_first_cbor_byte;
                    uint32_t crc = 0;
                    crc = crc32c_sw(crc, block_first_cbor_byte, block_cbor_length);


                    // convert CRC to network byte order
                    crc = htonl(crc);

                    // CBOR encode the CRC
                    err = cbor_encode_byte_string(&blockArrayEncoder, (const uint8_t*) &crc, 4);
                    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN
                }
                else
                {
                    // encode the CRC byte string to allow determination of encoding length
                    err = cbor_encode_byte_string(&blockArrayEncoder, zeroes, 4);
                    CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN
                }
            }
            else
            {
                // encode the CRC byte string to allow determination of encoding length
                err = cbor_encode_byte_string(&blockArrayEncoder, zeroes, 4);
                CBORUTIL_CHECK_CBOR_ENCODE_ERROR_RETURN
            }
        }
    }
    else if (crc_type != 0)
    {
        return CBORUTIL_FAIL;
    }

    return CBORUTIL_SUCCESS;
}

//----------------------------------------------------------------------
// return values:
//     -1 = error
//      0 = success encoding (actual len returned in encoded_len which should match buflen)
//     >0 = bytes needed to encode
int64_t
CborUtil::encode_byte_string_header(uint8_t* buf, uint64_t buflen, uint64_t in_use, 
                                    uint64_t bstr_len, int64_t& encoded_len)
{
    return encode_header_and_size(buf, buflen, in_use, CborByteStringType, bstr_len, encoded_len);
}

//----------------------------------------------------------------------
// return values:
//     -1 = error
//      0 = success encoding (actual len returned in encoded_len which should match buflen)
//     >0 = bytes needed to encode
int64_t
CborUtil::encode_array_header(uint8_t* buf, uint64_t buflen, uint64_t in_use, 
                              uint64_t array_size, int64_t& encoded_len)
{
    return encode_header_and_size(buf, buflen, in_use, CborArrayType, array_size, encoded_len);
}

//----------------------------------------------------------------------
// return values:
//     -1 = error
//      0 = success encoding (actual len returned in encoded_len which should match buflen)
//     >0 = bytes needed to encode
int64_t
CborUtil::encode_uint64(uint8_t* buf, uint64_t buflen, uint64_t in_use, 
                        uint64_t uval, int64_t& encoded_len)
{
    return encode_header_and_size(buf, buflen, in_use, CborIntegerType, uval, encoded_len);
}


//----------------------------------------------------------------------
// return values:
//     -1 = error
//      0 = success encoding (actual len returned in encoded_len which should match buflen)
//     >0 = bytes needed to encode
int64_t
CborUtil::encode_header_and_size(uint8_t* buf, uint64_t buflen, uint64_t in_use, 
                                 uint8_t cbor_type, uint64_t uval, int64_t& encoded_len)
{
    int64_t need_bytes = 0;

    // calculate number of bytes need to encode the CBOR header bytes
    // and set the CBOR Major Type byte
    //
    //     from tinycor/cbor.h
    //        typedef enum CborType {
    //            CborIntegerType     = 0x00,
    //            CborByteStringType  = 0x40,
    //            CborTextStringType  = 0x60,
    //            CborArrayType       = 0x80,
    //            CborMapType         = 0xa0,
    //            CborTagType         = 0xc0,
    //            CborSimpleType      = 0xe0,
    //            CborBooleanType     = 0xf5,
    //            CborNullType        = 0xf6,
    //            CborUndefinedType   = 0xf7,
    //            CborHalfFloatType   = 0xf9,
    //            CborFloatType       = 0xfa,
    //            CborDoubleType      = 0xfb,
    //        
    //            CborInvalidType     = 0xff   (equivalent to the break byte, so it will never be used)
    //        } CborType;
    uint8_t major_type_byte = cbor_type;
    if (uval < 24U) {
        // length encoded in the major type byte
        need_bytes = 1;
        major_type_byte |= uval & 0xffU;
    } else if (uval <= UINT8_MAX) {
        // major type byte + 1 size byte
        need_bytes = 2;
        major_type_byte += 24U;
    } else if (uval <= UINT16_MAX) {
        // major type byte + 2 size bytes
        need_bytes = 3;
        major_type_byte += 25U;
    } else if (uval <= UINT32_MAX) {
        // major type byte + 4 size bytes
        need_bytes = 5;
        major_type_byte += 26U;
    } else {
        // major type byte + 8 size bytes
        need_bytes = 9;
        major_type_byte += 27U;
    }

    int64_t remaining = need_bytes;

    // adjust need_bytes to be the total needed for the block header
    need_bytes += in_use;

    if (buf != nullptr) {
        // verify buf has enough room for the encoding of the CBOR header bytes
        if ((int64_t)(buflen - in_use) < remaining) {
            return -1;
        } else {
            int bits_to_shift;
            uint64_t mask;
            uint64_t offset = in_use;
            buf[offset++] = major_type_byte;
            --remaining;

            // insert size bytes in big-endian order
            while (remaining > 0) {
                bits_to_shift = 8 * (remaining - 1);
                mask = 0xffU << bits_to_shift;
                buf[offset++] = (uval & mask) >> bits_to_shift;
                --remaining;
            }

            encoded_len = need_bytes;

            // encoding successful - return 0 since no additional bytes needed
            need_bytes = 0;
        }
    }

    return need_bytes;
}



//----------------------------------------------------------------------
bool
CborUtil::data_available_to_parse(CborValue& cvElement, uint64_t len)
{
    return (cvElement.ptr <= (cvElement.parser->end - len));
}



//----------------------------------------------------------------------
// returns: 
//     BP_ERROR (-1) = CBOR or Protocol Error
//     CBORUTIL_SUCCESS (0) = success
//     CBORUTIL_UNEXPECTED_EOF (1) = need more data to parse
int
CborUtil::decode_byte_string(CborValue& cvElement, std::string& ret_val)
{
    if (!data_available_to_parse(cvElement, 1))
    {
        return CBORUTIL_UNEXPECTED_EOF;
    }


    if (cbor_value_is_byte_string(&cvElement))
    {
        uint8_t* buf = nullptr;
        size_t n = 0;
        CborError err = cbor_value_dup_byte_string(&cvElement, &buf, &n, &cvElement);
        CBORUTIL_CHECK_CBOR_DECODE_ERR_RETURN

        ret_val.clear();
        ret_val.append((const char*) buf, n);

        free(buf);
    }
    else
    {
        log_err_p(cborutil_logpath(), "CBOR decode Error: %s - expect CBOR byte string", fld_name());
        return CBORUTIL_FAIL;
    }

    return CBORUTIL_SUCCESS;
}

//----------------------------------------------------------------------
// returns: 
//     BP_ERROR (-1) = CBOR or Protocol Error
//     CBORUTIL_SUCCESS (0) = success
//     CBORUTIL_UNEXPECTED_EOF (1) = need more data to parse
int
CborUtil::decode_text_string(CborValue& cvElement, std::string& ret_val)
{
    if (!data_available_to_parse(cvElement, 1))
    {
        return CBORUTIL_UNEXPECTED_EOF;
    }


    if (cbor_value_is_text_string(&cvElement))
    {
        char *buf = nullptr;
        size_t n = 0;
        CborError err = cbor_value_dup_text_string(&cvElement, &buf, &n, &cvElement);
        CBORUTIL_CHECK_CBOR_DECODE_ERR_RETURN

        ret_val.clear();
        ret_val.append(buf, n);

        free(buf);
    }
    else
    {
        log_err_p(cborutil_logpath(), "CBOR decode error: %s - expect CBOR byte string", fld_name());
        return CBORUTIL_FAIL;
    }

    return CBORUTIL_SUCCESS;
}

//----------------------------------------------------------------------
// this is a wrapper around cbor_value_get_string_length() which provide an
// error indication if there is not enough data available to extract the length!
// <> found it was aborting on cbor_parer_init() when there were not enough bytes
//    to fully decode the length so that may mitigate the issue for normal processing
// -- probably needs something similar for array length if we have to worry about
//    lengths greater than 23
//
// returns: 
//     BP_ERROR (-1) = CBOR or Protocol Error
//     CBORUTIL_SUCCESS (0) = success
//     CBORUTIL_UNEXPECTED_EOF (1) = need more data to parse
int
CborUtil::decode_length_of_fixed_string(CborValue& cvElement, uint64_t& ret_val, uint64_t& decoded_bytes)
{
    if (!data_available_to_parse(cvElement, 1))
    {
        return CBORUTIL_UNEXPECTED_EOF;
    }

    if (!cbor_value_is_byte_string(&cvElement) && !cbor_value_is_text_string(&cvElement)) {
        log_err_p(cborutil_logpath(), "CBOR decode error: %s - expect CBOR byte or text string", fld_name());
        return CBORUTIL_FAIL;
    }

    if (!cbor_value_is_length_known(&cvElement)) {
        log_err_p(cborutil_logpath(), "CBOR decode error: %s - expect CBOR fixed length byte or text string", fld_name());
        return CBORUTIL_FAIL;
    }


    // do we have enough data bytes to extract the length?
    int64_t need_bytes = 1;
    uint8_t length_bits = *cvElement.ptr & 0x1f;

    switch (length_bits) {
        case 31:   // undefined length indicator
        case 30:   // reserved
        case 29:   // reserved
        case 28:   // reserved 
            log_crit_p(cborutil_logpath(), "CBOR decode error: %s - expect CBOR fixed length byte or text string"
                                           " - ?? was not caught by cbor_value_is_length_known() ??", fld_name());
            return CBORUTIL_FAIL;

        case 27:
            need_bytes = 9; // header byte + 8 bytes for a 64 bit value
            break;

        case 26:
            need_bytes = 5; // header byte + 4 bytes for a 32 bit value
            break;

        case 25:
            need_bytes = 3; // header byte + 2 bytes for a 16 bit value
            break;

        case 24:
            need_bytes = 2; // header byte + 1 bytes for a 8 bit value
            break;

        default:
            // need_bytes = 1;
            // the bits are the actual length
            ret_val = length_bits;
            break;
    }

    if (need_bytes > 1) {
        if (!data_available_to_parse(cvElement, need_bytes))
        {
            return CBORUTIL_UNEXPECTED_EOF;
        }

        CborError err = cbor_value_get_string_length(&cvElement, &ret_val);
        CBORUTIL_CHECK_CBOR_DECODE_ERR_RETURN
    }

    decoded_bytes = need_bytes;

    return CBORUTIL_SUCCESS;
}

//----------------------------------------------------------------------
// returns: 
//     BP_ERROR (-1) = CBOR or Protocol Error
//     CBORUTIL_SUCCESS (0) = success
//     CBORUTIL_UNEXPECTED_EOF (1) = need more data to parse
int
CborUtil::decode_uint(CborValue& cvElement, uint64_t& ret_val)
{
    if (!data_available_to_parse(cvElement, 1))
    {
        return CBORUTIL_UNEXPECTED_EOF;
    }

    if (cbor_value_is_unsigned_integer(&cvElement))
    {
        uint64_t uval = 0;
        CborError err = cbor_value_get_uint64(&cvElement, &uval);
        CBORUTIL_CHECK_CBOR_DECODE_ERR_RETURN

        err = cbor_value_advance_fixed(&cvElement);
        CBORUTIL_CHECK_CBOR_DECODE_ERR_RETURN
  
        ret_val = uval;
    }
    else
    {
        log_err_p(cborutil_logpath(), "CBOR decode Error: %s - must be encoded as an unsigned integer", fld_name());
        return CBORUTIL_FAIL;
    }

    return CBORUTIL_SUCCESS;
    
}

//----------------------------------------------------------------------
// returns: 
//     BP_ERROR (-1) = CBOR or Protocol Error
//     CBORUTIL_SUCCESS (0) = success
//     CBORUTIL_UNEXPECTED_EOF (1) = need more data to parse
int
CborUtil::decode_int(CborValue& cvElement, int64_t& ret_val)
{
    if (!data_available_to_parse(cvElement, 1))
    {
        return CBORUTIL_UNEXPECTED_EOF;
    }

    if (cbor_value_is_integer(&cvElement))
    {
        int64_t ival = 0;
        CborError err = cbor_value_get_int64(&cvElement, &ival);
        CBORUTIL_CHECK_CBOR_DECODE_ERR_RETURN

        err = cbor_value_advance_fixed(&cvElement);
        CBORUTIL_CHECK_CBOR_DECODE_ERR_RETURN
  
        ret_val = ival;
    }
    else
    {
        log_err_p(cborutil_logpath(), "CBOR decode Error: %s - must be encoded as an integer", fld_name());
        return CBORUTIL_FAIL;
    }

    return CBORUTIL_SUCCESS;
    
}

//----------------------------------------------------------------------
// returns: 
//     BP_ERROR (-1) = CBOR or Protocol Error
//     CBORUTIL_SUCCESS (0) = success
//     CBORUTIL_UNEXPECTED_EOF (1) = need more data to parse
int
CborUtil::decode_boolean(CborValue& cvElement, bool& ret_val)
{
    if (!data_available_to_parse(cvElement, 1))
    {
        return CBORUTIL_UNEXPECTED_EOF;
    }

    if (cbor_value_is_boolean(&cvElement))
    {
        bool bval = false;
        CborError err = cbor_value_get_boolean(&cvElement, &bval);
        CBORUTIL_CHECK_CBOR_DECODE_ERR_RETURN

        err = cbor_value_advance_fixed(&cvElement);
        CBORUTIL_CHECK_CBOR_DECODE_ERR_RETURN
  
        ret_val = bval;
    }
    else
    {
        log_err_p(cborutil_logpath(), "CBOR decode Error: %s - must be encoded as a boolean", fld_name());
        return CBORUTIL_FAIL;
    }

    return CBORUTIL_SUCCESS;
    
}

//----------------------------------------------------------------------
// returns: 
//     BP_ERROR (-1) = CBOR or Protocol Error
//     CBORUTIL_SUCCESS (0) = success
//     CBORUTIL_UNEXPECTED_EOF (1) = need more data to parse
int
CborUtil::validate_cbor_fixed_array_length(CborValue& cvElement, 
                                      uint64_t min_len, uint64_t max_len, uint64_t& num_elements)
{
    if (!data_available_to_parse(cvElement, 1))
    {
        return CBORUTIL_UNEXPECTED_EOF;
    }

    CborType type = cbor_value_get_type(&cvElement);
    if (CborArrayType != type)
    {
        log_err_p(cborutil_logpath(), "CBOR decode Error: %s - must be encoded as an array", fld_name());
        return CBORUTIL_FAIL;
    }

    if (! cbor_value_is_length_known(&cvElement))
    {
        log_err_p(cborutil_logpath(), "CBOR decode error: %s - must be encoded as a fixed length array", fld_name());
        return CBORUTIL_FAIL;
    }

    CborError err = cbor_value_get_array_length(&cvElement, &num_elements);
    CBORUTIL_CHECK_CBOR_DECODE_ERR_RETURN

    if (num_elements < min_len)
    {
        log_err_p(cborutil_logpath(), "CBOR decode error: %s -  array length less than minimum (%" PRIu64 "): %" PRIu64, 
                  fld_name(), min_len, num_elements);
        return CBORUTIL_FAIL;
    }
    else if (num_elements > max_len)
    {
        log_err_p(cborutil_logpath(), "CBOR decode error: %s -  array length greater than maximum (%" PRIu64 "): %" PRIu64, 
                  fld_name(), max_len, num_elements);
        return CBORUTIL_FAIL;
    }

    return CBORUTIL_SUCCESS; // okay to continue parsing the array
}


//----------------------------------------------------------------------
// return vals:
//     BP_ERROR (-1) = CBOR or Protocol Error
//     CBORUTIL_SUCCESS (0) = success
//     CBORUTIL_UNEXPECTED_EOF (1) = need more data to parse
int
CborUtil::decode_crc_and_validate(CborValue& cvElement, uint64_t crc_type, 
                                                 const uint8_t* block_first_cbor_byte, bool& validated)
{
    validated = false;

    if (!data_available_to_parse(cvElement, 1))
    {
        return CBORUTIL_UNEXPECTED_EOF;
    }

    if (!cbor_value_is_byte_string(&cvElement))
    {
        log_err_p(cborutil_logpath(), "CBOR decode error: %s - expected CBOR byte string", fld_name());
        return CBORUTIL_FAIL;
    }

    if (!cbor_value_is_length_known(&cvElement))
    {
        log_err_p(cborutil_logpath(), "CBOR decode error: %s - expected fixed length CBOR byte string", fld_name());
        return CBORUTIL_FAIL;
    }

    // correct length?
    size_t crc_len;
    CborError err = cbor_value_get_string_length(&cvElement, &crc_len);
    CBORUTIL_CHECK_CBOR_DECODE_ERR_RETURN

    if ((crc_type == 1) && (crc_len != 2))
    {
        log_err_p(cborutil_logpath(), "CBOR decode error: %s - expected 2 bytes for CRC16 instead of %zu", fld_name(), crc_len);
        return CBORUTIL_FAIL;
    }
    if ((crc_type == 2) && (crc_len != 4))
    {
        log_err_p(cborutil_logpath(), "CBOR decode error: %s - expected 4 bytes for CRC32c instead of %zu", fld_name(), crc_len);
        return CBORUTIL_FAIL;
    }


    // pointers to manipulate the CBOR data in order to calculate the CRC 
    const uint8_t* crc_first_cbor_byte = cvElement.ptr;
    const uint8_t* crc_first_cbor_data_byte = crc_first_cbor_byte + 1;

            
    std::string crc_string;
    int status = decode_byte_string(cvElement, crc_string);
    CHECK_CBORUTIL_STATUS_RETURN

    size_t block_cbor_length = crc_first_cbor_byte - block_first_cbor_byte + crc_string.size() + 1; // +1 for CBOR type and len byte

    if (crc_type == 1)
    {
        uint16_t crc;
        memcpy(&crc, crc_string.c_str(), 2);
        crc = ntohs(crc);

        // zero out the CRC CBOR bytes in order to calculate the CRC on the received data
        memset((void*) crc_first_cbor_data_byte, 0, 2);

        uint16_t calc_crc = crc16(block_first_cbor_byte, block_cbor_length);

        validated = (calc_crc == crc);
    }
    else if (crc_type == 2)
    {
        uint32_t crc;
        memcpy(&crc, crc_string.c_str(), 4);
        crc = ntohl(crc);

        // zero out the CRC CBOR bytes in order to calculate the CRC on the received data
        memset((void*) crc_first_cbor_data_byte, 0, 4);

        uint32_t calc_crc = 0;
        calc_crc = crc32c_sw(calc_crc, block_first_cbor_byte, block_cbor_length);

        validated = (calc_crc == crc);
    }
    else
    {
        log_err_p(cborutil_logpath(), "CBOR decode error: %s - invalid CRC type: %" PRIu64,
                  fld_name(), crc_type);
        return CBORUTIL_FAIL;
    }

    return CBORUTIL_SUCCESS;
}

//----------------------------------------------------------------------
int64_t
CborUtil::uint64_encoding_len(uint64_t val)
{
    int64_t need_bytes = 0;

    // calculate number of bytes need to encode the CBOR header bytes
    // and set the CBOR Major Type byte
    if (val < 24U) {
        // length encoded in the major type byte
        need_bytes = 1;
    } else if (val <= UINT8_MAX) {
        // major type byte + 1 size byte
        need_bytes = 2;
    } else if (val <= UINT16_MAX) {
        // major type byte + 2 size bytes
        need_bytes = 3;
    } else if (val <= UINT32_MAX) {
        // major type byte + 4 size bytes
        need_bytes = 5;
    } else {
        // major type byte + 8 size bytes
        need_bytes = 9;
    }

    return need_bytes;
}

//----------------------------------------------------------------------
uint16_t
CborUtil::crc16(const unsigned char* data, size_t size)
{
    uint16_t crc = 0;
    while (size--) 
    { 
        crc ^= *data++; 
        for (unsigned k = 0; k < 8; k++)
        {
            crc = crc & 1 ? (crc >> 1) ^ 0xa001 : crc >> 1; 
        }
    } return crc; 
}



/* CRC-32C (iSCSI) polynomial in reversed bit order. */
#define POLY 0x82f63b78

/* Table for a quadword-at-a-time software crc. */
//static pthread_once_t crc32c_once_sw = PTHREAD_ONCE_INIT;
uint32_t CborUtil::crc32c_table[8][256];
bool CborUtil::crc_initialized_ = false;

//----------------------------------------------------------------------
/* Construct table for software CRC-32C calculation. */
void
CborUtil::crc32c_init_sw(void)
{
    uint32_t n, crc, k;

    for (n = 0; n < 256; n++) {
        crc = n;
        crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
        crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
        crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
        crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
        crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
        crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
        crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
        crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
        crc32c_table[0][n] = crc;
    }
    for (n = 0; n < 256; n++) {
        crc = crc32c_table[0][n];
        for (k = 1; k < 8; k++) {
            crc = crc32c_table[0][crc & 0xff] ^ (crc >> 8);
            crc32c_table[k][n] = crc;
        }
    }
}


//----------------------------------------------------------------------
/* Table-driven software version as a fall-back.  This is about 15 times slower
   than using the hardware instructions.  This assumes little-endian integers,
   as is the case on Intel processors that the assembler code here is for. */
uint32_t
CborUtil::crc32c_sw(uint32_t crci, const unsigned char* buf, size_t len)
{
    const unsigned char *next = buf;
    uint64_t crc;
    uint64_t next_qword;

    //pthread_once(&crc32c_once_sw, crc32c_init_sw);
    if ( !crc_initialized_ ) {
        crc32c_init_sw();
        crc_initialized_ = true;
    }

    crc = crci ^ 0xffffffff;
    while (len && ((uintptr_t)next & 7) != 0) {
        crc = crc32c_table[0][(crc ^ *next++) & 0xff] ^ (crc >> 8);
        len--;
    }
    while (len >= 8) {
        memcpy(&next_qword, next, 8);
        crc ^= next_qword;
        crc = crc32c_table[7][crc & 0xff] ^
              crc32c_table[6][(crc >> 8) & 0xff] ^
              crc32c_table[5][(crc >> 16) & 0xff] ^
              crc32c_table[4][(crc >> 24) & 0xff] ^
              crc32c_table[3][(crc >> 32) & 0xff] ^
              crc32c_table[2][(crc >> 40) & 0xff] ^
              crc32c_table[1][(crc >> 48) & 0xff] ^
              crc32c_table[0][crc >> 56];
        next += 8;
        len -= 8;
    }
    while (len) {
        crc = crc32c_table[0][(crc ^ *next++) & 0xff] ^ (crc >> 8);
        len--;
    }
    return (uint32_t)crc ^ 0xffffffff;
}

//----------------------------------------------------------------------
} // namespace dtn
