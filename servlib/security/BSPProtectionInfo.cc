#include "BSPProtectionInfo.h"
#include "bundling/Bundle.h"
#include "BP_Local_CS.h"

#ifdef BSP_ENABLED
namespace dtn {
bool BSPProtectionInfo::src_matches(EndpointIDPatternNULL src, const Bundle *b) const {
    return BP_Local_CS::security_src_matches(secsrc, src, b, csnum);
}

bool BSPProtectionInfo::dest_matches(EndpointIDPatternNULL dest, const Bundle *b) const {
    return BP_Local_CS::security_dest_matches(secdest, dest, b, csnum);
}
}
#endif
