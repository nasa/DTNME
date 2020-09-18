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

#ifndef _BP_LOCAL_CS_H_
#define _BP_LOCAL_CS_H_

#ifdef BSP_ENABLED

#include "bundling/BlockProcessor.h"
#include "SecurityConfig.h"
#include "Ciphersuite.h"

namespace dtn {

class Ciphersuite;

class BP_Local_CS : public BP_Local {
public:
    friend class Ciphersuite;
    friend class Ciphersuite_integ; // The mutable canonicalization routines need to know about 
                                    // our internal data structures when they process the current block
    /// Use the same LocalBuffer type as Ciphersuite
    typedef Ciphersuite::LocalBuffer LocalBuffer;
    
    BP_Local_CS();
    BP_Local_CS(const BP_Local_CS&);
    virtual ~BP_Local_CS();



    // Add security parameter/result to the set that will be
    // serialized (this also sets the csflag that indicates that a sec
    // res/param exists.
    int add_security_param(int type, size_t length, u_char* value);
    int add_security_result(int type, size_t length, u_char* value);
    // Shortcut that adds a security result of type encapsulated
    // block.  These are actually two parts, a tag and a block.  They
    // are concatenated with no delimiter.
    int add_encapsulated_block(size_t blen, u_char *b, size_t tag_len, u_char* tag);
    // Shortcut that adds fragment info as a security parameter.  It
    // assumes the bundle has the fragment info already (as
    // bundle->frag_offset())
    int add_fragment_param(const Bundle *bundle);

    // This uses security_result_hole_ to leave space in the
    // serialization for a security result to be added later (some
    // ciphersuites need to serialize their block in generate, but
    // don't have one of the security results available until
    // finalize).
    int add_security_result_space(size_t length);

    // For ciphersuites which change their mind about including
    // something in the security result between callbacks.  This
    // empties our data structure of security result fields, sets the
    // hole length to zero, and resets the csflag to not indicate any
    // security results.  This allows ciphersuites to add results,
    // call serialize, enter a new callback, change their mind, get
    // rid of all the security results, add any new ones they like,
    // and reserialize.
    int reset_security_result();

    // Serialize produces the block contents buffer out of the
    // owner_cs_num_, cs_flags_, security_params_, security_result_,
    // security_result_hole, and correlator_.  You must set the
    // appropriate cs_flag if you have a correlator.
    // This is the opposite of Ciphersuite::parse, which produces a
    // BP_Local_CS out of a block contents buffer.
    int serialize(BlockInfoVec* xmit_blocks, BlockInfo *block, bool last);

    // This fills the given security result into the blank we left in
    // contents when we serialized.  This is normally called in
    // finalize when serialize was called in generate.
    int add_missing_security_result(BlockInfo *block, int type, size_t length, u_char *value);
    

    // Get the security result/security parameter of the given type
    // (storing it in save).  Return value is false if the block
    // doesn't have a result/parameter of the given type.
    bool get_security_result(LocalBuffer &save, int type);
    bool get_security_param(LocalBuffer &save, int type);
    // Similar to the above, but fragment offsets have a very specific
    // form.
    bool get_fragment_offset(u_int64_t &frag_offset_, u_int64_t &orig_length_);

    // Check if the block has a security parameter/result of the given
    // type.
    bool check_for_security_result(int type);
    bool check_for_security_param(int type);

    // Return the logical security src.  If the bundle has a security
    // source set, this returns it.  If it does not, it returns the
    // previous hop eid for BAB blocks and the bundle source for all
    // others. 
    EndpointID security_src_eid(const Bundle *b);
    // Return the logical security dest.  If the bundle has a security
    // destination set, this returns it.  If it does not, it returns
    // the nevt hop eid for BAB blocks (retrieved from the link) and
    // the bundle dest for all others.
    EndpointID security_dest_eid(const Bundle *b, const LinkRef& link);
    // As above, but if the block is a BAB block, returns my own EID.
    EndpointID security_dest_eid(const Bundle *b);



    // Same as the above three functions, but call .uri().host() on
    // the results so that we get a string representing the host part
    // of the eid only.
    std::string security_src_host(const Bundle *b);
    std::string security_dest_host(const Bundle *b, const LinkRef& link);
    std::string security_dest_host(const Bundle *b);


    // Check whether the logical security src/dest match the given pattern.  A
    // NULL src pattern is interpreted as the previous hop for BAB and the
    // bundle source for everything else.  A NULL dest pattern  is interpreted as the
    // current node for BAB and the bundle destination for everything
    // else.
    bool security_src_matches(EndpointIDPatternNULL src, const Bundle *b);
    bool security_dest_matches(EndpointIDPatternNULL dest, const Bundle *b);

    // Helper methods for the above that take the logical security
    // src/dest as input.
    static bool security_src_matches(EndpointID sec_src, EndpointIDPatternNULL src, const Bundle *b, int csnum);
    static bool security_dest_matches(EndpointID sec_dest, EndpointIDPatternNULL dest, const Bundle *b, int csnum); 


    /// @{ Accessors
    u_int16_t           cs_flags()               const { return cs_flags_; }
    u_int16_t           owner_cs_num()           const { return owner_cs_num_; }
    u_int64_t           correlator()             const { return correlator_; }
    u_int16_t           correlator_sequence()    const { return correlator_sequence_; }

    // key/salt/iv are stored as a courtesy to ciphersuites that need
    // to save them between callbacks.  They are not included in
    // serialization (since some ciphersuites will store info here
    // that shouldn't be serialized in this particular block).
    const LocalBuffer&  key()                    const { return key_; }
    const LocalBuffer&  salt()                   const { return salt_; }
    const LocalBuffer&  iv()                     const { return iv_; }

    std::string         security_src()           const { return security_src_; }
    std::string         security_dest()          const { return security_dest_; }
    BlockInfo::list_owner_t list_owner()         const { return list_owner_; }
    /// @}


    /// @{ Mutating accessors
    void         set_cs_flags(u_int16_t f)            { cs_flags_ = f; }
    void         set_owner_cs_num(u_int16_t n)        { owner_cs_num_ = n; }
    
    // key/salt/iv are stored as a courtesy to ciphersuites that need
    // to save them between callbacks.  They are not included in
    // serialization (since some ciphersuites will store info here
    // that shouldn't be serialized in this particular block).
    void         set_key(u_char* k, size_t len);
    void         set_salt(u_char* s, size_t len);
    void         set_iv(u_char* iv, size_t len);
    void         set_correlator(u_int64_t c)          { correlator_ = c; }
    void         set_correlator_sequence(u_int16_t c) { correlator_sequence_ = c; }
    void         set_security_src(std::string s)      { security_src_ = s; }
    void         set_security_dest(std::string d)     { security_dest_ = d; }
    void         set_list_owner(BlockInfo::list_owner_t o) { list_owner_ = o; }
    /// @}

    
protected:
   
    // Since the security params and security results data structures
    // are identical, we have a bunch of helper methods that edit them
    // so that the public functions above aren't full of duplicated
    // code. 
    static int add_param_or_result_tuple(map<uint8_t, LocalBuffer> &target, int type, size_t length, u_char* value);
    static int parse_params_or_result(map<uint8_t, LocalBuffer> &target, u_char* buf, size_t len);
    static int write_result_or_params(map<uint8_t, LocalBuffer> &m, u_char *buf, size_t len);
    static int write_result_or_params(map<uint8_t, LocalBuffer> &m, LocalBuffer &buffer);
    static size_t length_of_result_or_params(map<uint8_t, LocalBuffer> &m);

    u_int16_t               cs_flags_;
    u_int16_t               correlator_sequence_;
    u_int64_t               correlator_;
    LocalBuffer             key_;    
    LocalBuffer             iv_;    
    LocalBuffer             salt_;    
    std::string             security_src_;
    std::string             security_dest_;
    BlockInfo::list_owner_t list_owner_;
    u_int16_t               owner_cs_num_; 
    map<uint8_t, LocalBuffer> security_params_;
    map<uint8_t, LocalBuffer> security_result_;
    size_t security_result_hole_;

};  /* BP_Local_CS */

} // namespace dtn
#endif /* BSP_ENABLED */

#endif /* _BP_LOCAL_CS_H_ */
