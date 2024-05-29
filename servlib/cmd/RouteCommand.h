/*
 *    Copyright 2004-2006 Intel Corporation
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

#ifndef _ROUTE_COMMAND_H_
#define _ROUTE_COMMAND_H_

#include <third_party/oasys/tclcmd/TclCommand.h>

namespace dtn {

/**
 * The "route" command.
 */
class RouteCommand : public oasys::TclCommand {
public:
    RouteCommand();
    
    /**
     * Virtual from CommandModule.
     */
    virtual int exec(int argc, const char** argv, Tcl_Interp* interp);

protected:
    /// routines to process individual commands
    virtual int process_add(int argc, const char** argv);
    virtual int process_add_ipn_range(int argc, const char** argv);
    virtual int process_del(int argc, const char** argv);
    virtual int process_local_eid(int argc, const char** argv);
    virtual int process_local_eid_ipn(int argc, const char** argv);
    virtual int process_extrtr_interface(int argc, const char** argv);
    virtual int process_imc_router_enabled(int argc, const char** argv);
    virtual int process_is_imc_router_node(int argc, const char** argv);
    virtual int process_imc_home_region(int argc, const char** argv);
    virtual int process_clear_imc_region_db(int argc, const char** argv);
    virtual int process_clear_imc_group_db(int argc, const char** argv);
    virtual int process_clear_imc_manual_join_db(int argc, const char** argv);
    virtual int process_imc_region_add(int argc, const char** argv);
    virtual int process_imc_region_del(int argc, const char** argv);
    virtual int process_imc_group_add(int argc, const char** argv);
    virtual int process_imc_group_del(int argc, const char** argv);
    virtual int process_imc_manual_join_add(int argc, const char** argv);
    virtual int process_imc_manual_join_del(int argc, const char** argv);
    virtual int process_imc_region_dump(int argc, const char** argv);
    virtual int process_imc_group_dump(int argc, const char** argv);
    virtual int process_imc_manual_join_dump(int argc, const char** argv);
    virtual int process_imc_issue_bp6_joins(int argc, const char** argv);
    virtual int process_imc_issue_bp7_joins(int argc, const char** argv);
    virtual int process_imc_sync_on_startup(int argc, const char** argv);
    virtual int process_imc_sync(int argc, const char** argv);
    virtual int process_imc_delete_unrouteable(int argc, const char** argv);
    virtual int process_imc_unreachable_is_unrouteable(int argc, const char** argv);
};

} // namespace dtn

#endif /* _ROUTE_COMMAND_H_ */
