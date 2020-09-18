#ifndef __BSPPROTECTIONINFO_H__
#define __BSPPROTECTIONINFO_H__

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#ifdef BSP_ENABLED 

#include "oasys/serialize/Serialize.h"
#include "security/SecurityConfig.h"


namespace dtn {
class Bundle;
class BSPProtectionInfo: public oasys::SerializableObject {
    public:
      int csnum;
      EndpointID secsrc;
      EndpointID secdest;
    BSPProtectionInfo() {
    };
    BSPProtectionInfo(oasys::Builder& builder) {
        (void) builder;
    };
    virtual void serialize(oasys::SerializeAction* a) {
        a->process("csnum",&csnum);
        a->process("secsrc", &secsrc);
        a->process("secdest", &secdest);
    }
    bool src_matches(EndpointIDPatternNULL src, const Bundle *b) const;
    bool dest_matches(EndpointIDPatternNULL dest, const Bundle *b) const;
};

}
#endif
#endif
