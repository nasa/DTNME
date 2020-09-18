/*
 *    Copyright 2005-2006 Intel Corporation
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

#ifdef HAVE_CONFIG_H
#  include <oasys-config.h>
#endif

#include <algorithm>
#include <cstdarg>

#include "InitSequencer.h"
#include "../util/Singleton.h"

namespace oasys {

//////////////////////////////////////////////////////////////////////////////
//
// InitSequencer
//
template<>
InitSequencer* Singleton<InitSequencer>::instance_ = NULL;

/// Sort in decreasing order
struct InitStepSort {
    bool operator()(InitStep* left, InitStep* right)
    {
        return left->time() > right->time();
    }
};


InitSequencer::InitSequencer()
    : Logger("InitSequencer", "/oasys/init_sequencer")
{
}

int 
InitSequencer::start(std::string step, Plan* plan)
{
    (void)plan;
    
    int err;
    
    add_extra_deps(); 
    mark_dep(step);
    err = topo_sort();
    if (err != 0) 
    {
        return err;
    }

    err = run_steps();
    if (err != 0)
    {
        return err;
    }

    return 0;
}

void 
InitSequencer::add_step(InitStep* step)
{
    ASSERT(step != 0);
    
    if (steps_.find(step->name()) != steps_.end()) 
    {
        log_warn("Step %s already known to sequencer, ignoring", 
                 step->name().c_str());
        return;
    }

    steps_[step->name()] = step;
}

InitStep*
InitSequencer::get_step(const std::string& name)
{
    ASSERT(steps_.find(name) != steps_.end());
    return steps_[name];
}

void
InitSequencer::reset()
{
    for (StepMap::iterator i = steps_.begin(); 
         i != steps_.end(); ++i)
    {
        i->second->done_ = false;
    }
}

void 
InitSequencer::print_dot()
{
    std::string dotfile;

    log_info("digraph G {");
    for (StepMap::const_iterator i = steps_.begin(); 
         i != steps_.end(); ++i)
    {
        InitStep* step = i->second;

        log_info("\t\"%s\";", step->name().c_str());

        for (InitStep::DepList::const_iterator i = step->dependencies().begin();
             i != step->dependencies().end(); ++i)
        {
            log_info("\t\"%s\" -> \"%s\";", i->c_str(), step->name().c_str());
        }
    }    
    log_info("}");
}

int
InitSequencer::run_steps()
{
    std::vector<InitStep*> step_list;

    for (StepMap::iterator i = steps_.begin(); 
         i != steps_.end(); ++i)
    {
        step_list.push_back(i->second);
    }
    std::sort(step_list.begin(), step_list.end(), InitStepSort());
    
    int err = 0;
    for (std::vector<InitStep*>::iterator i = step_list.begin();
         i != step_list.end(); ++i)
    {
        InitStep* step = *i;

        log_debug("step %d %s", step->time(), step->name().c_str());
        if (step->mark_ && !step->done())
        {
            log_debug("running %s", step->name().c_str());
            ASSERT(step->dep_are_satisfied());
            err = step->run();
            if (err != 0) 
            {
                log_warn("%s had an error, stopping...", step->name().c_str());
                break;
            }
        }
    }
    return err;
}

int 
InitSequencer::topo_sort()
{
    std::vector<InitStep*> step_stack;
    ReverseDepEdges edges;

    // make backwards edges
    for (StepMap::iterator i = steps_.begin(); i != steps_.end(); ++i)
    {
        InitStep* step = i->second;
        step->time_    = -1;

        for (ReverseDepList::const_iterator j = step->dependencies().begin();
             j != step->dependencies().end(); ++j)
        {
            log_debug("%s edge to %s", j->c_str(), step->name().c_str());
            edges[*j].push_back(step->name());
        }

        // parentless, so can be started at any time        
        if (step->dependencies().size() == 0)
        {
            step_stack.push_back(step);
        }
    }
    
    // Perform the DFS from each dependency-less node
    dfs_time_ = 0;
    while (step_stack.size() > 0)
    {
        InitStep* step = step_stack.back();
        step_stack.pop_back();        
        dfs(step, edges);
    }

#ifndef NDEBUG
#    ifdef OASYS_LOG_DEBUG_ENABLED
    for (StepMap::iterator i = steps_.begin(); 
         i != steps_.end(); ++i)
    {
        InitStep* step = i->second;
        log_debug("step %s has time %d", step->name().c_str(), step->time_);
    }
#    endif // OASYS_LOG_DEBUG_ENABLED
#endif // NDEBUG

    return 0;
}
    
void 
InitSequencer::dfs(InitStep* step, ReverseDepEdges& edges)
{
    for (ReverseDepList::const_iterator i = edges[step->name()].begin();
         i != edges[step->name()].end(); ++i)
    {
        if (steps_[*i]->time_ == -1)
        {
            dfs(steps_[*i], edges);
        }
    }
    
    step->time_ = dfs_time_;
    ++dfs_time_;
}

void 
InitSequencer::mark_dep(const std::string& target)
{
    std::vector<InitStep*> step_stack;
    
    log_debug("target is %s", target.c_str());
    for (StepMap::iterator i = steps_.begin(); 
         i != steps_.end(); ++i)
    {
        i->second->mark_ = false;
    }

   
    ASSERT(steps_.find(target) != steps_.end());
   
    step_stack.push_back(steps_[target]);

    while (step_stack.size() > 0) 
    {
        InitStep* step = step_stack.back();
        step_stack.pop_back();

        if (!step->mark_) 
        {
            step->mark_ = true;
            log_debug("%s is a dependent step", step->name().c_str());
        }        

        for (InitStep::DepList::const_iterator i = step->dependencies().begin();
             i != step->dependencies().end(); ++i)
        {
            if (steps_.find(*i) == steps_.end())
            {
                PANIC("%s is dependent on %s which is bogus", 
                      step->name().c_str(), i->c_str());
            }
            
            if(!steps_[*i]->mark_)
            {
                step_stack.push_back(steps_[*i]);
            }
        }
    }
}

void
InitSequencer::add_extra_dep(InitExtraDependency* extra_dep)
{
    extra_dependencies_.push_back(extra_dep);
}

void
InitSequencer::add_extra_deps()
{
    for (std::vector<InitExtraDependency*>::iterator i = extra_dependencies_.begin();
         i != extra_dependencies_.end(); ++i)
    {
        // Check that these modules are legit
        ASSERT(steps_.find((*i)->new_dep_)  != steps_.end());
        ASSERT(steps_.find((*i)->depender_) != steps_.end());        

        log_debug("extra dependency of %s to %s", 
                  (*i)->depender_.c_str(), (*i)->new_dep_.c_str());

        steps_[(*i)->depender_]->add_dep((*i)->new_dep_);
    }
    
    // clear these after the first add
    extra_dependencies_.clear();
}

//////////////////////////////////////////////////////////////////////////////
//
// InitStep
//
InitStep::InitStep(const std::string& the_namespace, 
                   const std::string& name)
    : done_(false),
      name_(the_namespace + "::" + name),
      mark_(false),
      time_(-1)
{
    Singleton<InitSequencer>::instance()->add_step(this);    
}

InitStep::InitStep(const std::string& the_namespace, 
                   const std::string& name, int depsize, ...)
    : done_(false),
      name_(the_namespace + "::" + name),
      mark_(false),
      time_(-1)
{
    va_list ap;
    va_start(ap, depsize);
    for (int i=0; i<depsize; ++i)
    {
        dependencies_.push_back(va_arg(ap, const char*));
    }
    va_end(ap);
    
    Singleton<InitSequencer>::instance()->add_step(this);
}

InitStep::InitStep(const std::string& the_namespace, 
                   const std::string& name, const DepList& deps)
    : done_(false),
      name_(the_namespace + "::" + name),
      dependencies_(deps),
      mark_(false),
      time_(-1)
{
    Singleton<InitSequencer>::instance()->add_step(this);
}

int
InitStep::run()
{ 
    int err = run_component(); 
    if (!err) 
    {
        done_ = true; 
    }

    return err;
}

bool
InitStep::dep_are_satisfied()
{
    bool sat = true;
    for (DepList::const_iterator i = dependencies_.begin();
         i != dependencies_.end(); ++i)
    {
        sat &= Singleton<InitSequencer>::instance()->get_step(*i)->done();
    }

    return sat;
}

} // namespace oasys
