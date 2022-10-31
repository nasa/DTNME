/*
 *    Copyright 2015 United States Government as represented by NASA
 *       Marshall Space Flight Center. All Rights Reserved.
 *
 *    Released under the NASA Open Source Software Agreement version 1.3;
 *    You may obtain a copy of the Agreement at:
 * 
 *        http://ti.arc.nasa.gov/opensource/nosa/
 * 
 *    The subject software is provided "AS IS" WITHOUT ANY WARRANTY of any kind,
 *    either expressed, implied or statutory and this agreement does not,
 *    in any manner, constitute an endorsement by government agency of any
 *    results, designs or products resulting from use of the subject software.
 *    See the Agreement for the specific language governing permissions and
 *    limitations.
 */

#ifndef _BUNDLE_RESTAGING_COMMAND_H_
#define _BUNDLE_RESTAGING_COMMAND_H_

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#ifdef BARD_ENABLED

#include <third_party/oasys/tclcmd/TclCommand.h>

#include "bundling/BARDRestageCLIF.h"


namespace dtn {

/**
 * CommandModule for the "bard" command to configure/control
 * the BundleArchitecturalRestagingDaemon
 */
class BundleRestagingCommand : public oasys::TclCommand {
public:
    BundleRestagingCommand();
    
    /**
     * Virtual from CommandModule.
     */
    virtual int exec(int argc, const char** argv, Tcl_Interp* interp);

protected:
    /// routines to process individual commands
    virtual int process_quotas(int argc, const char** argv);
    virtual int process_usage(int argc, const char** argv);
    virtual int process_dump(int argc, const char** argv);
    virtual int process_add_quota(int argc, const char** argv);
    virtual int process_delete_quota(int argc, const char** argv);
    virtual int process_unlimited_quota(int argc, const char** argv);
    virtual int process_force_restage(int argc, const char** argv);
    virtual int process_rescan(int argc, const char** argv);
    virtual int process_del_restaged_bundles(int argc, const char** argv);
    virtual int process_del_all_restaged_bundles(int argc, const char** argv);
    virtual int process_reload(int argc, const char** argv);
    virtual int process_reload_all(int argc, const char** argv);

    /// utility functions
    virtual bool parse_bool(const char* val_str, bool& retval);
    virtual bool parse_number_value(const char* val_str, size_t& retval);
    virtual bool parse_quota_type(const char* val_str, bard_quota_type_t& retval);
    virtual bool parse_naming_scheme(const char* val_str, bard_quota_naming_schemes_t& retval);
    virtual bool parse_node_number(const char* val_str, size_t& retval);
};

} // namespace dtn

#endif  // BARD_ENABLED

#endif /* _BUNDLE_RESTAGING_COMMAND_H_ */
