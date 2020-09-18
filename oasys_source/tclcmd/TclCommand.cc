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

#ifdef HAVE_CONFIG_H
#  include <oasys-config.h>
#endif

#include <climits>
#include "TclCommand.h"
#include "DebugCommand.h"
#include "GettimeofdayCommand.h"
#include "HelpCommand.h"
#include "LogCommand.h"

#include "debug/DebugUtils.h"
#include "io/NetUtils.h"
#include "thread/SpinLock.h"
#include "util/StringBuffer.h"
#include "util/InitSequencer.h"

extern "C" int Tclreadline_Init(Tcl_Interp* interp);

namespace oasys {

/******************************************************************************
 *
 * TclCommandInterp
 *
 *****************************************************************************/
// static variables
TclCommandInterp* TclCommandInterp::instance_;
TclCommandList*   TclCommandInterp::auto_reg_ = NULL;

#include "command-init-tcl.c"

//----------------------------------------------------------------------
TclCommandInterp::TclCommandInterp(const char* logpath)
    : Logger("TclCommandInterp", "%s", logpath)
{}

//----------------------------------------------------------------------
int
TclCommandInterp::do_init(const char* argv0, bool no_default_cmds)
{
    interp_ = Tcl_CreateInterp();
    lock_   = new SpinLock();
    Tcl_Preserve(interp_);

    // for some reason, this needs to be called to set up the various
    // locale strings and get things like the "ascii" encoding defined
    // for a file channel
    Tcl_FindExecutable(argv0);
    
    // run Tcl_Init to set up the local tcl package path, but don't
    // depend on it succeeding in case there's a strange tcl
    // installation
    if (Tcl_Init(interp_) != TCL_OK) {
        StringBuffer err("initialization problem calling Tcl_Init: %s\n"
                         "(this is not a fatal error, continuing initialization...)\n\n",
                         Tcl_GetStringResult(interp_));
        log_multiline(LOG_WARN, err.c_str());
    }

    // do auto registration of commands (if any)
    if (auto_reg_) {
        ASSERT(auto_reg_); 
        while (!auto_reg_->empty()) {
            TclCommand* m = auto_reg_->front();
            auto_reg_->pop_front();
            reg(m);
        }
    
        delete auto_reg_;
        auto_reg_ = NULL;
    }

    // register the default commands
    if (! no_default_cmds) {
        reg(new DebugCommand());
        reg(new GettimeofdayCommand());
        reg(new HelpCommand());
        reg(new LogCommand());
    }
    
    // evaluate the boot-time tcl commands (copied since tcl may
    // overwrite the string value)
    char* cmd = strdup(INIT_COMMAND);
    if (Tcl_Eval(interp_, cmd) != TCL_OK) {
        log_err("error in init commands: \"%s\"", Tcl_GetStringResult(interp_));
        return TCL_ERROR;
    }
    free(cmd);

    return TCL_OK;
}

//----------------------------------------------------------------------
TclCommandInterp::~TclCommandInterp()
{
    log_notice("shutting down interpreter");
    TclCommandList::iterator iter;
    for (iter = commands_.begin();
         iter != commands_.end();
         ++iter)
    {
        log_debug("deleting %s command", (*iter)->name_.c_str());
        delete *iter;
    }

    log_debug("all commands deleted");

    commands_.clear();

    Tcl_DeleteInterp(interp_);
    Tcl_Release(interp_);

    delete lock_;
}

//----------------------------------------------------------------------
void
TclCommandInterp::shutdown()
{
    delete instance_;
    instance_ = NULL;
}

//----------------------------------------------------------------------
int
TclCommandInterp::init(const char* argv0,
                       const char* logpath,
                       bool no_default_cmds)
{
    ASSERT(instance_ == NULL);
    instance_ = new TclCommandInterp(logpath);
    
    return instance_->do_init(argv0, no_default_cmds);
}

//----------------------------------------------------------------------
int
TclCommandInterp::exec_file(const char* file)
{
    int err;
    ScopeLock l(lock_, "TclCommandInterp::exec_file");

    log_debug("executing command file %s", file);
    
    err = Tcl_EvalFile(interp_, (char*)file);
    
    if (err != TCL_OK) {
        logf(LOG_ERR, "error: line %d: '%s':\n%s",
             Tcl_GetErrorLine(interp_), Tcl_GetStringResult(interp_),
             Tcl_GetVar(interp_, "errorInfo", TCL_GLOBAL_ONLY));
    }
    
    return err;    
}

//----------------------------------------------------------------------
int
TclCommandInterp::exec_command(const char* command)
{
    int err;
    ScopeLock l(lock_, "TclCommandInterp::exec_command");

    // ignore empty command lines
    if (command[0] == '\0')
        return TCL_OK;

    // tcl modifies the command string while executing it, so we need
    // to make a copy
    char* buf = strdup(command);

    log_debug("executing command '%s'", buf);
    
    err = Tcl_Eval(interp_, buf);
    
    free(buf);
    
    if (err != TCL_OK) {
        logf(LOG_ERR, "error: line %d: '%s':\n%s",
             Tcl_GetErrorLine(interp_), Tcl_GetStringResult(interp_),
             Tcl_GetVar(interp_, "errorInfo", TCL_GLOBAL_ONLY));
    }
    
    return err;
}

//----------------------------------------------------------------------
int
TclCommandInterp::exec_command(int objc, Tcl_Obj** objv)
{
    int err;
    ScopeLock l(lock_, "TclCommandInterp::exec_command");

    err = Tcl_EvalObjv(interp_, objc, objv, TCL_EVAL_GLOBAL);
    
    if (err != TCL_OK) {
        logf(LOG_ERR, "error: line %d: '%s':\n%s",
             Tcl_GetErrorLine(interp_), Tcl_GetStringResult(interp_),
             Tcl_GetVar(interp_, "errorInfo", TCL_GLOBAL_ONLY));
    }
    
    return err;
}

//----------------------------------------------------------------------
void
TclCommandInterp::set_command_logpath()
{
    StringBuffer cmd("set command_logpath %s", logpath());
    if (Tcl_Eval(interp_, const_cast<char*>(cmd.c_str())) != TCL_OK) {
        log_err("tcl error setting command_logpath: \"%s\"",
                Tcl_GetStringResult(interp_));
    }
}

//----------------------------------------------------------------------
void
TclCommandInterp::command_server(const char* prompt,
                                 in_addr_t addr, u_int16_t port)
{
    set_command_logpath();
    log_debug("starting command server on %s:%d", intoa(addr), port);
    StringBuffer cmd("command_server \"%s\" %s %d", prompt, intoa(addr), port);
    
    if (Tcl_Eval(interp_, const_cast<char*>(cmd.c_str())) != TCL_OK) {
        log_err("tcl error starting command_server: \"%s\"",
                Tcl_GetStringResult(interp_));
    }
}

//----------------------------------------------------------------------
void
TclCommandInterp::command_loop(const char* prompt)
{
    set_command_logpath();
    StringBuffer cmd("command_loop \"%s\"", prompt);

#if TCLREADLINE_ENABLED
    Tclreadline_Init(interp_);
#endif
    
    if (Tcl_Eval(interp_, const_cast<char*>(cmd.c_str())) != TCL_OK) {
        log_err("tcl error in command_loop: \"%s\"", Tcl_GetStringResult(interp_));
    }
}

//----------------------------------------------------------------------
void
TclCommandInterp::event_loop()
{
    set_command_logpath();
    if (Tcl_Eval(interp_, "event_loop") != TCL_OK) {
        log_err("tcl error in event_loop: \"%s\"", Tcl_GetStringResult(interp_));
    }
}

//----------------------------------------------------------------------
void
TclCommandInterp::exit_event_loop()
{
    if (Tcl_Eval(interp_, "exit_event_loop") != TCL_OK) {
        log_err("tcl error in event_loop: \"%s\"", Tcl_GetStringResult(interp_));
    }
}

//----------------------------------------------------------------------
void
TclCommandInterp::reg(TclCommand *command)
{
    ScopeLock l(lock_, "TclCommandInterp::reg");

    // use our logpath as the base for the command's log path
    command->logpathf("%s/%s", logpath(), command->name());
    
    command->logf(LOG_DEBUG, "%s command registering", command->name());

    Tcl_CmdInfo info;
    if (Tcl_GetCommandInfo(interp_, (char*)command->name(), &info) != 0) {
        log_warn("re-registering command %s over existing command",
                 command->name());
    }
                 
    Tcl_CreateObjCommand(interp_, 
                         const_cast<char*>(command->name()),
                         TclCommandInterp::tcl_cmd,
                         (ClientData)command,
                         NULL);
    
    commands_.push_front(command);
}

//----------------------------------------------------------------------
bool
TclCommandInterp::lookup(const char* command, TclCommand** commandp)
{
    Tcl_CmdInfo info;

    if (Tcl_GetCommandInfo(interp_, (char*)command, &info) == 0) {
        log_debug("lookup tcl command %s: does not exist", command);
        return false;
    }

    if (info.objProc == TclCommandInterp::tcl_cmd) {
        log_debug("lookup tcl command %s: exists and is TclCommand %p",
                  command, info.clientData);
        
        if (commandp)
            *commandp = (TclCommand*)info.objClientData;
        
    } else {
        log_debug("lookup tcl command %s: exists but is not a TclCommand",
                  command);
    }

    return true;
}

//----------------------------------------------------------------------
void
TclCommandInterp::auto_reg(TclCommand *command)
{
    // this should only be called from the static initializers, i.e.
    // we haven't been initialized yet
    ASSERT(instance_ == NULL);

    // we need to explicitly create the auto_reg list the first time
    // since there's no guarantee of ordering of static constructors
    if (!auto_reg_)
        auto_reg_ = new TclCommandList();
    
    auto_reg_->push_back(command);
}

//----------------------------------------------------------------------
void
TclCommandInterp::reg_atexit(void(*fn)(void*), void* data)
{
    ScopeLock l(lock_, "TclCommandInterp::reg_atexit");
    Tcl_CreateExitHandler(fn, data);
}

//----------------------------------------------------------------------
Tcl_Channel
TclCommandInterp::register_file_channel(ClientData fd, int readOrWrite)
{
    Tcl_Channel channel = Tcl_MakeFileChannel(fd, readOrWrite);
    
    if (channel == NULL) {
        log_err("can't create tcl file channel: %s",
                strerror(Tcl_GetErrno()));
        return NULL;
    }

    Tcl_RegisterChannel(interp_, channel);
    return channel;
}
    
//----------------------------------------------------------------------
int 
TclCommandInterp::tcl_cmd(ClientData client_data, Tcl_Interp* interp,
                          int objc, Tcl_Obj* const* objv)
{
    TclCommand* command = (TclCommand*)client_data;

    // first check for builtin commands
    if (command->do_builtins_) 
    {
        if (objc >= 2) {
            const char* cmd = Tcl_GetStringFromObj(objv[1], NULL);
            if (strcmp(cmd, "cmd_info") == 0) {
                return command->cmd_info(interp);
            }

            if (strcmp(cmd, "set") == 0) {
                return command->cmd_set(objc, (Tcl_Obj**)objv, interp);
            }
        }
    }

    return command->exec(objc, (Tcl_Obj**)objv, interp);
}

//----------------------------------------------------------------------
void
TclCommandInterp::set_result(const char* result)
{
    Tcl_SetResult(interp_, (char*)result, TCL_VOLATILE);
}

//----------------------------------------------------------------------
void
TclCommandInterp::set_objresult(Tcl_Obj* obj)
{
    Tcl_SetObjResult(interp_, obj);
}

//----------------------------------------------------------------------
void
TclCommandInterp::append_result(const char* result)
{
    Tcl_AppendResult(interp_, (char*)result, NULL);
}

//----------------------------------------------------------------------
void
TclCommandInterp::resultf(const char* fmt, ...)
{
    StringBuffer buf;
    STRINGBUFFER_VAPPENDF(buf, fmt);
    set_result(buf.c_str());
}

//----------------------------------------------------------------------
void
TclCommandInterp::append_resultf(const char* fmt, ...)
{
    StringBuffer buf;
    STRINGBUFFER_VAPPENDF(buf, fmt);
    append_result(buf.c_str());
}

//----------------------------------------------------------------------
void
TclCommandInterp::wrong_num_args(int argc, const char** argv, int parsed,
                                 int min, int max)
{
    set_result("wrong number of arguments to '");
    append_result(argv[0]);
    
    for (int i = 1; i < parsed; ++i) {
        append_result(" ");
        append_result(argv[i]);
    }
    append_result("'");

    if (max == min) {
        append_resultf(" expected %d, got %d", min, argc);
    } else if (max == INT_MAX) {
        append_resultf(" expected at least %d, got %d", min, argc);
    } else {
        append_resultf(" expected %d - %d, got %d", min, max, argc);
    }
}

//----------------------------------------------------------------------
void
TclCommandInterp::wrong_num_args(int objc, Tcl_Obj** objv, int parsed,
                                 int min, int max)
{
    char* argv[objc];
    for (int i = 0; i < objc; ++i) {
        argv[i] = Tcl_GetStringFromObj(objv[i], NULL);
    }
    wrong_num_args(objc, (const char**)argv, parsed, min, max);
}

//----------------------------------------------------------------------
const char*
TclCommandInterp::get_result()
{
    return Tcl_GetStringResult(interp_);
}

/******************************************************************************
 *
 * TclCommand
 *
 *****************************************************************************/
TclCommand::TclCommand(const char* name, const char* theNamespace)
    : Logger("TclCommand", "/command/%s", name),
      do_builtins_(true)
{

    if (theNamespace != 0) {
        name_ += theNamespace;
        name_ += "::";
    }

    name_ += name;
}

//----------------------------------------------------------------------
TclCommand::~TclCommand()
{
    BindingTable::iterator iter;
    for (iter = bindings_.begin(); iter != bindings_.end(); ++iter) {
        delete iter->second;
    }
    bindings_.clear();
}

//----------------------------------------------------------------------
int
TclCommand::exec(int objc, Tcl_Obj** objv, Tcl_Interp* interp)
{
    // If the default implementation is called, then convert all
    // arguments to strings and then call the other exec variant.
    char* argv[objc];

    for (int i = 0; i < objc; ++i) {
        argv[i] = Tcl_GetStringFromObj(objv[i], NULL);
    }

    return exec(objc, (const char**) argv, interp);
}

//----------------------------------------------------------------------
int
TclCommand::exec(int argc, const char** argv, Tcl_Interp* interp)
{
    (void)argc;
    (void)interp;
    
    resultf("command %s unknown argument", argv[0]);
    return TCL_ERROR;
}

//----------------------------------------------------------------------
void
TclCommand::resultf(const char* fmt, ...)
{
    StringBuffer buf;
    STRINGBUFFER_VAPPENDF(buf, fmt);
    TclCommandInterp::instance()->set_result(buf.c_str());
}

//----------------------------------------------------------------------
void
TclCommand::append_resultf(const char* fmt, ...)
{
    StringBuffer buf;
    STRINGBUFFER_VAPPENDF(buf, fmt);
    TclCommandInterp::instance()->append_result(buf.c_str());
}

//----------------------------------------------------------------------
int
TclCommand::cmd_info(Tcl_Interp* interp)
{
    (void)interp;
    
    StringBuffer buf;

    for (BindingTable::iterator itr = bindings_.begin();
         itr != bindings_.end(); ++itr)
    {
        buf.appendf("%s ", (*itr).first.c_str());
    }
    
    set_result(buf.c_str());
    return TCL_OK;
}

//----------------------------------------------------------------------
int
TclCommand::cmd_set(int objc, Tcl_Obj** objv, Tcl_Interp* interp)
{
    (void)interp;
    ASSERT(objc >= 2);
    
    // handle "set binding [value]" command
    if (objc < 3 || objc > 4) {
        resultf("wrong number of args: expected 3-4, got %d", objc);
        return TCL_ERROR;
    }

    const char* var = Tcl_GetStringFromObj(objv[2], NULL);
    int val_len = 0;
    const char* val = NULL;
    if (objc == 4) {
        val = Tcl_GetStringFromObj(objv[3], &val_len);
    }
    
    BindingTable::iterator itr;
    itr = bindings_.find(var);
    
    if (itr == bindings_.end()) {
        resultf("set: binding for %s does not exist", var);
        return TCL_ERROR;
    }
    Opt* opt = (*itr).second;

    // set value (if any)
    if (val) {
        if (opt->set(val, val_len) != 0) {
            resultf("%s set %s: invalid value '%s'",
                    Tcl_GetStringFromObj(objv[0], 0), var, val);
            return TCL_ERROR;
        }

        if (validate(var, val, opt) != 0) {
            return TCL_ERROR;
        }
    }

    StaticStringBuffer<256> buf;
    opt->get(&buf);
    set_result(buf.c_str());

    return TCL_OK;
}

//----------------------------------------------------------------------
int
TclCommand::validate(const char* var, const char* val, Opt* opt)
{
    (void)var;
    (void)val;
    (void)opt;
    return 0;
}

//----------------------------------------------------------------------
void
TclCommand::bind_var(Opt* opt)
{
    const char* name = opt->longopt_;
    if (bindings_.find(name) != bindings_.end()) {
        if (Log::initialized()) {
            log_warn("warning, binding for %s already exists", name);
        }
    }

    bindings_[name] = opt;

    // we're now strict about requiring help strings
    ASSERT(opt->desc_ != NULL && opt->desc_[0] != '\0');
    
    StaticStringBuffer<256> subcmd("set %s", name);
    if (opt->valdesc_[0]) {
        subcmd.appendf(" <%s>", opt->valdesc_);
    }
    add_to_help(subcmd.c_str(), opt->desc_);
}

//----------------------------------------------------------------------
void
TclCommand::unbind(const char* name)
{
    BindingTable::iterator iter = bindings_.find(name);

    if (iter == bindings_.end()) {
        if (Log::initialized()) {
            log_warn("warning, binding for %s doesn't exist", name);
        }
        return;
    }

    if (Log::initialized()) {
        log_debug("removing binding for %s", name);
    }

    Opt* old = iter->second;
    bindings_.erase(iter);

    delete old;
}

} // namespace oasys
