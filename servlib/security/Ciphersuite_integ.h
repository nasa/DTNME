#ifndef __CIPHERSUITE_INTEG_H__
#define __CIPHERSUITE_INTEG_H__

#include "Ciphersuite.h"

#ifdef BSP_ENABLED

namespace dtn {
struct PrimaryBlock_ex;

class Ciphersuite_integ: public Ciphersuite {
  public:
    int mutable_canonicalization_primary(const Bundle *bundle, BlockInfo *block, BlockInfo *iter /*This is a pointer to the primary block*/, OpaqueContext*  r, char **dict);

    int mutable_canonicalization_extension(const Bundle *bundle, BlockInfo *block, BlockInfo *iter, OpaqueContext*  r,char *dict);
    
    int mutable_canonicalization_payload(const Bundle *bundle, BlockInfo *block, BlockInfo *iter, OpaqueContext*  r,char *dict);

    static void digest(const Bundle*    bundle,
                        const BlockInfo* caller_block,
                        const BlockInfo* target_block,
                        const void*      buf,
                        size_t           len,
                        OpaqueContext*   r);

    static int read_primary(const Bundle*    bundle, 
                              BlockInfo*       block,
                              PrimaryBlock_ex& primary,
                              char**           dict);
    static int get_dict(const Bundle *bundle, BlockInfo* block, char **dict);

};
}
#endif
#endif
