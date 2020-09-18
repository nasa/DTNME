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

#ifndef _OASYS_GETOPT_H_
#define _OASYS_GETOPT_H_

#include <string>
#include <vector>
#include "Options.h"

namespace oasys {

/*
 * Wrapper class for a call to ::getopt.
 */
class Getopt {
public:
    /**
     * Constructor (does nothing).
     */
    Getopt();
     
    /**
     * Destructor to clean up all the opts.
     */
    ~Getopt();
    
    /**
     * Register a new option binding.
     */
    void addopt(Opt* opt);

    /**
     * Parse argv, processing all registered getopt. Returns the
     * index of the first non-option argument in argv
     * 
     * @param progname   	the name of the executable
     * @param argc	   	count of command line args
     * @param argv   		command line arg values
     * @param extra_opts 	additional usage string
     */
    int getopt(const char* progname, int argc, char* const argv[],
               const char* extra_usage = "");
    
    /**
     * Prints a nicely formatted usage string to stderr.
     *
     * @param progname   	the name of the executable
     * @param extra_opts 	additional usage string
     */
    void usage(const char* progname,
               const char* extra_usage = "");
    
protected:
    typedef std::vector<Opt*> OptList;
    
    Opt*    opts_[256];	// indexed by option character
    OptList allopts_;	// list of all getopt
};

} // namespace oasys

#endif /* _OASYS_GETOPT_H_ */
