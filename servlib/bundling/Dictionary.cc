/*
 *    Copyright 2006 Intel Corporation
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

#include "Dictionary.h"
#include <oasys/debug/Log.h>

namespace dtn {

//----------------------------------------------------------------------
Dictionary::Dictionary()
    : dict_(NULL), dict_length_(0), length_(0)
{
}

//----------------------------------------------------------------------
Dictionary::Dictionary(const oasys::Builder&)
    : dict_(NULL), dict_length_(0), length_(0)
{
}

//----------------------------------------------------------------------
Dictionary::~Dictionary()
{
    if (dict_) {
        free(dict_);
        dict_ = 0;
        dict_length_ = 0;
        length_ = 0;
    }
}

//----------------------------------------------------------------------
void
Dictionary::set_dict(const u_char* dict, u_int32_t length)
{
    ASSERT(dict_ == NULL);
    dict_ = (u_char*)malloc(length);
    memcpy(dict_, dict, length);
    length_ = dict_length_ = length;
}

//----------------------------------------------------------------------
bool
Dictionary::get_offset(const std::string& str, u_int32_t* offset)
{
    *offset = 0;
    while (*offset < length_) {
        u_int32_t len = strlen((char*)(&dict_[*offset]));
        if (!strncmp(str.c_str(), (char*)(&dict_[*offset]), len)) {
            if (str.length() == len) {
                return true; // already there
            }
        }
        *offset += (len + 1);
        ASSERT(*offset <= length_);
    }

    return false;
}
    
//----------------------------------------------------------------------
bool
Dictionary::get_offset(const std::string& str, u_int64_t* offset)
{
    *offset = 0;
    while (*offset < length_) {
        u_int64_t len = strlen((char*)(&dict_[*offset]));
        if (!strncmp(str.c_str(), (char*)(&dict_[*offset]), len)) {
            if (str.length() == len) {
                return true; // already there
            }
        }
        *offset += (len + 1);
        ASSERT(*offset <= length_);
    }

    return false;
}
    
//----------------------------------------------------------------------
void
Dictionary::add_str(const std::string& str)
{
    u_int32_t offset;
    if (get_offset(str, &offset)) {
        return;
    }
    
    if (dict_length_ < length_ + str.length() + 1) {
        do {
            dict_length_ = (dict_length_ == 0) ? 64 : dict_length_ * 2;
        } while (dict_length_ < length_ + str.length() + 1);
        
        dict_ = (u_char*)realloc(dict_, dict_length_);
    }

    memcpy(&dict_[length_], str.data(), str.length());
    dict_[length_ + str.length()] = '\0';
    length_ += str.length() + 1;
}


//----------------------------------------------------------------------
void
Dictionary::serialize(oasys::SerializeAction* a)
{
    a->process("dict", dict_, length_);
    a->process("dict_length", &dict_length_);
    a->process("length", &length_);
}

//----------------------------------------------------------------------
bool
Dictionary::extract_eid(EndpointID* eid,
                        u_int32_t scheme_offset,
                        u_int32_t ssp_offset)
{
    static const char* log = "/dtn/bundle/protocol";

    // If there's nothing in the dictionary, return
    if (dict_length_ == 0) {
        log_err_p(log, "cannot extract eid from zero-length dictionary");
        return false;
    }    

    if (scheme_offset >= (dict_length_ - 1)) {
	log_err_p(log, "illegal offset for scheme dictionary offset: "
                  "offset %d, total length %u",
                  scheme_offset, dict_length_);
	return false;
    }

    if (ssp_offset >= (dict_length_ - 1)) {
	log_err_p(log, "illegal offset for ssp dictionary offset: "
                  "offset %d, total length %u",
                  ssp_offset, dict_length_);
	return false;
    }
    
    eid->assign((char*)&dict_[scheme_offset],
                (char*)&dict_[ssp_offset]);

    if (! eid->valid()) {
	log_err_p(log, "invalid endpoint id '%s': "
                  "scheme '%s' offset %u/%u ssp '%s' offset %u/%u",
                  eid->c_str(),
                  eid->scheme_str().c_str(),
                  scheme_offset, dict_length_,
                  eid->ssp().c_str(),
                  ssp_offset, dict_length_);
        return false;                                                      
    }                                                                   
    
    log_debug_p(log, "parsed eid (offsets %u, %u) %s", 
                scheme_offset, ssp_offset, eid->c_str());
    return true;
}

//----------------------------------------------------------------------
bool
Dictionary::extract_eid(EndpointID* eid,
                        u_int64_t scheme_offset,
                        u_int64_t ssp_offset)
{
	u_int32_t scheme_offset32 = scheme_offset;
	u_int32_t ssp_offset32 = ssp_offset;
	
	return extract_eid(eid, scheme_offset32, ssp_offset32);
}

#if 0
// the routines here are slightly different from those
// in PrimaryBlockProcessor.cc although the functionality
// appears to be the same
///XXX-pl TODO check to see

//----------------------------------------------------------------------
void
PrimaryBlockProcessor::get_dictionary_offsets(DictionaryVector *dict,
                                              const EndpointID& eid,
                                              u_int16_t* scheme_offset,
                                              u_int16_t* ssp_offset)
{
    DictionaryVector::iterator iter;
    for (iter = dict->begin(); iter != dict->end(); ++iter) {
	if (iter->str == eid.scheme_str())
	    *scheme_offset = htons(iter->offset);

	if (iter->str == eid.ssp())
	    *ssp_offset = htons(iter->offset);
    }
}

//----------------------------------------------------------------------
bool
PrimaryBlockProcessor::extract_dictionary_eid(EndpointID* eid,
                                              const char* what,
                                              u_int16_t* scheme_offsetp,
                                              u_int16_t* ssp_offsetp,
                                              u_char* dictionary,
                                              u_int32_t dictionary_len)
{
    static const char* log = "/dtn/bundle/protocol";
    u_int16_t scheme_offset, ssp_offset;
    memcpy(&scheme_offset, scheme_offsetp, 2);
    memcpy(&ssp_offset, ssp_offsetp, 2);
    scheme_offset = ntohs(scheme_offset);
    ssp_offset = ntohs(ssp_offset);

    if (scheme_offset >= (dictionary_len - 1)) {
	log_err_p(log, "illegal offset for %s scheme dictionary offset: "
                  "offset %d, total length %u", what,
                  scheme_offset, dictionary_len);
	return false;
    }

    if (ssp_offset >= (dictionary_len - 1)) {
	log_err_p(log, "illegal offset for %s ssp dictionary offset: "
                  "offset %d, total length %u", what,
                  ssp_offset, dictionary_len);
	return false;
    }
    
    eid->assign((char*)&dictionary[scheme_offset],
                (char*)&dictionary[ssp_offset]);

    if (! eid->valid()) {
	log_err_p(log, "invalid %s endpoint id '%s': "
                  "scheme '%s' offset %u/%u ssp '%s' offset %u/%u",
                  what, eid->c_str(),
                  eid->scheme_str().c_str(),
                  scheme_offset, dictionary_len,
                  eid->ssp().c_str(),
                  ssp_offset, dictionary_len);
        return false;                                                      
    }                                                                   
    
    log_debug_p(log, "parsed %s eid (offsets %u, %u) %s", 
                what, scheme_offset, ssp_offset, eid->c_str());
    return true;
}

//----------------------------------------------------------------------
void
PrimaryBlockProcessor::debug_dump_dictionary(const char* bp, u_int32_t len,
                                             PrimaryBlock2* primary2)
{
#ifndef NDEBUG
    oasys::StringBuffer dict_copy;

    const char* end = bp + len;
    ASSERT(end[-1] == '\0');
    
    while (bp != end) {
        dict_copy.appendf("%s ", bp);
        bp += strlen(bp) + 1;
    }

    log_debug_p("/dtn/bundle/protocol",
                "dictionary len %zu, value: '%s'", len, dict_copy.c_str());
                  
    log_debug_p("/dtn/bundle/protocol",
                "dictionary offsets: dest %u,%u source %u,%u, "
                "custodian %u,%u replyto %u,%u",
                ntohs(primary2->dest_scheme_offset),
                ntohs(primary2->dest_ssp_offset),
                ntohs(primary2->source_scheme_offset),
                ntohs(primary2->source_ssp_offset),
                ntohs(primary2->custodian_scheme_offset),
                ntohs(primary2->custodian_ssp_offset),
                ntohs(primary2->replyto_scheme_offset),
                ntohs(primary2->replyto_ssp_offset));
#else
    (void)bp;
    (void)len;
    (void)primary2;
#endif
}
#endif /* zero */

} // namespace dtn
