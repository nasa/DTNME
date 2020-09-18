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

#ifndef _CIPHERSUITE_H_
#define _CIPHERSUITE_H_

#ifdef BSP_ENABLED

#include "bundling/BlockProcessor.h"
#include "SecurityConfig.h"

namespace dtn {

struct PrimaryBlock_ex;

class BP_Local_CS;

/**
 * Block processor superclass for ciphersutes
 * This level handles suite registration and
 * similar activities.  It provides code to check whether a bundle
 * satisifes a rule, and various helper methods for the individual
 * ciphersuites.
 */
    class Ciphersuite {
        friend class BP_Local_CS;
public:
    /// For local binary (non-string) stuff we use
    /// a scratch buffer but with minimal initial allocation
    typedef oasys::ScratchBuffer<u_char*, 16> LocalBuffer;

    /// @{ Import some typedefs from other classes
    typedef BlockInfo::list_owner_t list_owner_t;
    typedef BundleProtocol::status_report_reason_t status_report_reason_t;
    /// @}
    
    /**
     * Values for security flags that appear in the 
     * ciphersuite field.
     */
    typedef enum {
        CS_BLOCK_HAS_SOURCE      = 0x10,
        CS_BLOCK_HAS_DEST        = 0x08,
        CS_BLOCK_HAS_PARAMS      = 0x04,
        CS_BLOCK_HAS_CORRELATOR  = 0x02,
        CS_BLOCK_HAS_RESULT      = 0x01
    } ciphersuite_flags_t;
    
   
    /**
     * Values for security types that appear in the 
     * ciphersuite parameters and results fields.
     */
    typedef enum {
        CS_reserved0                        =  0,
        CS_IV_field                         =  1,
        CS_reserved2_field                  =  2,
        CS_key_info_field                   =  3,
        CS_fragment_offset_and_length_field =  4,
        CS_signature_field                  =  5,
        CS_reserved6                        =  6,
        CS_salt_field                       =  7,
        CS_PC_block_ICV_field               =  8,
        CS_reserved9                        =  9,
        CS_encap_block_field                = 10,
        CS_reserved11                       = 11
    } ciphersuite_fields_t;
    
    Ciphersuite();
    Ciphersuite(SecurityConfig *config);
    virtual ~Ciphersuite();

    // This deletes the security config, to avoid
    // memory leaks on shutdown.  These are unimportant, but make
    // using valgrind to locate important ones harder.
    static void shutdown(void);
   
    // Add a ciphersuite to the list of registered ciphersuites.
    static void register_ciphersuite(Ciphersuite* cs);

    // Return a pointer to a ciphersuite (assuming it is registered)
    // given its ciphersuite number.  This is how the block processors
    // locate the ciphersuites at each stage of processing.
    static Ciphersuite* find_suite(u_int16_t num);

    // Register all the ciphersuites we know about.
    static void init_default_ciphersuites(void);

    // This allows helper methods (either declared here or in abstract
    // children of this class) to refer to the ciphersuite number of
    // the current ciphersuite calling them.
    virtual u_int16_t cs_num() = 0;

    // Create a BP_Local_CS locals value for the block, given the raw
    // contents (for blocks we've received).  
    static void parse(BlockInfo* block);

    /**
     * The following methods correspond to the callback methods that 
     * block processors define.  The security block processors will call 
     * the ciphersuite's version of these methods if the block has a known
     * ciphersuite type.
     */ 
    
    virtual int consume(Bundle* bundle, BlockInfo* block,
                        u_char* buf, size_t len);

    virtual int reload_post_process(Bundle*       bundle,
                                    BlockInfoVec* block_list,
                                    BlockInfo*    block);

    virtual bool validate(const Bundle*           bundle,
                          BlockInfoVec*           block_list,
                          BlockInfo*              block,
                          status_report_reason_t* reception_reason,
                          status_report_reason_t* deletion_reason) = 0;

    virtual int prepare(const Bundle*    bundle,
                        BlockInfoVec*    xmit_blocks,
                        const BlockInfo* source,
                        const LinkRef&   link,
                        list_owner_t     list) = 0;
    
    virtual int generate(const Bundle*  bundle,
                         BlockInfoVec*  xmit_blocks,
                         BlockInfo*     block,
                         const LinkRef& link,
                         bool           last) = 0;
    
    virtual int finalize(const Bundle*  bundle, 
                         BlockInfoVec*  xmit_blocks, 
                         BlockInfo*     block, 
                         const LinkRef& link) = 0;
    /** 
      * End of the methods that correpsond to block processor callbacks
      */

    /**
      * Given a ciphersuite number, a security source, and a security
      * destination, this checks whether the bundle has a BSP block
      * with that ciphersuite number, security source, and security
      * destination.  The security source/destination can be
      * wildcarded, or NULL.  These correspond to the security rules
      * that can be set in TCL and the meanings are the same.
      */
    static bool check_validation(const Bundle*        bundle, 
                                 const BlockInfoVec*  block_list, 
                                 u_int16_t            num,
                                 EndpointIDPatternNULL &src,
                                 EndpointIDPatternNULL &dest);
                                 
    /**
     * Create a correlator for this block-list. Include part
     * of the fragment-offset is this bundle is a fragment.
     */
    static u_int64_t create_correlator(const Bundle*        bundle, 
                                       const BlockInfoVec*  block_list);

    /**
     * Convenience methods to test if the security source/destination
     * is an endpoint registered at the local node.
     */
    static bool source_is_local_node(const Bundle*    bundle,
                                     const BlockInfo* block);

    static bool destination_is_local_node(const Bundle*    bundle,
                                          const BlockInfo* block);

    // SecurityConfig uses these in deciding whether a bundle's
    // security configuration is "consistent."  These find the
    // outermost security destination when there are layers of
    // encapsulated payloads or encapsulted extension blocks.
    // find_last_secdeste is conceptually broken, because each
    // extension block may have its own outermost destination.  We
    // actually need to run this per extension block we intend to
    // encapsulate.
    static EndpointID find_last_secdestp(const Bundle *bundle);
    static EndpointID find_last_secdeste(const Bundle *bundle);
    
    // Creates a new locals variable for the block if it doesn't
    // currently have one. 
    virtual void init_locals(BlockInfo* block);

    // Convenience method for the individual ciphersuites.  This is
    // used to create a new outgoing block from an incoming block by
    // copying block->source() to block.  
    // It calls generate preamble using values from block->source() and
    // copies the contents buffer from block->source();
    // This is called in ciphersuites' generate methods on blocks with
    // list type LIST_RECEIVED.  It pays no attention to BP_Local_CS
    // values, it just copies the block as opaque data.
    static int copy_block_untouched(BlockInfo *block, BlockInfoVec *xmit_blocks, bool last);

    // Convenience method that checks whether the block's security
    // destination matches our local eid.
    static bool we_are_block_sec_dest(const BlockInfo *source);

    // Convenience method for the ciphersuites and block processors.
    // This sets the csnum, eid_list, list_owner, and security
    // src/dest of bi from source, csnum, and list.   This is normally used in
    // block processors or ciphersuites prepare methods.
    static int create_bsp_block_from_source(BlockInfo &bi,const Bundle *bundle, const BlockInfo *source, BlockInfo::list_owner_t list, int csnum);


    // Convenience methods to produce a string representation of a
    // memory buffer as a string of hex digits.  These are used for
    // logging.
    static string buf2str(const u_char *buf, size_t len);
    static string buf2str(const LocalBuffer buf);
    static string buf2str(const BlockInfo::DataBuffer buf);


    // The global security config (anyone who wants it refers to it
    // through us).
    static SecurityConfig *config;
    
protected:
    
    /**
     * Generate the standard preamble for the given block type, flags,
     * EID-list and content length.  This is a convenience method that
     * calls the block processor generate_preamble method under the
     * hood.
     */
    static void generate_preamble(BlockInfoVec*  xmit_blocks, 
                           BlockInfo* block,
                           u_int8_t type,
                           u_int64_t flags,
                           u_int64_t data_length);


    // this is a very clunky way to get the "base" portion of the eid.
    static EndpointID base_portion(const EndpointID &in);
private:
    // Helper functions for check_validation (only called from there)
    static bool check_esb_metadata(BlockInfoVec::const_iterator iter, EndpointIDPatternNULL src, EndpointIDPatternNULL dest, const Bundle *b, int csnum);
    static bool check_src_dest_match(BP_Local_CS* locals, EndpointIDPatternNULL src, EndpointIDPatternNULL dest, const Bundle *bundle);


private:

    /**
     * Array of registered BlockProcessor-derived handlers for
     * various ciphersuites -- fixed size for now [maybe make adjustable later]
     */
    static Ciphersuite* ciphersuites_[1024];
    
    static bool inited;
};

} // namespace dtn

#define CS_FAIL_IF(x)                                              \
    do { if ( (x) ) {                                              \
        log_err_p(log, "TEST FAILED (%s) at %s:%d\n",              \
                (#x), __FILE__, __LINE__);                         \
        goto fail;                                                 \
    } } while(0);

#define CS_FAIL_IF_NULL(x)                                         \
    do { if ( (x) == NULL) {                                       \
        log_err_p(log, "TEST FAILED (%s == NULL) at %s:%d\n",      \
                (#x), __FILE__, __LINE__);                         \
        goto fail;                                                 \
    } } while(0);

#endif /* BSP_ENABLED */

#endif /* _CIPHERSUITE_H_ */
