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

/*
 *    Modifications made to this file by the patch file dtnme_mfs-33289-1.patch
 *    are Copyright 2015 United States Government as represented by NASA
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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <oasys/tclcmd/TclCommand.h>

#include "Simulator.h"
#include "Node.h"
#include "Topology.h"
#include "SimLog.h"
#include "bundling/BundleTimestamp.h"


namespace oasys {
    template<> dtnsim::Simulator* oasys::Singleton<dtnsim::Simulator, false>::instance_ = NULL;
}

using namespace dtn;

namespace dtnsim {

//----------------------------------------------------------------------

double Simulator::time_ = 0;
bool   Simulator::interrupted_ = false;
double Simulator::runtill_ = -1;

//----------------------------------------------------------------------
Simulator::Simulator()
    : Logger("Simulator", "/dtnsim"),
      eventq_(),
      exit_event_(NULL)
{
}

//----------------------------------------------------------------------
void
Simulator::post(SimEvent* e)
{	
    instance_->eventq_.push(e);
}

//----------------------------------------------------------------------
void
Simulator::exit() 
{
    ::exit(0);
}

//----------------------------------------------------------------------
int
Simulator::run_node_events()
{
    bool done;
    int next_timer;
    do {
        done = true;
        next_timer = -1;
        
        Topology::NodeTable::iterator iter;
        for (iter =  Topology::node_table()->begin();
             iter != Topology::node_table()->end();
             ++iter)
        {
            check_interrupt();
        
            Node* node = iter->second;
            node->set_active();
        
            int next = oasys::TimerSystem::instance()->run_expired_timers();
            if (next != -1) {
                if (next_timer == -1) {
                    next_timer = next;
                } else {
                    next_timer = std::min(next_timer, next);
                }
            }
        
            log_debug("processing all bundle events for node %s", node->name());
            if (node->process_one_bundle_event()) {
                done = false;
                while (node->process_one_bundle_event()) {
                    check_interrupt();
                }
            }
        }
    } while (!done);

    return next_timer;
}

//----------------------------------------------------------------------
void
Simulator::log_inqueue_stats()
{
    Topology::NodeTable::iterator node_iter;
    for (node_iter =  Topology::node_table()->begin();
         node_iter != Topology::node_table()->end();
         ++node_iter)
    {
        Node* node = node_iter->second;

        oasys::ScopeLock l(node->pending_bundles()->lock(), "log_inqueue_stats");
        pending_bundles_t::iterator bundle_iter;
        for (bundle_iter = node->pending_bundles()->begin();
             bundle_iter != node->pending_bundles()->end();
             ++bundle_iter)
        {
            #ifdef PENDING_BUNDLES_IS_MAP
                Bundle* bundle = bundle_iter->second; // for <map> lists
            #else
                Bundle* bundle = *bundle_iter; // for <list> lists
            #endif
            SimLog::instance()->log_inqueue(node, bundle);
        }
    }
}

//----------------------------------------------------------------------
void
Simulator::run()
{
    oasys::Log* log = oasys::Log::instance();
    log->set_prefix("--");

    log_debug("Configuring all nodes");
    Topology::NodeTable::iterator iter;
    for (iter = Topology::node_table()->begin();
         iter != Topology::node_table()->end();
         ++iter)
    {
        iter->second->configure();
    }

    log_debug("Setting up interrupt handler");
    signal(SIGINT, handle_interrupt);
    
    log_debug("Starting Simulator event loop...");

    while (1) {
        check_interrupt();

        int next_timer_ms = run_node_events();
        double next_timer = (next_timer_ms == -1) ? INT_MAX :
                            time_ + (((double)next_timer_ms) / 1000);
        double next_event = INT_MAX;
        log->set_prefix("--");
        
        SimEvent* e = NULL;
        if (! eventq_.empty()) {
            e = eventq_.top();
            next_event = e->time();
        }
        
        if ((next_timer_ms == -1) && (e == NULL)) {
            break;
        }
        else if (next_timer < next_event) {
            time_ = next_timer;
            log_debug("advancing time by %u ms to %f for next timer",
                      next_timer_ms, time_);
        }
        else {
            ASSERT(e != NULL);
            eventq_.pop();
            time_ = e->time();

            if (e->is_valid()) {
                ASSERT(e->handler() != NULL);
                /* Process the event */
                log_debug("Event:%p type %s at time %f",
                          e, e->type_str(), time_);
                e->handler()->process(e);
            }
        }
        
        if ((Simulator::runtill_ != -1) &&
            (time_ > Simulator::runtill_)) {
            log_info("Exiting simulation. "
                     "Current time (%f) > Max time (%f)",
                     time_, Simulator::runtill_);
            goto done;
        }
    }

    log_info("Simulator loop done -- no pending events or timers (time is %f)",
             time_);

    if (exit_event_) {
        run_at_event(exit_event_);
    }
    
done:
    log_inqueue_stats();
    SimLog::instance()->flush();
}

//----------------------------------------------------------------------
void
Simulator::pause()
{
    oasys::StaticStringBuffer<128> cmd;
    cmd.appendf("puts \"Simulator paused at time %f...\"", time_);
    oasys::TclCommandInterp::instance()->exec_command(cmd.c_str());

    run_console(false);
}

//----------------------------------------------------------------------
void
Simulator::run_console(bool complete)
{
    Node* cur_active = Node::active_node();
    interrupted_ = true;
    
    if (complete) {
        oasys::TclCommandInterp::instance()->command_loop("dtnsim% ");
    } else {
        // we can't re-enter tclreadline more than once so if it's not
        // complete we need to use the simple command loop
        oasys::TclCommandInterp::instance()->exec_command(
            "simple_command_loop \"dtnsim% \"");
    }
    
    interrupted_ = false;
    cur_active->set_active();
}

//----------------------------------------------------------------------
void
Simulator::handle_interrupt(int sig)
{
    (void)sig;
    
    if (interrupted_) {
        instance()->exit();
    } else {
        interrupted_ = true;
    }
}

//----------------------------------------------------------------------
void
Simulator::check_interrupt()
{
    if (interrupted_) {
        oasys::StaticStringBuffer<128> cmd;
        cmd.appendf("puts \"Simulator interrupted at time %f...\"", time_);
        oasys::TclCommandInterp::instance()->exec_command(cmd.c_str());
        run_console(false);
    }
}

//----------------------------------------------------------------------
/**
 * Override gettimeofday to return the simulator time.
 */
extern "C" int
gettimeofday(struct timeval *tv, struct timezone *tz)
{
    (void)tz;
    double now = Simulator::time();
    DOUBLE_TO_TIMEVAL(now, *tv);

    // Sometimes converting a double like 100.2 into an timeval
    // results in a value like of 100.199999 so we fudge it a bit here
    // to make the output look prettier
    if ((tv->tv_usec % 1000) == 999) {
        tv->tv_usec += 1;
    }
    return 0;
}

//----------------------------------------------------------------------
void
Simulator::process(SimEvent *e)
{
    switch (e->type()) {
    case SIM_AT_EVENT: {
        run_at_event((SimAtEvent*)e);
        break;
    }    
    default:
        NOTREACHED;
    }
}

//----------------------------------------------------------------------
void
Simulator::set_exit_event(SimAtEvent* evt)
{
    ASSERTF(exit_event_ == NULL, "cannot set multiple exit events");
    exit_event_ = evt;
}

//----------------------------------------------------------------------
void
Simulator::run_at_event(SimAtEvent* evt)
{
    int err = oasys::TclCommandInterp::instance()->
              exec_command(evt->objc_, evt->objv_);
    if (err != 0) {
        oasys::StringBuffer cmd;
        cmd.appendf("puts \"ERROR in at command, pausing simulation\"");
        oasys::TclCommandInterp::instance()->exec_command(cmd.c_str());
        pause();
    }
}

} // namespace dtnsim
