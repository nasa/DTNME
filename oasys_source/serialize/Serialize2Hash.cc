#ifdef HAVE_CONFIG_H
#  include <oasys-config.h>
#endif

#include "Serialize2Hash.h"

#include "../serialize/MarshalSerialize.h"
#include "../util/jenkins_hash.h"

namespace oasys {

//----------------------------------------------------------------------------
Serialize2Hash::Serialize2Hash(const SerializableObject* obj)
    : obj_(obj)
{
    MarshalSize sizer(Serialize::CONTEXT_LOCAL);
    sizer.action(obj_);

    Marshal ms(Serialize::CONTEXT_LOCAL, buf_.buf(sizer.size()), buf_.len());
    ms.action(obj_);
    ASSERT(! ms.error());
}
    
//----------------------------------------------------------------------------
u_int32_t 
Serialize2Hash::get_hash32() const
{
    return jenkins_hash(buf_.buf(), buf_.len(), 0);
}
    
//----------------------------------------------------------------------------
void 
Serialize2Hash::get_hashMD5(const char* outbuf) const
{
    (void) outbuf;
    // XXX/bowei -- TODO
    NOTIMPLEMENTED;
}

} // namespace oasys
