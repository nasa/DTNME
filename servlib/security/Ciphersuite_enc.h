#ifndef CIPHERSUITE_ENC_H
#define CIPHERSUITE_ENC_H
#ifdef BSP_ENABLED

#include "Ciphersuite.h"
#include "gcm/gcm.h"

namespace dtn {

class Ciphersuite_enc: public Ciphersuite {
  public:
	enum {
		op_invalid = 0,
		op_encrypt = 1,
		op_decrypt = 2
	};

	typedef struct {
		u_int8_t operation;
		gcm_ctx  c;
	} gcm_ctx_ex;


    virtual u_int32_t get_key_len() {
        return 128/8;
    };

    const static u_int32_t nonce_len = 12;
    const static u_int32_t salt_len  = 4;
    const static u_int32_t iv_len    = nonce_len - salt_len;
    const static u_int32_t tag_len   = 128/8;
    


    int encrypt_contents(u_char *salt, u_char *iv, gcm_ctx_ex ctx_ex, BlockInfo *block, LocalBuffer &store_result_here, u_char *tag);
    int add_sec_src_dest_to_eid_refs(BlockInfo *iter, string sec_src, string sec_dest, u_int16_t *cs_flags);

    u_char nonce[nonce_len];
    int encapsulate(BlockInfo *iter, BlockInfoVec *xmit_blocks, BP_Local_CS *src_locals, int block_type /*of the result, not the src*/, gcm_ctx_ex ctx_ex,LocalBuffer encrypted_key, bool first_block, bool use_correlator);

    // Convenience method for encapsulating subsequent blocks.  It
    // just calls encapsulate with the right parameters
    int encapsulate_subsequent(BlockInfo *iter, BlockInfoVec *xmit_blocks, BP_Local_CS *src_locals, int block_type, gcm_ctx_ex ctx_ex);

    int decapsulate(const Bundle *bundle, BlockInfo* iter, BP_Local_CS* locals, bool first_block, u_char *key, u_char *salt, u_int64_t &correlator);
    int recreate_block(LocalBuffer encap_block, BlockInfo* iter, const Bundle *bundle, int skip_eid_refs);


    virtual int decrypt(
						std::string   security_dest,
						const LocalBuffer&   enc_data,
						LocalBuffer&   result) = 0;
};
 
}
#endif
#endif
