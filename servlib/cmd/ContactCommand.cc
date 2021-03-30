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

#include <third_party/oasys/util/StringBuffer.h>

#include "ContactCommand.h"
#include "bundling/BundleDaemon.h"
#include "contacts/ContactPlanner.h"

namespace dtn {

ContactCommand::ContactCommand()
    : TclCommand("contact")
{

    add_to_help("load plan <Filename>", "load a Contact Plan file formatted as a CSV file");
    add_to_help("export plan <EID> <Filepath>","export contact plan for specified EID to a CSV file");
    add_to_help("view plan <EID>", "view contact plan for specified EID");
    add_to_help("reset plan", "delete all contact plans");
    add_to_help("add contact <EID> <Link Name> <Start Time> <Duration>", "add contact based on provided parameters; time values should be formatted YYYY:DOY:HH:MM:SS and duration is in seconds");
    add_to_help("delete contact <EID> <Contact ID List>", "delete one or more contacts from a given EID; Contact ID List should be formatted as number:number");
    add_to_help("view contact <EID> <Contact ID List>", "view one or more contacts from a given EID; Contact ID List should be formatted as number:number");

}

int
ContactCommand::exec(int argc, const char** argv, Tcl_Interp* interp)
{
    (void)interp;

    if(strcmp(argv[1], "load") == 0) {
          if (argc != 4) {
              wrong_num_args(argc, argv, 1, 4, 4);
              return TCL_ERROR;
          }
          ContactPlanner::instance()->parse_plan(argv[3]);
          oasys::StringBuffer buf;
          std::string eid = BundleDaemon::instance()->local_eid_ipn().str();
          buf.append("\nPlan Loaded for "+eid+"\n");
          buf.append("Format: <Contact ID> <EID> <Link Name> <Start Time> <Duration>\n");
          std::vector<dtn::ContactPlanner::Contact_Entry> tmpList;
          tmpList = ContactPlanner::instance()->get_plan(eid);
          for (std::vector<dtn::ContactPlanner::Contact_Entry>::iterator it = tmpList.begin(); it != tmpList.end(); ++it)
          {
            std::string contact_id = std::to_string(it->id_);
            std::string eid = it->eid_;
            std::string link_name = it->link_name_;
            time_t tmp_start = (time_t)it->start_time_.in_seconds();
            struct tm* tmp_info;
            tmp_info = localtime(&tmp_start);
            char time_buf[18];
            strftime(time_buf,18,"%Y:%j:%H:%M:%S",tmp_info);
            std::string start_time(time_buf);
            std::stringstream ss_duration;
            ss_duration << it->duration_;
            std::string duration;
            ss_duration >> duration;
            buf.append(contact_id+" "); 
            buf.append(eid+" "); 
            buf.append(link_name+" ");
            buf.append(start_time+" ");
            buf.append(duration+" ");
          }
          set_result(buf.c_str());
    }
    else if(strcmp(argv[1], "export") == 0) {
          if (argc != 5) {
              wrong_num_args(argc, argv, 1, 5, 5);
              return TCL_ERROR;
          }
          bool export_plan = ContactPlanner::instance()->export_plan(argv[3],argv[4]);
          if(export_plan) {
            resultf("Contact Plan has been exported");
          } else {
            resultf("Contact Plan was not exported. Export Failed.");
          }
    }
    else if(strcmp(argv[1], "reset") == 0) {
          if (argc != 3) {
              wrong_num_args(argc, argv, 1, 3, 3);
              return TCL_ERROR;
          }
          ContactPlanner::instance()->reset_plan();
          resultf("Contact Plan has been reset");
    }
    else if(strcmp(argv[1], "add") == 0) {
          if (argc != 7) {
              wrong_num_args(argc, argv, 1, 7, 7);
              return TCL_ERROR;
          }
          struct tm time_check;
          if(!strptime(argv[5],"%Y:%j:%H:%M:%S",&time_check)){
            resultf("Invalid format for Start Time. Format should be YYYY:DOY:HH:MM:SS");
            return TCL_ERROR;
          }
          ContactPlanner::instance()->add_contact(argv[3],argv[4],argv[5],argv[6]);
          oasys::StringBuffer buf;
          std::string eid(argv[3]);
          buf.append("\nUpdated Plan for "+eid+"\n");
          buf.append("Format: <Contact ID> <EID> <Link Name> <Start Time> <Duration>\n");
          std::vector<dtn::ContactPlanner::Contact_Entry> tmpList;
          tmpList = ContactPlanner::instance()->get_plan(argv[3]);
          for (std::vector<dtn::ContactPlanner::Contact_Entry>::iterator it = tmpList.begin(); it != tmpList.end(); ++it)
          {
            std::string contact_id = std::to_string(it->id_);
            std::string eid = it->eid_;
            std::string link_name = it->link_name_;
            time_t tmp_start = (time_t)it->start_time_.in_seconds();
            struct tm* tmp_info;
            tmp_info = localtime(&tmp_start);
            char time_buf[18];
            strftime(time_buf,18,"%Y:%j:%H:%M:%S",tmp_info);;
            std::string start_time(time_buf);;
            std::stringstream ss_duration;
            ss_duration << it->duration_;
            std::string duration;
            ss_duration >> duration;
            buf.append(contact_id+" "); 
            buf.append(eid+" "); 
            buf.append(link_name+" ");
            buf.append(start_time+" ");
            buf.append(duration+" ");
          }
          set_result(buf.c_str());
    }
    else if(strcmp(argv[1], "delete") == 0) {
          if (argc != 5) {
              wrong_num_args(argc, argv, 1, 5, 5);
              return TCL_ERROR;
          }
          ContactPlanner::instance()->delete_contact(argv[3],argv[4]);
          oasys::StringBuffer buf;
          std::string eid(argv[3]);
          buf.append("\nUpdated Plan for "+eid+"\n");
          buf.append("Format: <Contact ID> <EID> <Link Name> <Start Time> <Duration>\n");
          std::vector<dtn::ContactPlanner::Contact_Entry> tmpList;
          tmpList = ContactPlanner::instance()->get_plan(argv[3]);
          for (std::vector<dtn::ContactPlanner::Contact_Entry>::iterator it = tmpList.begin(); it != tmpList.end(); ++it)
          {
            std::string contact_id = std::to_string(it->id_);
            std::string eid = it->eid_;
            std::string link_name = it->link_name_;
            time_t tmp_start = (time_t)it->start_time_.in_seconds();
            struct tm* tmp_info;
            tmp_info = localtime(&tmp_start);
            char time_buf[18];
            strftime(time_buf,18,"%Y:%j:%H:%M:%S",tmp_info);;
            std::string start_time(time_buf);;
            std::stringstream ss_duration;
            ss_duration << it->duration_;
            std::string duration;
            ss_duration >> duration;
            buf.append(contact_id+" "); 
            buf.append(eid+" "); 
            buf.append(link_name+" ");
            buf.append(start_time+" ");
            buf.append(duration+" ");
          }
          set_result(buf.c_str());
    }
    else if(strcmp(argv[1], "view") == 0) {
      if (strcmp(argv[2], "plan") == 0) {
          if (argc != 4) {
              wrong_num_args(argc, argv, 1, 4, 4);
              return TCL_ERROR;
          }
          oasys::StringBuffer buf;
          std::string eid(argv[3]);
          buf.append("\nViewing Plan for "+eid+"\n");
          buf.append("Format: <Contact ID> <EID> <Link Name> <Start Time> <Duration>\n");
          std::vector<dtn::ContactPlanner::Contact_Entry> tmpList;
          tmpList = ContactPlanner::instance()->get_plan(argv[3]);
          for (std::vector<dtn::ContactPlanner::Contact_Entry>::iterator it = tmpList.begin(); it != tmpList.end(); ++it)
          {
            std::string contact_id = std::to_string(it->id_);
            std::string eid = it->eid_;
            std::string link_name = it->link_name_;
            time_t tmp_start = (time_t)it->start_time_.in_seconds();
            struct tm* tmp_info;
            tmp_info = localtime(&tmp_start);
            char time_buf[18];
            strftime(time_buf,18,"%Y:%j:%H:%M:%S",tmp_info);;
            std::string start_time(time_buf);;
            std::stringstream ss_duration;
            ss_duration << it->duration_;
            std::string duration;
            ss_duration >> duration;
            buf.append(contact_id+" "); 
            buf.append(eid+" "); 
            buf.append(link_name+" ");
            buf.append(start_time+" ");
            buf.append(duration+" ");
          }
          set_result(buf.c_str());
      }
      else if (strcmp(argv[2], "contact") == 0) {
          if (argc != 5) {
              wrong_num_args(argc, argv, 1, 5, 5);
              return TCL_ERROR;
          }
          oasys::StringBuffer buf;
          std::string eid(argv[3]);
          buf.append("\nViewing Contacts for "+eid+"\n");
          buf.append("Format: <Contact ID> <EID> <Link Name> <Start Time> <Duration>\n");
          std::vector<dtn::ContactPlanner::Contact_Entry> tmpList;
          tmpList = ContactPlanner::instance()->get_contact(argv[3],argv[4]);
          for (std::vector<dtn::ContactPlanner::Contact_Entry>::iterator it = tmpList.begin(); it != tmpList.end(); ++it)
          {
            std::string contact_id = std::to_string(it->id_);
            std::string eid = it->eid_;
            std::string link_name = it->link_name_;
            time_t tmp_start = (time_t)it->start_time_.in_seconds();
            struct tm* tmp_info;
            tmp_info = localtime(&tmp_start);
            char time_buf[18];
            strftime(time_buf,18,"%Y:%j:%H:%M:%S",tmp_info);;
            std::string start_time(time_buf);;
            std::stringstream ss_duration;
            ss_duration << it->duration_;
            std::string duration;
            ss_duration >> duration;
            buf.append(contact_id+" "); 
            buf.append(eid+" "); 
            buf.append(link_name+" ");
            buf.append(start_time+" ");
            buf.append(duration+" ");
          }
          set_result(buf.c_str());
      }
    }
 
    return TCL_OK;
}

} // namespace dtn
