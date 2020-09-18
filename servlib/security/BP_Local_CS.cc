/*
 *    Copyright 2006 SPARTA Inc
 * 
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 * 
 *        http://www.apache.org/licenses/LICENSE-2.0
 * 
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#ifdef BSP_ENABLED

#include "Ciphersuite.h"
#include "BP_Local_CS.h"
#include "BA_BlockProcessor.h"
#include "Ciphersuite_BA1.h"
#include "Ciphersuite_BA5.h"
#include "Ciphersuite_BA9.h"
#include "PI_BlockProcessor.h"
#include "Ciphersuite_PI2.h"
#include "Ciphersuite_PI6.h"
#include "Ciphersuite_PI10.h"
#include "PC_BlockProcessor.h"
#include "Ciphersuite_PC3.h"
#include "Ciphersuite_PC7.h"
#include "Ciphersuite_PC11.h"
#include "ES_BlockProcessor.h"
#include "Ciphersuite_ES4.h"
#include "Ciphersuite_ES8.h"
#include "Ciphersuite_ES12.h"
#include "bundling/Bundle.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleProtocol.h"
#include "contacts/Link.h"
#include "bundling/SDNV.h"

namespace dtn {

static const char* log = "/dtn/bundle/ciphersuite";

int BP_Local_CS::add_param_or_result_tuple(map<uint8_t, LocalBuffer> &target, int type, size_t length, u_char* value) {
    log_debug_p(log, "add_param_or_result_tuple adding a parameter of type=%d length=%d", type, length);
    target[type].reserve(length);
    target[type].set_len(length);
    memcpy(target[type].buf(), value, length);
    return 1;
}


int BP_Local_CS::parse_params_or_result(map<uint8_t, LocalBuffer> &target, u_char* buf, size_t len) {
    log_debug_p(log, "parse_params_or_result being called with len=%d", len);
    size_t field_length;
    size_t sdnv_len;
    uint8_t item_type;
    while ( len > 0 ) {
        log_debug_p(log, "parse_params_or_result looping with len=%d", len);
        item_type = *buf++;
        --len;
        if(len < 1) {
            return -1;
        }
        sdnv_len = SDNV::decode(buf, len, &field_length);
        buf += sdnv_len;
        if(len < sdnv_len) {
            return -1;
        }
        len -= sdnv_len;

        add_param_or_result_tuple(target, item_type, field_length, buf);
        buf += field_length;
        if(len < field_length) {
            return -1;
        }
        len -= field_length;
    }
    return 0;
}
size_t BP_Local_CS::length_of_result_or_params(map<uint8_t, LocalBuffer> &m) {
    map<uint8_t, LocalBuffer>::iterator it;
    size_t length=0;
    for(it=m.begin();it!=m.end();it++) {
        length += 1 + SDNV::encoding_len(it->second.len()) + it->second.len();
    }
    return length;
}
int BP_Local_CS::write_result_or_params(map<uint8_t, LocalBuffer> &m, u_char *buf, size_t len) {
     map<uint8_t, LocalBuffer>::iterator it;
     size_t sdnv_len;
    for(it=m.begin();it!=m.end();it++) {
        *buf++ = it->first;
        len--;
        sdnv_len = SDNV::encode(it->second.len(), buf, len);
        buf += sdnv_len;
        len -= sdnv_len;

        memcpy(buf, it->second.buf(), it->second.len());
        buf += it->second.len();
        len -= it->second.len();
    }
    return 1; 
}
int BP_Local_CS::write_result_or_params(map<uint8_t, LocalBuffer> &m, LocalBuffer &buffer) {
     map<uint8_t, LocalBuffer>::iterator it;
     size_t len;
     u_char *buf;
     size_t sdnv_len;

    for(it=m.begin();it!=m.end();it++) {
        len = 1+SDNV::encoding_len(it->second.len())+it->second.len();
        buffer.reserve(len);
        buffer.set_len(len);
        buf = buffer.buf();
        *buf++ = it->first;
        len--;
        sdnv_len = SDNV::encode(it->second.len(), buf, len);
        buf += sdnv_len;
        len -= sdnv_len;

        memcpy(buf, it->second.buf(), it->second.len());
        buf += it->second.len();
        len -= it->second.len();
    }
    return 1; 
}

//----------------------------------------------------------------------
BP_Local_CS::BP_Local_CS()
    : BP_Local(),
      cs_flags_(0), 
      correlator_(0LL),
      owner_cs_num_(0),
      security_params_(),
      security_result_(),
      security_result_hole_(0)
{
}

//----------------------------------------------------------------------
BP_Local_CS::BP_Local_CS(const BP_Local_CS& b)
    : BP_Local(),
      cs_flags_(b.cs_flags_), 
      correlator_(b.correlator_),
      owner_cs_num_(b.owner_cs_num_),
      security_params_(b.security_params_),
      security_result_(b.security_result_),
      security_result_hole_(b.security_result_hole_)
    //XXX-pl  might need more items copied
{
}

//----------------------------------------------------------------------
BP_Local_CS::~BP_Local_CS()
{
}

//----------------------------------------------------------------------
void
BP_Local_CS::set_key(u_char* k, size_t len)
{
    key_.reserve(len);
    key_.set_len(len);
    memcpy(key_.buf(), k, len);
}

//----------------------------------------------------------------------
void
BP_Local_CS::set_salt(u_char* s, size_t len)
{
    salt_.reserve(len);
    salt_.set_len(len);
    memcpy(salt_.buf(), s, len);
}

//----------------------------------------------------------------------
void
BP_Local_CS::set_iv(u_char* iv, size_t len)
{
    iv_.reserve(len);
    iv_.set_len(len);
    memcpy(iv_.buf(), iv, len);
}


 
int BP_Local_CS::add_security_param(int type, size_t length, u_char* value) {
    set_cs_flags(cs_flags() | Ciphersuite::CS_BLOCK_HAS_PARAMS);
    return add_param_or_result_tuple(security_params_, type, length, value);
}
int BP_Local_CS::add_security_result(int type, size_t length, u_char* value) {
    set_cs_flags(cs_flags() | Ciphersuite::CS_BLOCK_HAS_RESULT);
    return add_param_or_result_tuple(security_result_, type, length, value);
}

int BP_Local_CS::add_encapsulated_block(size_t blen, u_char *b, size_t tag_len, u_char* tag) {
    log_debug_p(log, "BP_Local_CS::add_encapsulated_block adding encapsulated block of len %d with tag of len %d", blen, tag_len);
    LocalBuffer temp;
    temp.reserve(tag_len+blen);
    temp.set_len(tag_len+blen);
    memcpy(temp.buf(), b, blen);
    memcpy(temp.buf()+blen, tag, tag_len);
    security_result_[Ciphersuite::CS_encap_block_field] = temp;
    set_cs_flags(cs_flags() | Ciphersuite::CS_BLOCK_HAS_RESULT);
    return 1;
}

   
int BP_Local_CS::add_security_result_space(size_t length) {
    security_result_hole_ += 1+ SDNV::encoding_len(length) + length;
    set_cs_flags(cs_flags() | Ciphersuite::CS_BLOCK_HAS_RESULT);
    return 1;
}

int BP_Local_CS::reset_security_result() {
    security_result_.empty();
    security_result_hole_ = 0;
    set_cs_flags(cs_flags() & (0xFFFFFFFF-Ciphersuite::CS_BLOCK_HAS_RESULT));
    return 1;
}

int BP_Local_CS::serialize(BlockInfoVec* xmit_blocks, BlockInfo *block, bool last) {
    log_debug_p(log, "BP_Local_CS::serialize: start");
    size_t length=0;
    if(cs_flags() & Ciphersuite::CS_BLOCK_HAS_CORRELATOR) {
        length += SDNV::encoding_len(correlator());
    }
    length += SDNV::encoding_len(owner_cs_num());
    length += SDNV::encoding_len(cs_flags());
    log_debug_p(log, "BP_Local_CS::serialize: dealt with correlator and flags");
    size_t params_len = length_of_result_or_params(security_params_); //writable_security_params()->len();
    size_t result_len = length_of_result_or_params(security_result_) + security_result_hole_; //writable_security_result()->len();
    log_debug_p(log, "BP_Local_CS::serialize: params_len=%d result_len=%d (hole=%d)", params_len, result_len, security_result_hole_);

    if(cs_flags() & Ciphersuite::CS_BLOCK_HAS_PARAMS) {
        log_debug_p(log, "BP_Local_CS::serialize: adding params_len to total length");
        length += SDNV::encoding_len(params_len) + params_len;
    }
    if(cs_flags() & Ciphersuite::CS_BLOCK_HAS_RESULT) {
        log_debug_p(log, "BP_Local_CS::serialize: adding resilt_len to total length");
        length += SDNV::encoding_len(result_len) + result_len;
    }
    DataBuffer *contents = block->writable_contents();

    // Just in case we're reserializing!
    contents->set_len(0);

    Ciphersuite::generate_preamble(xmit_blocks, 
    		block,
    		block->owner()->block_type(),
    		//BundleProtocol::BLOCK_FLAG_DISCARD_BUNDLE_ONERROR |  //This causes non-BSP nodes to delete the bundle
    		(last ? BundleProtocol::BLOCK_FLAG_LAST_BLOCK : 0),
    		length);
    log_debug_p(log, "BP_Local_CS::serialize: about to start writing block contents");

    contents->reserve(block->data_offset() + length);
    contents->set_len(block->data_offset() + length);

    u_char *buf = block->writable_contents()->buf() + block->data_offset();
    size_t len = length;
    size_t sdnv_len = 0;

    // Assemble data into block contents.

    // ciphersuite number and flags
    sdnv_len = SDNV::encode(owner_cs_num(), buf, len);
    CS_FAIL_IF(sdnv_len <= 0);
    buf += sdnv_len;
    len -= sdnv_len;

    sdnv_len = SDNV::encode(cs_flags(), buf, len);
    CS_FAIL_IF(sdnv_len <= 0);
    buf += sdnv_len;
    len -= sdnv_len;

    if ( cs_flags() & Ciphersuite::CS_BLOCK_HAS_CORRELATOR ) {
    	// correlator
        log_debug_p(log, "BP_Local_CS: serializing correlator");
    	sdnv_len = SDNV::encode(correlator(), buf, len);
    	CS_FAIL_IF(sdnv_len <= 0);
    	buf += sdnv_len;
    	len -= sdnv_len;
    }

    log_debug_p(log, "BP_Local_CS: About to consider params/result");

    if( cs_flags() & Ciphersuite::CS_BLOCK_HAS_PARAMS) {
        // length of params
        log_debug_p(log, "BP_Local_CS: serializing security params");
        sdnv_len = SDNV::encode(params_len, buf, len);
        CS_FAIL_IF(sdnv_len <= 0);
        buf += sdnv_len;
        len -= sdnv_len;
        write_result_or_params(security_params_, buf, len);
        buf += params_len;
        len -= params_len;
    }
    if( cs_flags() & Ciphersuite::CS_BLOCK_HAS_RESULT) {
        // length of result
        log_debug_p(log, "BP_Local_CS: serializing security result");
        sdnv_len = SDNV::encode(result_len, buf, len);
        CS_FAIL_IF(sdnv_len <= 0);
        buf += sdnv_len;
        len -= sdnv_len;
        write_result_or_params(security_result_, buf, len);
        buf += result_len;
        len -= result_len;
    }
    log_debug_p(log, "BP_Local_CS::serialize: about to assert len %d==0", len);
    ASSERT(len == 0);
    return 1;
fail:
    return 0;


}

int BP_Local_CS::add_missing_security_result(BlockInfo *block, int type, size_t length, u_char *value) {
    log_debug_p(log, "BP_Local_CS::add_missing_security_result: adding missing security result of type=%d length=%d", type, length);
    size_t len_to_add = 1 + SDNV::encoding_len(length) + length;
    add_security_result(type, length, value);
    security_result_hole_ -= len_to_add;

    u_char *buf = block->contents().buf();
    size_t len = block->contents().len();
    size_t sdnv_len;
    buf += len;
    buf -= len_to_add;
    len = len_to_add;

    *buf++ = type;
    len--;
    sdnv_len = SDNV::encode(length, buf, len);
    buf += sdnv_len;
    len -= sdnv_len;

    memcpy(buf, value, length);
    return 1;
}


int BP_Local_CS::add_fragment_param(const Bundle *bundle) {
     u_char fragment_item[22];
     u_char *ptr = NULL;
     size_t rem;
     size_t temp;
     ptr = &fragment_item[2];
     rem = sizeof(fragment_item);
     temp = SDNV::encode(bundle->frag_offset(), ptr, rem);
     ptr += temp;
     rem -= temp;
     temp += SDNV::encode(bundle->payload().length(), ptr, rem);
     return add_security_param(Ciphersuite::CS_fragment_offset_and_length_field, temp, fragment_item);
}

bool BP_Local_CS::get_security_result(LocalBuffer &save, int type) {
    map<uint8_t, LocalBuffer>::iterator it;
    it = security_result_.find(type);
    if(it!= security_result_.end()) {
        save = it->second;
        return true;
    }
    return false;
}

bool BP_Local_CS::get_fragment_offset(u_int64_t &frag_offset_, u_int64_t &orig_length_) {
    u_char * buf;
    size_t len;
    size_t sdnv_len;
    map<uint8_t, LocalBuffer>::iterator it;
    it = security_result_.find(Ciphersuite::CS_fragment_offset_and_length_field);
    if(it!= security_result_.end()) {
        buf=it->second.buf();
        len=it->second.len();
    	sdnv_len = SDNV::decode(buf, len, &frag_offset_);
    	buf += sdnv_len;
    	len -= sdnv_len;
    	sdnv_len = SDNV::decode(buf, len, &orig_length_);
    	buf += sdnv_len;
    	len -= sdnv_len;
        return true;
   	}
    return false;
 
}
 
bool BP_Local_CS::get_security_param(LocalBuffer &save, int type) {
    map<uint8_t, LocalBuffer>::iterator it;
    it = security_params_.find(type);
    if(it!= security_params_.end()) {
        save = it->second;
        return true;
    }
    return false;
}
bool BP_Local_CS::check_for_security_result(int type) {
    if(security_result_.count(type) > 0 ) {
        return true;
    }
    return false;
}
bool BP_Local_CS::check_for_security_param(int type) {
    if(security_params_.count(type) > 0 ) {
        return true;
    }
    return false;
}

EndpointID BP_Local_CS::security_src_eid(const Bundle *b) {
    if(cs_flags() & Ciphersuite::CS_BLOCK_HAS_SOURCE ) {
        return EndpointID(security_src_);
    }

    if(SecurityConfig::get_block_type(owner_cs_num()) == BundleProtocol::BUNDLE_AUTHENTICATION_BLOCK) {
        return b->prevhop();
    } else {
        return b->source();
    }
}

EndpointID BP_Local_CS::security_dest_eid(const Bundle *b,const LinkRef &link) {
    if(cs_flags() & Ciphersuite::CS_BLOCK_HAS_DEST ) {
        return EndpointID(security_dest_);
    }
    if(SecurityConfig::get_block_type(owner_cs_num()) == BundleProtocol::BUNDLE_AUTHENTICATION_BLOCK) {
        return EndpointID(link->nexthop());
    } else {
        return b->dest();
    }
}

EndpointID BP_Local_CS::security_dest_eid(const Bundle *b) {
    if(cs_flags() & Ciphersuite::CS_BLOCK_HAS_DEST ) {
        return EndpointID(security_dest_);
    }
    if(SecurityConfig::get_block_type(owner_cs_num()) == BundleProtocol::BUNDLE_AUTHENTICATION_BLOCK) {
        return EndpointID(BundleDaemon::instance()->local_eid());
    }
    return b->dest();
}


string BP_Local_CS::security_src_host(const Bundle *b) {
    return security_src_eid(b).uri().host();
}

string BP_Local_CS::security_dest_host(const Bundle *b, const LinkRef &link) {
    return security_dest_eid(b, link).uri().host();
}

string BP_Local_CS::security_dest_host(const Bundle *b) {
    return security_dest_eid(b).uri().host();
}

bool BP_Local_CS::security_src_matches(EndpointID sec_src, EndpointIDPatternNULL src, const Bundle *b, int csnum) {
    if(src.isnull) {
        if(SecurityConfig::get_block_type(csnum) == BundleProtocol::BUNDLE_AUTHENTICATION_BLOCK) {
            return sec_src == Ciphersuite::base_portion(b->prevhop());
        } else {
            return sec_src == Ciphersuite::base_portion(b->source());
        }
    } else {
        return src.pat.match(sec_src);
    }

}
bool BP_Local_CS::security_dest_matches(EndpointID sec_dest, EndpointIDPatternNULL dest, const Bundle *b, int csnum) {
    if(dest.isnull) {
        if(SecurityConfig::get_block_type(csnum) == BundleProtocol::BUNDLE_AUTHENTICATION_BLOCK) {
            return sec_dest == Ciphersuite::base_portion(BundleDaemon::instance()->local_eid());
        } else {
            return sec_dest == Ciphersuite::base_portion(b->dest());
        }
    } else {
        return dest.pat.match(sec_dest);
    }
 
}
bool BP_Local_CS::security_src_matches(EndpointIDPatternNULL src, const Bundle *b) {
    return security_src_matches(Ciphersuite::base_portion(security_src_eid(b)), src, b, owner_cs_num());
}

bool BP_Local_CS::security_dest_matches(EndpointIDPatternNULL dest, const Bundle *b) {
    return security_dest_matches(Ciphersuite::base_portion(security_dest_eid(b)), dest, b, owner_cs_num());
}
} // namespace dtn

#endif /* BSP_ENABLED */
