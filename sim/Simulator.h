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

#include <queue>
#include <oasys/debug/DebugUtils.h>
#include <oasys/debug/Log.h>
#include <oasys/util/Singleton.h>

#include "SimEvent.h"
#include "SimEventHandler.h"

namespace dtnsim {

/**
 * The main simulator class. This defines the main event loop
 */
class Simulator : public oasys::Singleton<Simulator, false>,
                  public SimEventHandler,
                  public oasys::Logger
{
public:
    /**
     * Return the current simulator time.
     */
    static double time() { return time_; }

    /**
     * Constructor.
     */
    Simulator();
        
    /**
     * Destructor.
     */
    virtual ~Simulator() {}

    /**
     * Add an event to the main event queue.
     */
    static void post(SimEvent *e);
    
    /**
     * Stops simulation and exits the loop
     */
    void exit();

    /**
     * Main run loop.
     */
    void run();

    /**
     * Handle all bundle events at nodes returning the amount of time
     * (in ms) until the next timer is due.
     */
    int run_node_events();

    /**
     * Pause execution of the simulator, running a console loop until
     * it exits.
     */
    void pause();

    /**
     * Run the command loop.
     */
    void run_console(bool complete);

    /**
     * Register a command to run at exit.
     */
    void set_exit_event(SimAtEvent* event);

    static double runtill_;             ///< time to end the simulation
    
private:
    /**
     * Virtual from SimEventHandler
     */
    void process(SimEvent* e);

    void log_inqueue_stats();

    static double time_;                ///< current time (static to avoid object)

    std::priority_queue<SimEvent*,
                        std::vector<SimEvent*>,
                        SimEventCompare> eventq_;

    SimAtEvent* exit_event_;

    void run_at_event(SimAtEvent* evt);

    /*
     * Handlers for SIGINT to pause the simulation while it's running.
     */
    static void handle_interrupt(int sig);
    void check_interrupt();
    static bool interrupted_;
};

} // namespace dtnsim
    
