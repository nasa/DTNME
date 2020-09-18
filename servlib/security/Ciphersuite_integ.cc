
#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif
#include "Ciphersuite.h"
#include "BP_Local_CS.h"
#include "bundling/Bundle.h"
#include "Ciphersuite_integ.h"
#include "bundling/SDNV.h"
#include <openssl/evp.h>

#ifdef BSP_ENABLED

namespace dtn {

static const char* log = "/dtn/bundle/ciphersuite";

struct PrimaryBlock_ex {
    u_int8_t version;
    u_int64_t processing_flags;
    u_int64_t block_length;
    u_int64_t dest_scheme_offset;
    u_int64_t dest_ssp_offset;
    u_int64_t source_scheme_offset;
    u_int64_t source_ssp_offset;
    u_int64_t replyto_scheme_offset;
    u_int64_t replyto_ssp_offset;
    u_int64_t custodian_scheme_offset;
    u_int64_t custodian_ssp_offset;
    u_int64_t creation_time;
    u_int64_t creation_sequence;
    u_int64_t lifetime;
    u_int64_t dictionary_length;
    u_int64_t fragment_offset;
    u_int64_t original_length;
};

// Need quad versions of hton for manipulating full-length (unpacked) SDNV values
#if defined(WORDS_BIGENDIAN) && (WORDS_BIGENDIAN == 1)
#define htonq( x ) (x)
#define ntohq( x ) (x)
#else

inline u_int64_t htonq( u_int64_t x )
{
    u_int64_t   res;
    u_int32_t   hi = x >> 32;
    u_int32_t   lo = x & 0xffffffff;
    hi = htonl( hi );
    res = htonl( lo );
    res = res << 32 | hi;

    return res;
}

inline u_int64_t ntohq( u_int64_t x )
{
    u_int64_t   res;
    u_int32_t   hi = x >> 32;
    u_int32_t   lo = x & 0xffffffff;
    hi = ntohl( hi );
    res = ntohl( lo );
    res = res << 32 | hi;

    return res;
}
#endif


int Ciphersuite_integ::mutable_canonicalization_primary(const Bundle *bundle, BlockInfo *block, BlockInfo *iter /*This is a pointer to the primary block*/, OpaqueContext*  r, char **d) {
     PrimaryBlock_ex primary;
     u_int64_t       mask_primary = 0x000000000007C1BELLU;    /// mask for primary-block flags
     u_int32_t       header_len;
     u_int64_t       target_flags;
     u_int32_t       offset;
     char            *dict;


    header_len =        1       //version
                        +   8       //flags SDNV
                        +   4       //header length itself
                        +   4       //destination eid length
                        +   4       //source eid length
                        +   4       //report-to eid length
                        +   8       //creation SDNV #1
                        +   8       //creation SDNV #2
                        +   8;      //lifetime SDNV
    
    if ( bundle->is_fragment() ) 
        header_len +=   8       //fragment offset SDNV
                        +   8;      //total-length SDNV
    
    // do stuff for primary, and ignore it during the walk
    
    if(read_primary(bundle, iter, primary, d) == -1) {
        return BP_FAIL;
    }
    dict = *d;

    header_len += strlen(dict + primary.dest_scheme_offset);
    header_len += strlen(dict + primary.dest_ssp_offset);
    header_len += strlen(dict + primary.source_scheme_offset);
    header_len += strlen(dict + primary.source_ssp_offset);
    header_len += strlen(dict + primary.replyto_scheme_offset);
    header_len += strlen(dict + primary.replyto_ssp_offset);
    log_debug_p(log, "Ciphersuite::mutable_canonicalization_primary() header_len %u", header_len);     


    // Now start the actual digest process
    digest( bundle, block, &*iter, &primary.version, 1, r);     //version
    
    primary.processing_flags &= mask_primary;
    target_flags = htonq(primary.processing_flags);
    digest( bundle, block, &*iter, &target_flags, sizeof(target_flags), r);
    
    header_len = htonl(header_len);
    digest( bundle, block, &*iter, &header_len, sizeof(header_len), r);
    
    
    offset = strlen(dict + primary.dest_scheme_offset) + strlen(dict + primary.dest_ssp_offset);    // Note:- "offset" is 4 bytes, not 8
    offset = htonl(offset);
    digest( bundle, block, &*iter, &offset, sizeof(offset), r);
    digest( bundle, block, &*iter, dict + primary.dest_scheme_offset, strlen(dict + primary.dest_scheme_offset), r);
    digest( bundle, block, &*iter, dict + primary.dest_ssp_offset, strlen(dict + primary.dest_ssp_offset), r);

    offset = strlen(dict + primary.source_scheme_offset) + strlen(dict + primary.source_ssp_offset);
    offset = htonl(offset);
    digest( bundle, block, &*iter, &offset, sizeof(offset), r);
    digest( bundle, block, &*iter, dict + primary.source_scheme_offset, strlen(dict + primary.source_scheme_offset), r);
    digest( bundle, block, &*iter, dict + primary.source_ssp_offset, strlen(dict + primary.source_ssp_offset), r);

    offset = strlen(dict + primary.replyto_scheme_offset) + strlen(dict + primary.replyto_ssp_offset);
    offset = htonl(offset);
    digest( bundle, block, &*iter, &offset, sizeof(offset), r);
    digest( bundle, block, &*iter, dict + primary.replyto_scheme_offset, strlen(dict + primary.replyto_scheme_offset), r);
    digest( bundle, block, &*iter, dict + primary.replyto_ssp_offset, strlen(dict + primary.replyto_ssp_offset), r);
    
    // two SDNVs for creation timestamp, one for lifetime
    primary.creation_time = htonq(primary.creation_time);
    digest( bundle, block, &*iter, &primary.creation_time, sizeof(primary.creation_time), r);
    primary.creation_sequence = htonq(primary.creation_sequence);
    digest( bundle, block, &*iter, &primary.creation_sequence, sizeof(primary.creation_sequence), r);
    primary.lifetime = htonq(primary.lifetime);
    digest( bundle, block, &*iter, &primary.lifetime, sizeof(primary.lifetime), r);
    
    if ( bundle->is_fragment() ) {
        primary.fragment_offset = htonq(primary.fragment_offset);
        digest( bundle, block, &*iter, &primary.fragment_offset, sizeof(primary.fragment_offset), r);
        primary.original_length = htonq(primary.original_length);
        digest( bundle, block, &*iter, &primary.original_length, sizeof(primary.original_length), r);
    }
    return BP_SUCCESS;
}


int Ciphersuite_integ::mutable_canonicalization_extension(const Bundle *bundle, BlockInfo *block, BlockInfo *iter, OpaqueContext*  r, char *dict) {
    u_char*         buf;
    size_t          len;
    size_t          sdnv_len;
    u_int32_t       offset;
    u_char          c;
    u_int64_t       target_flags;
    u_int64_t       flags_save;
    u_int64_t       mask = 0x57LLU;            /// specify mask for flags
    const char*     ptr;
    size_t          plen;
    u_int64_t       eid_ref_count = 0LLU;
    u_int64_t       target_content_length;
    u_int64_t       cs_flags;
    log_debug_p(log, "Ciphersuite_integ::mutable_canonicalization_Extension() dict=%x", dict);


    // Either it has no correlator, or it wasn't in the list.
    // So we will process it in the digest
            
    /**********  start preamble processing  **********/
    buf = iter->contents().buf();
    len = iter->full_length();
            
            
    // Process block type
    c = *buf++;
    len--;
    digest( bundle, block, &*iter, &c, 1, r);
            
    // Process flags
    sdnv_len = SDNV::decode( buf, len, &target_flags);
    buf += sdnv_len;
    len -= sdnv_len;
            
    log_debug_p(log, "Ciphersuite::mutable_canonicalization_extension(): before processing, target_flags=%llx", target_flags);
    flags_save = target_flags;
    target_flags &= mask;
    target_flags = htonq(target_flags);
    log_debug_p(log, "Ciphersuite::mutable_canonicalization_extension(): after processing, target_flags=%llx", target_flags);
    digest( bundle, block, &*iter, &target_flags, sizeof(target_flags), r);
            
    // EID list is next, starting with the count although we don't digest it
    if ( flags_save & BundleProtocol::BLOCK_FLAG_EID_REFS ) {                    
        sdnv_len = SDNV::decode(buf, len, &eid_ref_count);
        buf += sdnv_len;
        len -= sdnv_len;
        log_debug_p(log, "Ciphersuite::mutable_canonicalization_extension() eid_ref_count %llu", U64FMT(eid_ref_count));
                                        
        // each ref is a pair of SDNVs, so process 2 * eid_ref_count text pieces
        if ( eid_ref_count > 0 ) {
            for ( u_int32_t i = 0; i < (2 * eid_ref_count); i++ ) {
                sdnv_len = SDNV::decode(buf, len, &offset);
                log_debug_p(log, "Ciphersuite::mutable_canonicalization_extension() eid offset %d", offset);
                buf += sdnv_len;
                len -= sdnv_len;
                        
                ptr = dict + offset;    //point at item in dictionary
                plen = strlen(ptr);     // length *without* NULL-terminator
                log_debug_p(log, "Ciphersuite::mutable_canonicalization_extension() digesting eid part %s", ptr);
                digest( bundle, block, &*iter, ptr, plen, r);
            }
        }       
    }

    // Process data length
    sdnv_len = SDNV::decode(buf, len, &target_content_length);
    buf += sdnv_len;
    len -= sdnv_len;
    log_debug_p(log, "Ciphersuite::mutable_canonicalization_extension(): block content length=%llu", target_content_length);
            
    target_content_length = htonq(target_content_length);
    if((&*iter)!=block || cs_num()==2) {
        digest( bundle, block, &*iter, &target_content_length, sizeof(target_content_length), r);
    }
            
    // start of data is where to start main digest
    offset = buf - iter->contents().buf();
    ASSERT(offset == iter->data_offset());
    /**********  end of preamble processing  **********/
            
            
    /**********  start content processing  **********/
    log_debug_p(log, "Ciphersuite::mutable_canonicalization_extension() starting content processing");
            
    // if it's the current block, we have to exclude security-result data.
    // Note that security-result-length *is* included

    // We need to handle this situation totally differently on
    // outgoing blocks, because the contents field
    // won't contain its data yet.
    if ( (&*iter) == block ) {
        BP_Local_CS *target_locals = dynamic_cast<BP_Local_CS*>(iter->locals());
        log_debug_p(log, "Ciphersuite::mutable_canonicalization_extension() this is the current block, so we need to skip the security result");

        // ciphersuite number and flags
        unsigned char temp[10];
        sdnv_len = SDNV::encode(cs_num(), temp, 10);
        digest(bundle, block, &*iter, temp, sdnv_len,r);

        cs_flags = target_locals->cs_flags();
        sdnv_len = SDNV::encode(cs_flags,temp, 10);
        digest(bundle, block, &*iter, temp, sdnv_len, r);
        log_debug_p(log, "Ciphersuite::mutable_canonicalization_extension() cs_flags=%llx", cs_flags);
        log_debug_p(log, "Ciphersuite::mutable_canonicalization_extension() we're up to the flags");
            
        // if there's a security-result we have to ease up to it
        if ( cs_flags & CS_BLOCK_HAS_CORRELATOR ) {
            sdnv_len = SDNV::encode(target_locals->correlator(), temp, 10);
            digest(bundle, block, &*iter, temp, sdnv_len, r);
        }

                
        if ( cs_flags & CS_BLOCK_HAS_PARAMS ) {
            uint64_t params_length;
            params_length = BP_Local_CS::length_of_result_or_params(target_locals->security_params_);
            LocalBuffer param_buf;
            BP_Local_CS::write_result_or_params(target_locals->security_params_, param_buf);
            log_debug_p(log, "Ciphersuite::mutable_canonicalization_extension(): params (of length %llu)", params_length);
            sdnv_len = SDNV::encode(params_length, temp, 10);
            digest(bundle, block, &*iter, temp, sdnv_len, r);
            digest(bundle, block, &*iter, param_buf.buf(), param_buf.len(), r);
        }
                
        // We exclude the length of the current security
        // result, unless this is PIB2, which requires it.
        if ( cs_num() == 2) {
            uint64_t result_length;
            result_length = BP_Local_CS::length_of_result_or_params(target_locals->security_result_)+target_locals->security_result_hole_;
            log_debug_p(log, "Ciphersuite::mutable_canonicalization_extension(): including the result length field of %llu", result_length);
            sdnv_len = SDNV::encode(result_length, temp, 10);
            digest(bundle, block, &*iter, temp, sdnv_len, r);
        }
                
    } else {
           
       log_debug_p(log, "Ciphersuite::mutable_canonicalization_extension() about to call iter->owner()->process with len=%d, offset=%d",len,offset); 
       iter->owner()->process( Ciphersuite_integ::digest,
                            bundle,
                            block,
                            &*iter,
                            offset,
                            len,
                            r);
    }
   log_debug_p(log, "Ciphersuite::mutable_canonicalization_extension() finished call to iter->owner()->process"); 
    /**********  end of content processing  **********/
    log_debug_p(log, "Ciphersuite::mutable_canonicalization_extension() digest done %p", &*iter);

    return BP_SUCCESS;
}


int Ciphersuite_integ::mutable_canonicalization_payload(const Bundle *bundle, BlockInfo *block, BlockInfo *iter, OpaqueContext*  r,char *dict) {
    u_char*         buf;
    u_int32_t       offset;
    size_t          len;
    size_t          sdnv_len;
    u_int64_t       target_flags;
    u_int64_t       flags_save;
    u_int64_t       eid_ref_count = 0LLU;
    const char*     ptr;
    size_t          plen;
    u_char          c;
    u_int64_t       mask = 0x57LLU;
    u_int64_t       target_content_length;

    buf = iter->contents().buf();
    len = iter->full_length();
            
            
    // Process block type
    c = *buf++;
    len--;
    digest( bundle, block, &*iter, &c, 1, r);
            
    // Process flags
    sdnv_len = SDNV::decode( buf, len, &target_flags);
    buf += sdnv_len;
    len -= sdnv_len;
                                
    flags_save = target_flags;
    target_flags &= mask;
    target_flags = htonq(target_flags);
    digest( bundle, block, &*iter, &target_flags, sizeof(target_flags), r);
            
    // EID list is next, starting with the count although we don't digest it
    if ( flags_save & BundleProtocol::BLOCK_FLAG_EID_REFS ) {                    
        sdnv_len = SDNV::decode(buf, len, &eid_ref_count);
        buf += sdnv_len;
        len -= sdnv_len;
                                        
        // each ref is a pair of SDNVs, so process 2 * eid_ref_count text pieces
        if ( eid_ref_count > 0 ) {
            for ( u_int32_t i = 0; i < (2 * eid_ref_count); i++ ) {
                sdnv_len = SDNV::decode(buf, len, &offset);
                buf += sdnv_len;
                len -= sdnv_len;
                        
                ptr = dict + offset;    //point at item in dictionary
                plen = strlen(ptr);     // length *without* NULL-terminator
                digest( bundle, block, &*iter, ptr, plen, r);
            }
        }       
    }

    // Process data length
    sdnv_len = SDNV::decode(buf, len, &target_content_length);
    buf += sdnv_len;
    len -= sdnv_len;
            
    target_content_length = htonq(target_content_length);
    digest( bundle, block, &*iter, &target_content_length, sizeof(target_content_length), r);
            
    // start of data is where to start main digest
    offset = buf - iter->contents().buf();
    ASSERT(offset == iter->data_offset());
    /**********  end of preamble processing  **********/
            
    /**********  start content processing  **********/
                               
   // It is critical to call process here, since the payload may not
   // be stored on the contents buffer (it may be on disk).  process
   // can handle that case. 
    iter->owner()->process( Ciphersuite_integ::digest,
                            bundle,
                            block,
                            &*iter,
                            offset,
                            len,
                            r);
    /**********  end of content processing  **********/
    log_debug_p(log, "Ciphersuite::mutable_canonicalization_payload() PAYLOAD_BLOCK done");
    return BP_SUCCESS;
}
    

void
Ciphersuite_integ::digest(const Bundle*    bundle,
                        const BlockInfo* caller_block,
                        const BlockInfo* target_block,
                        const void*      buf,
                        size_t           len,
                        OpaqueContext*   r)
{
    (void)bundle;
    (void)caller_block;
    (void)target_block;
    
    //log_debug_p(log, "Ciphersuite::digest() %zu bytes %s", len, buf2str((u_char*)buf, len).c_str());
    log_debug_p(log, "Ciphersuite::digest() %zu bytes", len);
    EVP_MD_CTX*       pctx = reinterpret_cast<EVP_MD_CTX*>(r);

    EVP_DigestUpdate( pctx, buf, len );
}

//----------------------------------------------------------------------
int
Ciphersuite_integ::read_primary(const Bundle*    bundle, 
                              BlockInfo*       block,
                              PrimaryBlock_ex& primary,
                              char**           dict)
{
    u_char*         buf;
    size_t          len;

    size_t primary_len = block->full_length();

    buf = block->writable_contents()->buf();
    len = block->writable_contents()->len();

    ASSERT(primary_len == len);

    primary.version = *(u_int8_t*)buf;
    buf += 1;
    len -= 1;
    
    if (primary.version != BundleProtocol::CURRENT_VERSION) {
        log_warn_p(log, "protocol version mismatch %d != %d",
                   primary.version, BundleProtocol::CURRENT_VERSION);
        return -1;
    }
    
#define PBP_READ_SDNV(location) { \
    int sdnv_len = SDNV::decode(buf, len, location); \
    if (sdnv_len < 0) \
        goto tooshort; \
    buf += sdnv_len; \
    len -= sdnv_len; }
    
    // Grab the SDNVs representing the flags and the block length.
    PBP_READ_SDNV(&primary.processing_flags);
    PBP_READ_SDNV(&primary.block_length);

    log_debug_p(log, "parsed primary block: version %d length %u",
                primary.version, block->data_length());    
    
    /*
     * it may be that the ASSERT which follows is not appropriate because we're doing this
     * on the outbound side and it seems that data_length() is the same as full_length().
     * But what's remaining should be the same as what is promised.
 	 * log_debug_p(log, "parsed primary block: version %d length %u full_length %u len remaining %zu",
 	 * primary.version, block->data_length(), block->full_length(), len);
  	 * What remains in the buffer should now be equal to what the block-length
  	 * field advertised.
  	 * ASSERT(len == block->data_length());
  	 */
    ASSERT(len == primary.block_length);
    
    // Read the various SDNVs up to the start of the dictionary.
    PBP_READ_SDNV(&primary.dest_scheme_offset);
    PBP_READ_SDNV(&primary.dest_ssp_offset);
    PBP_READ_SDNV(&primary.source_scheme_offset);
    PBP_READ_SDNV(&primary.source_ssp_offset);
    PBP_READ_SDNV(&primary.replyto_scheme_offset);
    PBP_READ_SDNV(&primary.replyto_ssp_offset);
    PBP_READ_SDNV(&primary.custodian_scheme_offset);
    PBP_READ_SDNV(&primary.custodian_ssp_offset);
    PBP_READ_SDNV(&primary.creation_time);
    PBP_READ_SDNV(&primary.creation_sequence);
    PBP_READ_SDNV(&primary.lifetime);
    PBP_READ_SDNV(&primary.dictionary_length);
    *dict = reinterpret_cast<char*>(buf);
    if (bundle->is_fragment()) {
        PBP_READ_SDNV(&primary.fragment_offset);
        PBP_READ_SDNV(&primary.original_length);
    }
#undef PBP_READ_SDNV
    return 0;
    
 tooshort:
    return -1;
}

int Ciphersuite_integ::get_dict(const Bundle *bundle, BlockInfo* block, char **dict) {
    PrimaryBlock_ex primary;
    return read_primary(bundle, block, primary, dict);
}

}
#endif
