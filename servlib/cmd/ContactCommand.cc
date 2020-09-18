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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <oasys/util/StringBuffer.h>

#include "ContactCommand.h"

namespace dtn {

ContactCommand::ContactCommand()
    : TclCommand("contact")
{

    add_to_help("load plan <Filename>", "load a Contact Plan file formatted as a CSV file");
    add_to_help("view plan <EID>", "view contact plan for specified EID");
    add_to_help("reset plan", "delete all contact plans");
    add_to_help("add contact <EID> <Link Name> <Start Time> <Stop Time> <Throttle Rate>", "add contact based on provided parameters; time values should be formatted YYY:DOY:HH:MM:SS and throttle rates should be calculated in bits per second");
    add_to_help("delete contact <EID> <Contact ID List>", "delete one or more contacts from a given EID; Contact ID List should be formatted as number:number");
    add_to_help("view contact <EID> <Contact ID List>", "view one or more contacts from a given EID; Contact ID List should be formatted as number:number");

}

int
ContactCommand::exec(int argc, const char** argv, Tcl_Interp* interp)
{
    (void)interp;
    
    if(strcmp(argv[1], "load") == 0) {
          if (argc != 3) {
              wrong_num_args(argc, argv, 1, 3, 3);
              return TCL_ERROR;
          }
          //ContactPlanner::instace()->parse(argv[3]);
    }
    else if(strcmp(argv[1], "reset") == 0) {
          if (argc != 2) {
              wrong_num_args(argc, argv, 1, 2, 2);
              return TCL_ERROR;
          }
    }
    else if(strcmp(argv[1], "add") == 0) {
          if (argc != 7) {
              wrong_num_args(argc, argv, 1, 7, 7);
              return TCL_ERROR;
          }
    }
    else if(strcmp(argv[1], "delete") == 0) {
          if (argc != 4) {
              wrong_num_args(argc, argv, 1, 4, 4);
              return TCL_ERROR;
          }
    }
    else if(strcmp(argv[1], "view") == 0) {
      if (strcmp(argv[2], "plan") == 0) {
          if (argc != 4) {
              wrong_num_args(argc, argv, 1, 4, 4);
              return TCL_ERROR;
          }
      }
      else if (strcmp(argv[2], "contact") == 0) {
          if (argc != 4) {
              wrong_num_args(argc, argv, 1, 4, 4);
              return TCL_ERROR;
          }
      }
    }

 
    return TCL_OK;
}

} // namespace dtn
