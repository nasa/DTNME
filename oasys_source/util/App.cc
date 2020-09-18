/*
 *    Copyright 2007 Intel Corporation
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
 *    Modifications made to this file by the patch file oasys_mfs-33289-1.patch
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

#include <oasys-config.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/resource.h>

#include "App.h"
#include "Random.h"
#include "../debug/FatalSignals.h"

namespace oasys {

//----------------------------------------------------------------------
App::App(const char* classname,
         const char* name,
         const char* version)
    : Logger(classname, "%s", name),
      name_(name),
      version_(version),
      extra_usage_(""),
      random_seed_(0),
      random_seed_set_(false),
      niceness_(0),
      niceness_set_(false),
      print_version_(false),
      loglevelstr_(""),
      loglevel_(LOG_DEFAULT_THRESHOLD),
      logfile_("-"),
      debugpath_(LOG_DEFAULT_DBGFILE),
      daemonize_(false),
      conf_file_(""),
      conf_file_set_(false),
      ignore_sigpipe_(true)
{
}

//----------------------------------------------------------------------
void
App::init_app(int argc, char* const argv[])
{
#ifdef OASYS_DEBUG_MEMORY_ENABLED
    DbgMemInfo::init();
#endif
    
    fill_options();
    
    int remainder = opts_.getopt(argv[0], argc, argv, extra_usage_.c_str());
    
    if (print_version_) 
    {
        printf("%s version %s\n", name_.c_str(), version_.c_str());
        exit(0);
    }
    
    validate_options(argc, argv, remainder);

    if (niceness_set_)
    {   
        printf("Process niceness is set to %d\n",niceness_);
        fflush(stdout);
        setpriority(PRIO_PROCESS, getpid(), niceness_);
    }

    init_log();
    init_signals();
    init_random();

    if (daemonize_) {
        daemonizer_.daemonize(true);
    }
}

//----------------------------------------------------------------------
void
App::validate_options(int argc, char* const argv[], int remainder)
{
    if (remainder != argc) 
    {
        fprintf(stderr, "invalid argument '%s'\n", argv[remainder]);
        print_usage_and_exit();
    }
}

//----------------------------------------------------------------------
void
App::fill_options()
{
    fill_default_options(0);
}

//----------------------------------------------------------------------
void
App::fill_default_options(int flags)
{
    opts_.addopt(
        new BoolOpt('v', "version", &print_version_,
                           "print version information and exit"));

    opts_.addopt(
        new StringOpt('o', "output", &logfile_, "<output>",
                             "file name for logging output "
                             "(default - indicates stdout)"));

    opts_.addopt(
        new StringOpt('l', NULL, &loglevelstr_, "<level>",
                             "default log level [debug|warn|info|crit]"));

    opts_.addopt(
        new IntOpt('s', "seed", &random_seed_, "<seed>",
                          "random number generator seed", &random_seed_set_));

    opts_.addopt(
        new IntOpt('N', "niceness", &niceness_, "<niceness>",
                        "Setting the niceness range from -19 to 20", &niceness_set_));

    if (flags & DAEMONIZE_OPT) {
        opts_.addopt(
            new BoolOpt('d', "daemonize", &daemonize_,
                        "run as a daemon"));
    }

    if (flags & CONF_FILE_OPT) {
        opts_.addopt(
            new StringOpt('c', "conf", &conf_file_, "<conf>",
                          "set the configuration file", &conf_file_set_));
    }
}

//----------------------------------------------------------------------
void
App::init_log()
{
    // Parse the debugging level argument
    if (loglevelstr_.length() != 0) 
    {
        loglevel_ = str2level(loglevelstr_.c_str());
        if (loglevel_ == LOG_INVALID) 
        {
            fprintf(stderr, "invalid level value '%s' for -l option, "
                    "expected debug | info | warning | error | crit\n",
                    loglevelstr_.c_str());
            notify_and_exit(1);
        }
    }
    Log::init(logfile_.c_str(), loglevel_, "", debugpath_.c_str());

    if (daemonize_) {
        if (logfile_ == "-") {
            fprintf(stderr, "daemon mode requires setting of -o <logfile>\n");
            notify_and_exit(1);
        }
        
        Log::instance()->redirect_stdio();
    }
}

//----------------------------------------------------------------------
void
App::init_signals()
{
    FatalSignals::init(name_.c_str());

    Log::instance()->add_reparse_handler(SIGHUP);
    Log::instance()->add_rotate_handler(SIGUSR1);
    
    if (ignore_sigpipe_) {
        log_debug("ignoring SIGPIPE");
        signal(SIGPIPE, SIG_IGN);
    }
}

//----------------------------------------------------------------------
void
App::init_random()
{
    // seed the random number generator
    if (!random_seed_set_) 
    {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        random_seed_ = tv.tv_usec;
    }
    
    log_notice("random seed is %u\n", random_seed_);
    Random::seed(random_seed_);
}

//----------------------------------------------------------------------
void
App::notify_and_exit(char status)
{
    if (daemonize_) {
        daemonizer_.notify_parent(status);
    }
    exit(status);
}

} // namespace oasys
