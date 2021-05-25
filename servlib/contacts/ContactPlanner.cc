/*
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

#include <sys/time.h>
#include <time.h>

#include <third_party/oasys/io/IO.h>
#include <third_party/oasys/tclcmd/TclCommand.h>

#include "ContactPlanner.h"


template<> 
dtn::ContactPlanner* oasys::Singleton<dtn::ContactPlanner, false>::instance_ = NULL;

namespace dtn {

//----------------------------------------------------------------------
ContactPlanner::ContactPlanner()
    :Thread("ContactPlanner", CREATE_JOINABLE),
    manager_handle_(NULL)
{
}

//----------------------------------------------------------------------
ContactPlanner::~ContactPlanner()
{
}

//----------------------------------------------------------------------
void
ContactPlanner::do_init()
{
}

//----------------------------------------------------------------------
void
ContactPlanner::init()
{
    if (instance_ != NULL)
    {
        PANIC("ContactPlanner already initialized");
    }

    instance_ = new ContactPlanner();
    instance_->do_init();
} 

//----------------------------------------------------------------------
void
ContactPlanner::run()
{
  EndpointID tmp_eid = BundleDaemon::instance()->local_eid_ipn();
  int diff_seconds;
  while (1) {
    if (should_stop()) {
      log_debug("ContactPlanner: stopping");
      break;
    }
    if(contactPlan.size() > 0){
      oasys::Time tmp_now = oasys::Time::now();
      struct Contact_Entry tmp_entry = contactPlan.front();
      if(tmp_entry.link_ == NULL){
        if(manager_handle_->has_link(tmp_entry.link_name_.c_str())){
          tmp_entry.link_ = manager_handle_->find_link(tmp_entry.link_name_.c_str());
        }
      }
      if(tmp_entry.link_ != NULL){
        diff_seconds = tmp_now.in_seconds() - tmp_entry.start_time_.in_seconds();
        if(diff_seconds >= 0 && diff_seconds <= 5){
          Contact* tmp_contact = new Contact(tmp_entry.link_);
          tmp_contact->set_start_time(tmp_entry.start_time_);
          tmp_contact->set_duration(tmp_entry.duration_);
          tmp_entry.link_->set_contact_state(true, tmp_contact);
          std::string entry_id = std::to_string(tmp_entry.id_)+":"+std::to_string(tmp_entry.id_);
          std::string target_eid = tmp_entry.eid_;
          delete_contact(target_eid,entry_id);
        } else if(diff_seconds > 5) {
          std::string entry_id = std::to_string(tmp_entry.id_)+":"+std::to_string(tmp_entry.id_);
          std::string target_eid = tmp_entry.eid_;
          delete_contact(target_eid,entry_id);
        }
      }
    }
    usleep(100000);
  }
  reset_plan();
}

//----------------------------------------------------------------------
std::vector<dtn::ContactPlanner::Contact_Entry>
ContactPlanner::get_plan(std::string eid)
{
  std::vector<dtn::ContactPlanner::Contact_Entry> tmp_list;
  for (std::vector<dtn::ContactPlanner::Contact_Entry>::iterator it = contactList.begin(); it != contactList.end(); ++it)
  {
    if(eid.compare(it->eid_)==0)
    {
      int tmp_id;
      tmp_id = it->id_;
      struct Contact_Entry tmp_entry;
      tmp_entry.id_ = tmp_id;
      tmp_entry.eid_ = it->eid_; 
      tmp_entry.link_name_ = it->link_name_;
      tmp_entry.link_ = it->link_;
      tmp_entry.start_time_ = it->start_time_;
      tmp_entry.duration_ = it->duration_;
     
      tmp_list.push_back(tmp_entry);
    }
  }
  return tmp_list;
}

//----------------------------------------------------------------------
std::vector<dtn::ContactPlanner::Contact_Entry>
ContactPlanner::get_contact(std::string eid, std::string id_range)
{
  int start, end;
  char colon;
  std::stringstream ss;
  ss.str(id_range);
  ss>>start;
  ss>>colon;
  ss>>end;
  std::vector<dtn::ContactPlanner::Contact_Entry> tmp_list;
  for (std::vector<dtn::ContactPlanner::Contact_Entry>::iterator it = contactList.begin(); it != contactList.end(); ++it)
  {
    if(eid.compare(it->eid_)==0)
    {
      int tmp_id;
      tmp_id = it->id_;
      if(tmp_id >= start && tmp_id <=end)
      {
        struct Contact_Entry tmp_entry;
        tmp_entry.id_ = tmp_id;
        tmp_entry.eid_ = it->eid_; 
        tmp_entry.link_name_ = it->link_name_;
        tmp_entry.link_ = it->link_;
        tmp_entry.start_time_ = it->start_time_;
        tmp_entry.duration_ = it->duration_;
     
        tmp_list.push_back(tmp_entry);
      }
    }
  }
  return tmp_list;
}

//----------------------------------------------------------------------
bool
ContactPlanner::export_plan(std::string eid, std::string filename)
{
 std::ofstream export_file(filename);

 if(export_file)
 {
   for (std::vector<dtn::ContactPlanner::Contact_Entry>::iterator it = contactList.begin(); it != contactList.end(); ++it)
   {
     if(eid.compare(it->eid_)==0)
     {
       export_file << it->eid_;
       export_file << ",";
       export_file << it->link_name_;
       export_file << ",";
       time_t tmp_start = (time_t)it->start_time_.in_seconds();
       struct tm* tmp_info;
       tmp_info = localtime(&tmp_start);
       char time_buf[18];
       strftime(time_buf,18,"%Y:%j:%H:%M:%S",tmp_info);
       std::string start_time(time_buf);
       export_file << start_time;
       export_file << ",";
       export_file << it->duration_;
       export_file << std::endl;
     }
   }

   export_file.close();
   return true;
 }
 return false;
}

//----------------------------------------------------------------------
void
ContactPlanner::parse_plan(std::string filename)
{
    std::ifstream mapfile(filename.c_str());
    std::string line;
    if(!mapfile) {
      fprintf(stderr, "Error reading file: %s\n", filename.c_str());
    }
    int count = 0;
    while(std::getline(mapfile,line)){
      line.erase(0,line.find_first_not_of("\t\n\v\f\r "));
      line.erase(line.find_last_not_of("\t\n\v\f\r ")+1);
      if(line.empty()){
        continue;
      }
      if(line.at(0) == '#') {
        continue;
      }

      std::stringstream lineStream(line);
      std::string token;
      struct Contact_Entry row;

      //TO-DO validate options before pushing to vector

      count = 0;
      while(std::getline(lineStream,token,',')){
       if(!token.empty()){
         switch(count){
           case 0:
             row.eid_ = token;
             break;
           case 1:
             row.link_name_ = token;
             if(manager_handle_->has_link(token.c_str())){
               row.link_ = manager_handle_->find_link(token.c_str());
             } else {
               row.link_ = NULL;
             }
             break;
           case 2:
             struct tm start_time;
             strptime(token.c_str(),"%Y:%j:%H:%M:%S",&start_time);
             row.start_time_ = oasys::Time((double)mktime(&start_time));
             break;
           case 3:
             row.duration_ = (u_int32_t)std::stoi(token);
             break;
         }
         if(count == 3){
           break;
         }
         count++;
       } else {
         fprintf(stderr,"Error empty token found\n");
         break;
       }
      }
      if(count != 3){
         fprintf(stderr,"Error incorrect arguments on line: %s\n",line.c_str());
         fprintf(stderr,"Exiting...\n");
         exit(0);
         break;
      }
      contactList.push_back(row);
    }
    update_plan();
}

//----------------------------------------------------------------------
void
ContactPlanner::reset_plan()
{
  contactList.clear();
  contactPlan.clear();
}

//----------------------------------------------------------------------
void
ContactPlanner::update_plan()
{
  int id = 1;
  contactPlan.clear();
  std::sort(contactList.begin(),contactList.end(),Contact_Compare());
  for (std::vector<Contact_Entry>::iterator it = contactList.begin(); it != contactList.end(); ++it)
  {
    it->id_ = id;
    id++;
    EndpointID tmp_eid_a = BundleDaemon::instance()->local_eid_ipn();
    EndpointID tmp_eid_b;
    tmp_eid_b.assign(it->eid_);
    if(tmp_eid_a.compare(tmp_eid_b)==0)
    {
      struct Contact_Entry tmp_entry;
      tmp_entry.id_ = it->id_;
      tmp_entry.eid_ = it->eid_; 
      tmp_entry.link_name_ = it->link_name_;
      tmp_entry.link_ = it->link_;
      tmp_entry.start_time_ = it->start_time_;
      tmp_entry.duration_ = it->duration_;
      contactPlan.push_back(tmp_entry);
    }
  }
}

//----------------------------------------------------------------------
void
ContactPlanner::add_contact(std::string eid, std::string link_name, std::string start_time, std::string duration)
{
  struct Contact_Entry row;

  row.eid_ = eid;
  row.link_name_ = link_name;
  if(manager_handle_->has_link(link_name.c_str())){
    row.link_ = manager_handle_->find_link(link_name.c_str());
  } else {
    row.link_ = NULL;
  }
  struct tm tmp_time;
  strptime(start_time.c_str(),"%Y:%j:%H:%M:%S",&tmp_time);
  row.start_time_ = oasys::Time((double)mktime(&tmp_time));
  row.duration_ = (u_int32_t)std::stoi(duration);

  contactList.push_back(row);

  update_plan();
}

//----------------------------------------------------------------------
void
ContactPlanner::delete_contact(std::string eid, std::string id_range)
{
  int start, end;
  char colon;
  std::stringstream ss;
  ss.str(id_range);
  ss>>start;
  ss>>colon;
  ss>>end;
  for (auto it = contactList.begin(); it != contactList.end(); ++it)
  {
    if(eid.compare(it->eid_)==0)
    {
      if(it->id_ >= start && it->id_ <= end)
      {
        contactList.erase(it);
        it--;
      }
    }
  }
  update_plan();
}
} // namespace dtn

