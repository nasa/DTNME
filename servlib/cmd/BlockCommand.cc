#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include "BlockCommand.h"
#include "bundling/BundleProtocol.h"

namespace dtn {

BlockCommand::BlockCommand() 
    : TclCommand("block")
{
    bind_var(new oasys::BoolOpt("age_outbound_enabled",
                                &BundleProtocol::params_.age_outbound_enabled_,
                                "Is the Age Extension Block enabled for outbound bundles "
                                "(default is false)"));

    bind_var(new oasys::BoolOpt("age_inbound_processing",
                                &BundleProtocol::params_.age_inbound_processing_,
                                "Is the Age Extension Block supported on inbound bundles "
                                "(default is true)"));

    bind_var(new oasys::BoolOpt("age_zero_creation_ts_time",
                                &BundleProtocol::params_.age_zero_creation_ts_time_,
                                "Is the Creation Timestamp Time zeroed out "
                                "(default is true)"));
}
    
} // namespace dtn
