#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <inttypes.h>


#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/time.h>
#include <time.h>
#include <bitset>

#include "dtn_api.h"
#include "APIEndpointIDOpt.h"

#include <third_party/oasys/util/Getopt.h>

#include "deliver_me.h"

namespace oasys {
    template <> deliver_me::Deliver_Me* oasys::Singleton<deliver_me::Deliver_Me>::instance_ = 0;
}

namespace deliver_me {

//----------------------------------------------------------------------
Deliver_Me::Deliver_Me()
    : App("Deliver_Me", "deliver_me"),
      api_address_(""),
      api_port_(0),
      local_entity_id_(""),
      file_segment_size_(0),
      ttl_(0),
      log_path_(""),
      sequence_number_(0),
      bundle_version_(7),
      should_shutdown_(false)
{
}

//----------------------------------------------------------------------
void
Deliver_Me::fill_options()
{
    //App::fill_default_options(DAEMONIZE_OPT);

    opts_.addopt(new oasys::StringOpt("a", &api_address_, "<api_address>","address used to connect to the DTNME instance"));
    opts_.addopt(new oasys::UIntOpt("p", &api_port_, "<api_port>","port used to connect to the DTNME instance"));
    opts_.addopt(new oasys::StringOpt("e", &local_entity_id_, "<entity_id>","id to represent the local cfdp entity"));
    opts_.addopt(new oasys::UIntOpt("s", &file_segment_size_, "<file_segment_size>","size used to define how much of file to send per bundle"));
    opts_.addopt(new oasys::StringOpt("l", &log_path_, "<log_path>","path used to store logs"));
    opts_.addopt(new oasys::UIntOpt("t", &ttl_, "<time_to_live>","set the time to live to be used by bundles transmitted from this entity"));
    opts_.addopt(new oasys::UIntOpt("c", &checksum_type_, "<checksum_type>","set the checksum type to be used for calculating checksums on received data"));
    opts_.addopt(new oasys::UIntOpt("v", &bundle_version_, "<bundle_version>","set the bundle protocol version to be used. Options are 6 or 7. Default is 7."));
    
}

//----------------------------------------------------------------------
void
Deliver_Me::validate_options(int argc, char* const argv[], int remainder)
{
    if (remainder != argc) {
      fprintf(stderr, "invalid argument '%s'\n", argv[remainder]);
      print_usage_and_exit();
    }
    if(api_address_.empty()){
      fprintf(stdout, "Must set API Address\n");
      print_usage_and_exit();
    }
    if(api_port_ == 0){
      fprintf(stdout, "Must set API Port\n");
      print_usage_and_exit();
    }
    if(local_entity_id_.empty()){
      fprintf(stdout, "Must set Entity ID\n");
      print_usage_and_exit();
    }
    if(file_segment_size_ == 0){
      fprintf(stdout, "Must set File Segment Size\n");
      print_usage_and_exit();
    }
    if(log_path_.empty()){
      fprintf(stdout, "Must set Log Path\n");
      print_usage_and_exit();
    }

}

//----------------------------------------------------------------------
void
Deliver_Me::init_threads()
{
   Bundle_Receiver *receiver = new Bundle_Receiver(this);
   receiver->start();
   Bundle_Transmitter *transmitter = new Bundle_Transmitter(this);
   transmitter->start();
   Process_PDU *pdu = new Process_PDU(this);
   pdu->start();
   Process_Primitive *primitive = new Process_Primitive(this);
   primitive->start();
   Log_Worker *log = new Log_Worker(this);
   log->start();
}

//----------------------------------------------------------------------
void
Deliver_Me::cli()
{
  bool valid = false;
  std::vector<std::string> valid_types = {"put.request","transaction.list","exit"};
  std::vector<std::string> args;
  std::string input;
  fprintf(stdout,"\n|----Usage----|\n");
  for(u_int i=0;i<valid_types.size();i++){
    if(i == 0){
      fprintf(stdout,"%s <destination_entity_id> <source_filename> <destination_filename> <checksum_type>\n",valid_types[i].c_str());
    } else {
      fprintf(stdout,"%s\n",valid_types[i].c_str());
    }
  }
  fprintf(stdout,"|-------------|\n");
  fprintf(stdout,"deliver_me: ");
  fflush(stdout);

  std::getline(std::cin,input);
  if(input.size()>0){
    std::string tmp;
    std::stringstream ss(input);
    while(getline(ss,tmp,' ')){
      args.push_back(tmp);
    }
    std::string command = args[0];
    std::transform(command.begin(),command.end(),command.begin(),[](unsigned char c){return std::tolower(c);});
    for(u_int i=0;i<valid_types.size();i++){
      if(command == valid_types[i]){
        if(i == 2){
          should_shutdown_ = true;
        }
        if((i == 0 && args.size() == 5) || args.size() == 1){
          valid = true;
        }
      }
    }
  }
  if(valid){
    primitive_lock_.lock("Deliver_Me::cli");
    primitive_buffer_.push_back(args);
    primitive_lock_.unlock();
  } else {
    fprintf(stdout,"Error: %s is an invalid input\n",input.c_str());
    fflush(stdout);
  }
}

//----------------------------------------------------------------------
int
Deliver_Me::main(int argc, char* argv[])
{
    init_app(argc, argv);

    //if (daemonize_) {
    //    daemonizer_.notify_parent(0);
    //}

    init_threads();

    while (!should_shutdown_) {
      cli();
      transaction_lock_.lock("Deliver_Me::main");
      for(int i=0;i<transaction_pool_.size();i++){
        if(!transaction_pool_[i]->should_work_){
          transaction_pool_.erase(transaction_pool_.begin()+i);
        }
      }
      transaction_lock_.unlock();
      usleep(1000);
    }

    return 0;
}

//----------------------------------------------------------------------
Deliver_Me::Log_Worker::Log_Worker(Deliver_Me *handle)
     : Thread("Deliver_Me::Log_Worker", Thread::DELETE_ON_EXIT),
       handle_(handle)
{
}

//----------------------------------------------------------------------
Deliver_Me::Log_Worker::~Log_Worker()
{
}

//----------------------------------------------------------------------
void
Deliver_Me::Log_Worker::run()
{
  std::string filename;
  bool fallback = false;
  if(handle_->log_path_.back() == '/'){
    filename = handle_->log_path_+"deliver_me.log";
  } else {
    filename = handle_->log_path_+"/"+"deliver_me.log";
  }

  std::ofstream log_stream;
  log_stream.open(filename, std::ofstream::out | std::ofstream::app);
  if(!log_stream){
    fprintf(stdout,"Error: unable to open %s logs will be written to stdout",filename.c_str());
    fflush(stdout);
  }

  while(!handle_->should_shutdown_){
  if(fallback){
    handle_->log_lock_.lock("Deliver_Me::Log_Worker::run");
    if(handle_->log_buffer_.size() > 0){
      std::string log = handle_->log_buffer_.front(); 
      handle_->log_buffer_.pop_front();
      handle_->log_lock_.unlock();
      fprintf(stdout,"%s",log.c_str());
      fflush(stdout);
    } else {
      handle_->log_lock_.unlock();
    }
  }else {
    if(log_stream.is_open()){
      long pos = log_stream.tellp();
      long max = 52428800;
      if(pos < max){
        handle_->log_lock_.lock("Deliver_Me::Log_Worker::run");
        if(handle_->log_buffer_.size() > 0){
          std::string log = handle_->log_buffer_.front(); 
          handle_->log_buffer_.pop_front();
          handle_->log_lock_.unlock();
          log_stream.write(log.c_str(),log.size());
          log_stream.flush();
        } else {
          handle_->log_lock_.unlock();
        }
      }
    } else {
      if(log_stream.is_open()){
        log_stream.close();
      }
      std::string new_filename;
      if(handle_->log_path_.back() == '/'){
        new_filename = handle_->log_path_+"deliver_me";
      } else {
        new_filename = handle_->log_path_+"/"+"deliver_me";
      }
      time_t now=time(0);
      tm* local = localtime(&now);
      char time_buf[20];
      strftime(time_buf,sizeof(time_buf),"%Y_%j_%H_%M_%S",local);
      new_filename.append(time_buf);
      new_filename += ".log.gz";
      std::string command = "gzip -c "+filename+" > "+new_filename;
      if(!std::system(command.c_str())){
        fprintf(stdout,"Error: Failed to rotate log file rerouting logs to stdout");
        fflush(stdout);
        fallback = true;
      } else if(!fallback) {
        log_stream.open(filename, std::ofstream::out | std::ofstream::app);
      }
    }
  }
  usleep(1000);
  }
}

//----------------------------------------------------------------------
Deliver_Me::Bundle_Receiver::Bundle_Receiver(Deliver_Me *handle)
     : Thread("Deliver_Me::Bundle_Receiver", Thread::DELETE_ON_EXIT),
       handle_(handle)
{
    u_int32_t receive_regid;
    
    std::string receive_eid = "ipn:" + handle_->local_entity_id_ + "." + handle_->RECEIVER_SERVICE_TAG;

    dtn_parse_eid_string(&receive_eid_, receive_eid.c_str());

    dtn_handle_t r_handle;
    receive_handle_ = r_handle;
    //Attempt to connect the Receive Handle to the DTN API
    int size = handle_->api_address_.size()+1;
    char* api_address = new char[size];
    strcpy(api_address,handle_->api_address_.c_str());
    if(dtn_open_with_IP(api_address,handle_->api_port_,&receive_handle_) !=0){
      handle_->log_lock_.lock("Deliver_Me::Bundle_Receiver");
      handle_->log_buffer_.push_back("Error: Bundle_Receiver unable to open connection to DTNME API\n");
      handle_->log_lock_.unlock();
    }
    handle_->log_lock_.lock("Deliver_Me::Bundle_Receiver");
    handle_->log_buffer_.push_back("Attempting to register Receive Handle\n");
    handle_->log_lock_.unlock();
    //Check if registration already exists for the Receive EID that we want to use
    if(dtn_find_registration(receive_handle_,&receive_eid_, &receive_regid) == 0){
      handle_->log_lock_.lock("Deliver_Me::Bundle_Receiver");
      handle_->log_buffer_.push_back("Previous Receive Handle Registration was found attempting to bind to it\n");
      handle_->log_lock_.unlock();
      //Registration was found. Attempt to bind to the existing registration
      if(dtn_bind(receive_handle_,receive_regid)){
        handle_->log_lock_.lock("Deliver_Me::Bundle_Receiver");
        handle_->log_buffer_.push_back("Error: Failed to bind to existing Receive Handle Registration\n");
        handle_->log_lock_.unlock();
        abort();
      }
      receive_regid_ = receive_regid;
    } else {
      //Registration was not found. Verify the Receive Handle is in a good state and generate new registration.
      if(dtn_errno(receive_handle_) == DTN_ENOTFOUND){
        dtn_reg_info_t reginfo;
        memset(&reginfo, 0, sizeof(reginfo));
        dtn_copy_eid(&reginfo.endpoint, &receive_eid_);
        reginfo.flags = DTN_REG_DEFER;
        reginfo.expiration = 60 * 60 * 24 * 365; // 1 year

        if(dtn_register(receive_handle_, &reginfo, &receive_regid) == 0) {
          receive_regid_ = reginfo.regid;
          handle_->log_lock_.lock("Deliver_Me::Bundle_Receiver");
          handle_->log_buffer_.push_back("Receive Handle Registration successful\n");
          handle_->log_lock_.unlock();
        } else {
          handle_->log_lock_.lock("Deliver_Me::Bundle_Receiver");
          handle_->log_buffer_.push_back("Error: Receive Handle Registration failed\n");
          handle_->log_lock_.unlock();
        }
      } else {
        handle_->log_lock_.lock("Deliver_Me::Bundle_Receiver");
        handle_->log_buffer_.push_back("Error: Receive Handle not found\n");
        handle_->log_lock_.unlock();
      }
    }
}

//----------------------------------------------------------------------
Deliver_Me::Bundle_Receiver::~Bundle_Receiver()
{
    dtn_close(receive_handle_);
}

//----------------------------------------------------------------------
void
Deliver_Me::Bundle_Receiver::run()
{
    dtn_bundle_spec_t    spec;
    dtn_bundle_payload_t payload;

    memset(&spec,    0, sizeof(spec));
    memset(&payload, 0, sizeof(payload));

    while (!handle_->should_shutdown_) {
      usleep(1000);
      if(dtn_recv(receive_handle_, &spec, DTN_PAYLOAD_MEM, &payload, DTN_TIMEOUT_INF) != 0){
        handle_->log_lock_.lock("Deliver_Me::Bundle_Receiver::run");
        handle_->log_buffer_.push_back("Error: Bundle Receive failed\n");
        handle_->log_lock_.unlock();
        dtn_free_payload(&payload);
        continue;
      }

      int len = payload.buf.buf_len;
      char* buf=(char*)malloc(len);
      memcpy(buf,payload.buf.buf_val,len);

      handle_->receive_lock_.lock("Deliver_Me::Bundle_Receiver::run");
      handle_->receive_buffer_.push_back({spec,buf,len});
      handle_->receive_lock_.unlock();

      dtn_free_payload(&payload);
    }
}

//----------------------------------------------------------------------
Deliver_Me::Bundle_Transmitter::Bundle_Transmitter(Deliver_Me *handle)
     : Thread("Deliver_Me::Bundle_Transmitter", Thread::DELETE_ON_EXIT),
       handle_(handle)
{
    u_int32_t transmit_regid;
    
    std::string transmit_eid = "ipn:" + handle_->local_entity_id_ + "." + handle_->TRANSMITTER_SERVICE_TAG;

    dtn_parse_eid_string(&transmit_eid_, transmit_eid.c_str());

    dtn_handle_t t_handle;
    transmit_handle_ = t_handle;
    //Attempt to connect the Receive Handle to the DTN API
    int size = handle_->api_address_.size()+1;
    char* api_address = new char[size];
    strcpy(api_address,handle_->api_address_.c_str());
    if(dtn_open_with_IP(api_address,handle_->api_port_,&transmit_handle_) !=0){
      handle_->log_lock_.lock("Deliver_Me::Bundle_Transmitter");
      handle_->log_buffer_.push_back("Error: Bundle_Transmitter unable to open connection to DTNME API\n");
      handle_->log_lock_.unlock();
    }
    handle_->log_lock_.lock("Deliver_Me::Bundle_Transmitter");
    handle_->log_buffer_.push_back("Attempting to register Transmit Handle\n");
    handle_->log_lock_.unlock();
    //Check if registration already exists for the Transmit EID that we want to use
    if(dtn_find_registration(transmit_handle_,&transmit_eid_, &transmit_regid) == 0){
      handle_->log_lock_.lock("Deliver_Me::Bundle_Transmitter");
      handle_->log_buffer_.push_back("Previous Transmit Handle Registration was found attempting to bind to it\n");
      handle_->log_lock_.unlock();
      //Registration was found. Attempt to bind to the existing registration
      if(dtn_bind(transmit_handle_,transmit_regid)){
        handle_->log_lock_.lock("Deliver_Me::Bundle_Transmitter");
        handle_->log_buffer_.push_back("Error: Failed to bind to existing Transmit Handle Registration\n");
        handle_->log_lock_.unlock();
        abort();
      }
      transmit_regid_ = transmit_regid;
    } else {
      //Registration was not found. Verify the Receive Handle is in a good state and generate new registration.
      if(dtn_errno(transmit_handle_) == DTN_ENOTFOUND){
        dtn_reg_info_t reginfo;
        memset(&reginfo, 0, sizeof(reginfo));
        dtn_copy_eid(&reginfo.endpoint, &transmit_eid_);
        reginfo.flags = DTN_REG_DEFER;
        reginfo.expiration = 60 * 60 * 24 * 365; // 1 year

        if(dtn_register(transmit_handle_, &reginfo, &transmit_regid) == 0) {
          transmit_regid_ = reginfo.regid;
          handle_->log_lock_.lock("Deliver_Me::Bundle_Transmitter");
          handle_->log_buffer_.push_back("Transmit Handle Registration successful\n");
          handle_->log_lock_.unlock();
        } else {
          handle_->log_lock_.lock("Deliver_Me::Bundle_Transmitter");
          handle_->log_buffer_.push_back("Error: Transmit Handle Registration failed\n");
          handle_->log_lock_.unlock();
        }
      } else {
        handle_->log_lock_.lock("Deliver_Me::Bundle_Transmitter");
        handle_->log_buffer_.push_back("Error: Transmit Handle not found\n");
        handle_->log_lock_.unlock();
      }
    }
}

//----------------------------------------------------------------------
Deliver_Me::Bundle_Transmitter::~Bundle_Transmitter()
{
    dtn_close(transmit_handle_);
}

//----------------------------------------------------------------------
void
Deliver_Me::Bundle_Transmitter::run()
{
    while(!handle_->should_shutdown_){
      handle_->transmit_lock_.lock("Deliver_Me::Bundle_Transmitter::run");
      if(handle_->transmit_buffer_.size() > 0){
        dtn_bundle_id_t bundle_id;
        memset(&bundle_id, 0, sizeof(bundle_id));
        dtn_bundle_payload_t payload;
        dtn_set_payload(&payload, DTN_PAYLOAD_MEM, handle_->transmit_buffer_.front().data, handle_->transmit_buffer_.front().length);
        if(dtn_send(transmit_handle_, DTN_REG_DEFER, &handle_->transmit_buffer_.front().spec, &payload, &bundle_id) == 0){

          delete[] handle_->transmit_buffer_.front().data;

          handle_->transmit_buffer_.pop_front();

        } else {
          std::string log = "Error: Bundle Transmit failed: "+std::to_string(dtn_errno(transmit_handle_));
          handle_->log_lock_.lock("Deliver_Me::Bundle_Transmitter::run");
          handle_->log_buffer_.push_back(log);
          handle_->log_lock_.unlock();
        }
      }
      handle_->transmit_lock_.unlock();
      usleep(1000);
    }
}

//----------------------------------------------------------------------
Deliver_Me::Process_Primitive::Process_Primitive(Deliver_Me *handle)
     : Thread("Deliver_Me::Process_Primitive", Thread::DELETE_ON_EXIT),
       handle_(handle)
{

}

//---------------------------------------------------------------------
Deliver_Me::Process_Primitive::~Process_Primitive()
{
}

//----------------------------------------------------------------------
void
Deliver_Me::Process_Primitive::run()
{
  std::vector<std::string> valid_types = {"put.request","transaction.list"};
  while(!handle_->should_shutdown_){
    handle_->primitive_lock_.lock("Deliver_Me::Process_Primitive::run");
    if(!handle_->primitive_buffer_.empty()){
      if(handle_->primitive_buffer_.front()[0] == valid_types[0]){
          handle_->sequence_lock_.lock("Deliver_Me::Process_Primitive::run");
          u_int sequence_number = ++handle_->sequence_number_;
          handle_->sequence_lock_.unlock();

          //Generate Meta PDU and bundle then pass to receive buffer
          std::vector<std::string> args = handle_->primitive_buffer_.front();
          handle_->primitive_lock_.unlock();
          std::string log_a = "Preparing to send file: "+args[2];
          handle_->log_lock_.lock("Deliver_Me::Process_Primitive::run");
          handle_->log_buffer_.push_back(log_a);
          handle_->log_lock_.unlock();
          std::fstream storage_;
          storage_.open(args[2], std::fstream::in | std::fstream::binary);
          if(!storage_.good()){
            handle_->log_lock_.lock("Deliver_Me::Process_Primitive::run");
            handle_->log_buffer_.push_back("Error: Could not open File Path");
            handle_->log_lock_.unlock();
          }
          storage_.seekg(0,std::ios::end);
          unsigned long long file_size = storage_.tellg();

          handle_->stats_lock_.lock("Deliver_Me::Process_Primitive::run");
          handle_->stats_buffer_.push_back({sequence_number,file_size,0,false,handle_->local_entity_id_,args[1],args[2],args[3]});
          int stats_index = handle_->stats_buffer_.size()-1;
          handle_->stats_lock_.unlock();

          //Variable File Size
          int value_size = 0;
          if(file_size <= 4294967295){
            value_size = 4;
          } else {
            value_size = 8;
          }
          char buffer[handle_->file_segment_size_];


          uint32_t src_filename_len = args[2].size();
          uint32_t dst_filename_len = args[3].size();

          int pdu_length = 1 + 1 + value_size + 1 + src_filename_len + 1 + dst_filename_len;

          char source_path[src_filename_len+1];
          std::strcpy(source_path,args[2].c_str());
          char destination_path[dst_filename_len+1];
          std::strcpy(destination_path,args[3].c_str());

          unsigned char bits_meta_header[pdu_length];

          int cursor = 0;
          //Set File directive bits to 7
          bits_meta_header[0] = 0x07;
          // closure not requested; checksum type null
          bits_meta_header[1] = 0x0F;
          //Set File Size
          if(value_size == 4){
            bits_meta_header[2] = (storage_.tellg()>>24) & 0xFF;
            bits_meta_header[3] = (storage_.tellg()>>16) & 0xFF;
            bits_meta_header[4] = (storage_.tellg()>>8) & 0xFF;
            bits_meta_header[5] = storage_.tellg() & 0xFF;
            cursor = 5;
          }
          if(value_size == 8){
            bits_meta_header[2] = (storage_.tellg()>>56) & 0xFF;
            bits_meta_header[3] = (storage_.tellg()>>48) & 0xFF;
            bits_meta_header[4] = (storage_.tellg()>>40) & 0xFF;
            bits_meta_header[5] = (storage_.tellg()>>32) & 0xFF;
            bits_meta_header[6] = (storage_.tellg()>>24) & 0xFF;
            bits_meta_header[7] = (storage_.tellg()>>16) & 0xFF;
            bits_meta_header[8] = (storage_.tellg()>>8) & 0xFF;
            bits_meta_header[9] = storage_.tellg() & 0xFF;
            cursor = 9;
          }
          
          //Set Source Path Size

          bits_meta_header[++cursor] = src_filename_len & 0xFF; 

          //Set Source Path
          for(uint32_t i=0; i<src_filename_len; i++){
            bits_meta_header[++cursor] = (unsigned char)source_path[i];
          }

          //Set Destination Path Size
          bits_meta_header[++cursor] = dst_filename_len & 0xFF;
          //Set Destination Path
          for(uint32_t i=0; i<dst_filename_len; i++){
            bits_meta_header[++cursor] = (unsigned char)destination_path[i];
          }
          
          //define Source_entity destination_entity and sequence_number
          u_int source_entity_id = (u_int)std::stoul(handle_->local_entity_id_,nullptr,10);
          u_int destination_entity_id = (u_int)std::stoul(args[1],nullptr,10);

          unsigned char bits_fixed_header[9];
          if(value_size == 8){
            bits_fixed_header[0] = 0b00100101;
          } else {
            bits_fixed_header[0] = 0b00100100;
          }
          bits_fixed_header[1] = (pdu_length>>8) & 0xFF;
          bits_fixed_header[2] = pdu_length & 0xFF;
          bits_fixed_header[3] = 0b00010000;
          bits_fixed_header[4] = (source_entity_id>>8) & 0xFF;
          bits_fixed_header[5] = source_entity_id & 0xFF;
          bits_fixed_header[6] = sequence_number & 0xFF;
          bits_fixed_header[7] = (destination_entity_id>>8) & 0xFF;
          bits_fixed_header[8] = destination_entity_id & 0xFF;


          char* pdu = new char[9+pdu_length];
          memcpy(pdu,bits_fixed_header,9);
          memcpy(pdu+9,bits_meta_header,pdu_length);

          //create bundle_spec
          dtn_bundle_spec_t spec;
          std::string source_eid = "ipn:" + handle_->local_entity_id_ + "." + handle_->TRANSMITTER_SERVICE_TAG;
          dtn_parse_eid_string(&spec.source, source_eid.c_str());
          std::string destination_eid = "ipn:" + args[1] + "." + handle_->RECEIVER_SERVICE_TAG;
          dtn_parse_eid_string(&spec.dest, destination_eid.c_str());
          spec.priority = COS_NORMAL;
          spec.expiration = handle_->ttl_;
          spec.bp_version = handle_->bundle_version_;
          //create bundle payload

          //create bundle payload
          handle_->transmit_lock_.lock("Deliver_Me::Process_Primitive::run");
          handle_->transmit_buffer_.push_back({spec,pdu,pdu_length+9});
          handle_->transmit_lock_.unlock();


          // set the file data bit in the fixed header while sending file data
          bits_fixed_header[0] |= 0b00010000;

          int loop=0;
          uint64_t offset = 0;
          u_int checksum = 0;
          storage_.clear();
          storage_.seekg(0);
          while(!storage_.eof() && handle_->file_segment_size_ > 0){
            storage_.read(buffer,handle_->file_segment_size_);

            auto bytes_read = storage_.gcount();
            if (bytes_read == 0){
                if (!storage_.eof()) {
                    // error ...
                    printf("Error reading file - bytes read = %d\n", bytes_read);
                    break;
                }
            }


            //Generate File PDUs
            offset = uint64_t(handle_->file_segment_size_) * uint64_t(loop);

            char* fpdu = new char[bytes_read+9+4];

            memcpy(fpdu,bits_fixed_header,9);
            
            if (value_size == 4) {
                fpdu[9]  = (offset >> 24) & 0xff;
                fpdu[10] = (offset >> 16) & 0xff;
                fpdu[11] = (offset >> 8) & 0xff;
                fpdu[12] = offset & 0xff;
            } else {
                fpdu[9]  = (offset >> 56) & 0xff;
                fpdu[10] = (offset >> 48) & 0xff;
                fpdu[11] = (offset >> 40) & 0xff;
                fpdu[12] = (offset >> 32) & 0xff;
                fpdu[13] = (offset >> 24) & 0xff;
                fpdu[14] = (offset >> 16) & 0xff;
                fpdu[15] = (offset >> 8) & 0xff;
                fpdu[16] = offset & 0xff;
            }

            memcpy(fpdu+(9+value_size),buffer,bytes_read);

            if(handle_->checksum_type_ == 0){
              for(int i=0;i<bytes_read;i++){
                u_int n = 3 - ((offset - bytes_read + i) & 0x03);
                u_int octet_val = buffer[i] << (n << 3);
                checksum += octet_val;
              }
            }


            //create bundle payload
            handle_->transmit_lock_.lock("Deliver_Me::Process_Primitive::run");
            handle_->transmit_buffer_.push_back({spec,fpdu,9+value_size+bytes_read});
            handle_->transmit_lock_.unlock();
            handle_->stats_lock_.lock("Deliver_Me::Process_Primitive::run");
            handle_->stats_buffer_[stats_index].bytes_processed = offset;
            handle_->stats_lock_.unlock();
            loop++;
          }
          storage_.close();
          //Send EOF
          unsigned char bits_eof_header[6+value_size];

          bits_eof_header[0] = 0x04 & 0xFF;
          bits_eof_header[1] = 0x00 & 0xFF;
          bits_eof_header[2] = (checksum>>24) & 0xFF;
          bits_eof_header[3] = (checksum>>16) & 0xFF;
          bits_eof_header[4] = (checksum>>8) & 0xFF;
          bits_eof_header[5] = checksum & 0xFF;
          bits_eof_header[6] = bits_meta_header[2];
          bits_eof_header[7] = bits_meta_header[3];
          bits_eof_header[8] = bits_meta_header[4];
          bits_eof_header[9] = bits_meta_header[5];
          if(value_size == 8){
            bits_eof_header[10] = bits_meta_header[6];
            bits_eof_header[11] = bits_meta_header[7];
            bits_eof_header[12] = bits_meta_header[8];
            bits_eof_header[13] = bits_meta_header[9];
          }

          // clear the file data bit in the fixed header for the EOF File Directive
          bits_fixed_header[0] &= 0b11101111;

          char* eof = new char[15+value_size];
          memcpy(eof,bits_fixed_header,9);
          memcpy(eof+9,bits_eof_header,6+value_size);
          
          handle_->transmit_lock_.lock("Deliver_Me::Process_Primitive::run");
          handle_->transmit_buffer_.push_back({spec,eof,15+value_size});
          handle_->transmit_lock_.unlock();
          handle_->primitive_lock_.lock("Deliver_Me::Process_Primitive::run");
          handle_->primitive_buffer_.pop_front();
          handle_->primitive_lock_.unlock();
          handle_->stats_lock_.lock("Deliver_Me::Process_Primitive::run");
          handle_->stats_buffer_[stats_index].eof_processed = true;
          handle_->stats_lock_.unlock();
          std::string log_b = "Finished sending file: "+args[2];
          handle_->log_lock_.lock("Deliver_Me::Process_Primitive::run");
          handle_->log_buffer_.push_back(log_b);
          handle_->log_lock_.unlock();
          handle_->primitive_lock_.lock("Deliver_Me::Process_Primitive::run");
      } else if(handle_->primitive_buffer_.front()[0] == valid_types[1]){
        handle_->primitive_lock_.unlock();
        handle_->stats_lock_.lock("Deliver_Me::Process_Primitive::run");
        fprintf(stdout,"\nTransaction ID | Total Bytes | Bytes Processed | EOF Processed | Source Entity | Destination Entity | Source Filename | Destination Filename\n");
        fprintf(stdout,"------------------------------------------------------------------------------------------------------\n");
        for(int i=0;i<handle_->stats_buffer_.size();i++){
          fprintf(stdout,"%d  |  %lu  |  %lu  |  %s  |  %s  |  %s  |  %s  |  %s\n",handle_->stats_buffer_[i].transaction_id,handle_->stats_buffer_[i].total_bytes,handle_->stats_buffer_[i].bytes_processed,(handle_->stats_buffer_[i].eof_processed?"true":"false"),handle_->stats_buffer_[i].source_entity.c_str(),handle_->stats_buffer_[i].destination_entity.c_str(),handle_->stats_buffer_[i].source_filename.c_str(),handle_->stats_buffer_[i].destination_filename.c_str());
          fflush(stdout);
        }
        handle_->stats_lock_.unlock();
        handle_->primitive_lock_.lock("Deliver_Me::Process_Primitive::run");
        handle_->primitive_buffer_.pop_front();
        handle_->primitive_lock_.unlock();
        handle_->primitive_lock_.lock("Deliver_Me::Process_Primitive::run");
      }
    }
    handle_->primitive_lock_.unlock();
    usleep(1000);
  }
}

//----------------------------------------------------------------------
void
Deliver_Me::Process_Primitive::put(Put_Request request)
{
}

//----------------------------------------------------------------------
Deliver_Me::Process_PDU::Process_PDU(Deliver_Me *handle)
     : Thread("Deliver_Me::Process_PDU", Thread::DELETE_ON_EXIT),
       handle_(handle)
{

}

//----------------------------------------------------------------------
Deliver_Me::Process_PDU::~Process_PDU()
{
}

//----------------------------------------------------------------------
void
Deliver_Me::Process_PDU::run()
{
  while(!handle_->should_shutdown_){
    handle_->receive_lock_.lock("Deliver_Me::Process_PDU::run");
    if(!handle_->receive_buffer_.empty()){
      dtn_bundle_spec_t spec = handle_->receive_buffer_.front().spec;     
      Fixed_Header header = process_header(handle_->receive_buffer_.front().data,handle_->receive_buffer_.front().length);
   
      if(!header.pdu_type){
        //Process File Directive - Note: documentation calls for hex, but we're dealing with binary so I chose to convert the file directive to numbers instead of heax to save some effort.
        std::bitset<8> directive(handle_->receive_buffer_.front().data[header.length]);
        
        u_int file_directive = directive.to_ulong(); 
        switch(file_directive){
          case 4:
            process_eof(handle_->receive_buffer_.front().data,handle_->receive_buffer_.front().length,header);
            break;
          case 7:
            process_meta(handle_->receive_buffer_.front().data,handle_->receive_buffer_.front().length,header,spec);
            break;
          default:
            //Invalid File Directive
            std::string error = "Error: File Directive does not match any Class 1 directives: "+file_directive;
            handle_->log_lock_.lock("Deliver_Me::Process_PDU::run");
            handle_->log_buffer_.push_back(error);
            handle_->log_lock_.unlock();
            break;
        }
      } else {
        //find associated transaction
        //create pdu_buffer and add to history;
        process_file(handle_->receive_buffer_.front().data,handle_->receive_buffer_.front().length,header);
      }
      handle_->receive_buffer_.pop_front();
      handle_->transaction_lock_.lock("Deliver_Me::Process_PDU::run");
      if(!handle_->transaction_pool_.empty() && !file_buffer_.empty()){
        for(int i=0;i<handle_->transaction_pool_.size();i++){
          for(int l=0;l<file_buffer_.size();l++){
            if(handle_->transaction_pool_[i]->header_.source_entity_id == file_buffer_[l].header.source_entity_id && handle_->transaction_pool_[i]->header_.sequence_number == file_buffer_[l].header.sequence_number){
              handle_->transaction_pool_[i]->process_file(file_buffer_[l].pdu);
              std::deque<Deliver_Me::File_Standby_PDU>::iterator nth = file_buffer_.begin() + l;
              file_buffer_.erase(nth);
              l--;
            }
          }
        }
      } 
      handle_->transaction_lock_.unlock();
    }
    handle_->receive_lock_.unlock();
    usleep(1000);
  }
}

//----------------------------------------------------------------------
Deliver_Me::Fixed_Header
Deliver_Me::Process_PDU::process_header(char* buffer, int length)
{
  Deliver_Me::Fixed_Header header;
  std::string bin_buffer = std::bitset<8>(buffer[0]).to_string()+std::bitset<8>(buffer[1]).to_string()+std::bitset<8>(buffer[2]).to_string()+std::bitset<8>(buffer[3]).to_string();
  std::bitset<32> fixed_header(bin_buffer);
  std::bitset<3>  version;
  std::bitset<16> pdu_data_length;
  std::bitset<3>  entity_id_length;
  std::bitset<3>  sequence_number_length;
  int index = 2;
  for(int i=31;i>28;i--){
    if(fixed_header.test(i)){
      version.set(index);
    }
    index--;
  }
  index = 15;
  for(int i=23;i>7;i--){
    if(fixed_header.test(i)){
      pdu_data_length.set(index);
    }
    index--;
  }
  index = 2;
  for(int i=6;i>3;i--){
    if(fixed_header.test(i)){
      entity_id_length.set(index);
    }
    index--;
  }
  index = 2;
  for(int i=2;i>=0;i--){
    if(fixed_header.test(i)){
      sequence_number_length.set(index);
    }
    index--;
  }
  header.version = version.to_ulong();
  header.pdu_type = fixed_header.test(28);
  header.direction = fixed_header.test(27);
  header.transmission_mode = fixed_header.test(26);
  header.crc_flag = fixed_header.test(25);
  header.large_file_flag = fixed_header.test(24);
  header.pdu_data_length = pdu_data_length.to_ulong();
  header.segmentation_control = fixed_header.test(7);
  header.entity_id_length = entity_id_length.to_ulong();
  header.entity_id_length++;
  header.segment_metadata_flag = fixed_header.test(3);
  header.sequence_number_length = sequence_number_length.to_ulong();
  header.sequence_number_length++;
  u_int id_length = header.entity_id_length;
  u_int sn_length = header.sequence_number_length;
  char* tmp_source_id[id_length];
  char* tmp_destination_id[id_length];
  char* tmp_sn[sn_length];
  index = 4;
  std::string str_sid;
  for(int i=0;i<id_length;i++){
    std::bitset<8> bb(buffer[index+i]);
    str_sid += bb.to_string();
  }
  u_int int_sid= std::stoi(str_sid,nullptr,2);
  index+=id_length;
  std::string str_sn;
  for(int i=0;i<sn_length;i++){
    std::bitset<8> bb(buffer[index+i]);
    str_sn += bb.to_string();
  }
  u_int int_sn= std::stoi(str_sn,nullptr,2);
  index+=sn_length;
  std::string str_did;
  for(int i=0;i<id_length;i++){
    std::bitset<8> bb(buffer[index+i]);
    str_did += bb.to_string();
  }
  u_int int_did= std::stoi(str_did,nullptr,2);
  header.source_entity_id = int_sid;
  header.sequence_number = int_sn;
  header.destination_entity_id = int_did;
  header.length = 4+id_length+id_length+sn_length;

  return header;
}

//----------------------------------------------------------------------
void
Deliver_Me::Process_PDU::process_meta(char* buffer, int length, Fixed_Header header, dtn_bundle_spec_t spec)
{
  
  Deliver_Me::Meta_PDU pdu;
  int processed_offset = header.length;

  std::bitset<8> directive(buffer[processed_offset]);
  pdu.file_directive = directive.to_ulong();
  processed_offset += 1;

  //Extract Segmentation Control Flag - flag is stored in bitset position 7
  std::bitset<8> flags(buffer[processed_offset]);
  pdu.closure_requested = flags.test(6);
  std::bitset<4> checksum;
  if(flags.test(3)){checksum.set(3);}
  if(flags.test(2)){checksum.set(2);}
  if(flags.test(1)){checksum.set(1);}
  if(flags.test(0)){checksum.set(0);}
  pdu.checksum_type = checksum.to_ulong();
  processed_offset += 1;
  
  //Extract Filesize
  std::string str_file_size;
  int value_size = 0;
  if(header.large_file_flag){
    value_size = 8;
  } else {
    value_size = 4;
  }
  for(int i=0;i<value_size;i++){
    std::bitset<8> bb(buffer[processed_offset+i]);
    str_file_size += bb.to_string();
  }
  pdu.file_size = std::stoull(str_file_size,nullptr,2);
  processed_offset += value_size;

  //Extract Source Filename Length
  std::bitset<8> bb_sfl(buffer[processed_offset]);
  pdu.source_filename_length = bb_sfl.to_ulong();
  processed_offset += 1;
  
  //Extract Source Filename
  for(int i=0;i<pdu.source_filename_length;i++){
    pdu.source_filename.push_back(buffer[processed_offset+i]);
  }
  processed_offset += pdu.source_filename_length;

  //Extract Destination Filename Length
  std::bitset<8> bb_dfl(buffer[processed_offset]);
  pdu.destination_filename_length = bb_dfl.to_ulong();
  processed_offset += 1;
  
  //Extract Destination Filename
  for(int i=0;i<pdu.destination_filename_length;i++){
    pdu.destination_filename.push_back(buffer[processed_offset+i]);
  }
  processed_offset += pdu.destination_filename_length;

  //check if at end of packet. if not then process TLV's
  while(processed_offset < length){
    //Extract Type Field
    std::bitset<8> bb_type(buffer[processed_offset]);
    u_int type_field = bb_type.to_ulong();
    processed_offset += 1;

    //Extract Length Field
    std::bitset<8> bb_length(buffer[processed_offset]);
    u_int length_field = bb_length.to_ulong();
    processed_offset += 1;

    //Extract Value Field
    std::string value_field;
    for(int i=0;i<length_field;i++){
      value_field.push_back(buffer[processed_offset+i]);
    }
    pdu.tlv_buffer.push_back({type_field,length_field,value_field});
    processed_offset += length_field;
  }

  handle_->transaction_lock_.lock("Deliver_Me::Process_PDU::process_meta");
  if(!handle_->transaction_pool_.empty()){
    bool found = false;
    for(int i=0;i<handle_->transaction_pool_.size();i++){
      if(handle_->transaction_pool_[i]->header_.source_entity_id == header.source_entity_id && handle_->transaction_pool_[i]->header_.sequence_number == header.sequence_number){
        handle_->transaction_pool_[i]->process_meta(pdu);
        found = true;
        break;
      }
    }
    if(!found){
      handle_->transaction_pool_.push_back(new Deliver_Me::Transaction_Worker(handle_,header,pdu,spec)); 
      handle_->transaction_pool_.back()->start();
    }
  } else {
    //Compile into Transaction and add to buffer
    handle_->transaction_pool_.push_back(new Deliver_Me::Transaction_Worker(handle_,header,pdu,spec)); 
    handle_->transaction_pool_.back()->start();
  }
  handle_->transaction_lock_.unlock();
}

//----------------------------------------------------------------------
void
Deliver_Me::Process_PDU::process_eof(char* buffer, int length, Fixed_Header header)
{
  Deliver_Me::EOF_PDU pdu;
  int processed_offset = header.length;

  std::bitset<8> directive(buffer[processed_offset]);
  pdu.file_directive = directive.to_ulong();
  processed_offset += 1;
  
  std::bitset<4> condition_code(buffer[processed_offset]);
  pdu.condition_code = condition_code.to_ulong();
  processed_offset += 1;

  uint32_t* u32 = (uint32_t*)(buffer + processed_offset);
  pdu.file_checksum = ntohl(*u32);
  processed_offset += 4;

  if(header.large_file_flag){
    uint64_t* u64 = (uint64_t*)(buffer + processed_offset);
    pdu.file_size = be64toh(*u64);
    processed_offset += 8;
  } else {
    u32 = (uint32_t*)(buffer + processed_offset);
    pdu.file_size = ntohl(*u32);
    processed_offset += 4;
  }

  pdu.set = true;

  handle_->transaction_lock_.lock("Deliver_Me::Process_PDU::process_eof");
  if(!handle_->transaction_pool_.empty()){
    bool found = false;
    for(int i=0;i<handle_->transaction_pool_.size();i++){
      if(handle_->transaction_pool_[i]->header_.source_entity_id == header.source_entity_id && handle_->transaction_pool_[i]->header_.sequence_number == header.sequence_number){
        handle_->transaction_pool_[i]->process_eof(pdu);
        found = true;
        break;
      }
    }
    if(!found){
      handle_->log_lock_.lock("Deliver_Me::Process_PDU::process_eof");
      handle_->log_buffer_.push_back("Error: No Transaction found for EOF PDU");
      handle_->log_lock_.unlock();
    }
  }
  handle_->transaction_lock_.unlock();
}

//----------------------------------------------------------------------
void
Deliver_Me::Process_PDU::process_file(char* buffer, int length, Fixed_Header header)
{
  Deliver_Me::File_PDU pdu;
  int processed_offset = header.length;

  if(header.large_file_flag){
    std::bitset<64> segment_offset(buffer[processed_offset]);
    pdu.segment_offset = segment_offset.to_ulong();
    processed_offset += 8;
  } else {
    std::bitset<32> segment_offset(buffer[processed_offset]);
    pdu.segment_offset = segment_offset.to_ulong();
    processed_offset += 4;
  }

  pdu.length = length-processed_offset;
  pdu.data = (char*)malloc(pdu.length);
  for(int i=0;i<pdu.length;i++){
    pdu.data[i] = buffer[processed_offset+i];
  }
  //memcpy(pdu.data,buffer[processed_offset],pdu.length-1);

  handle_->transaction_lock_.lock("Deliver_Me::Process_PDU::process_file");
  if(!handle_->transaction_pool_.empty()){
    bool found = false;
    for(int i=0;i<handle_->transaction_pool_.size();i++){
      if(handle_->transaction_pool_[i]->header_.source_entity_id == header.source_entity_id && handle_->transaction_pool_[i]->header_.sequence_number == header.sequence_number){
        handle_->transaction_pool_[i]->process_file(pdu);
        found = true;
        break;
      }
    }
    if(!found){
      handle_->log_lock_.lock("Deliver_Me::Process_PDU::process_file");
      handle_->log_buffer_.push_back("Error: No Transaction found for File PDU");
      handle_->log_lock_.unlock();
      file_buffer_.push_back({header,pdu});
    }
  } else {
    handle_->log_lock_.lock("Deliver_Me::Process_PDU::process_file");
    handle_->log_buffer_.push_back("Error: No Transaction found for File PDU");
    handle_->log_lock_.unlock();
    file_buffer_.push_back({header,pdu});
  }
  handle_->transaction_lock_.unlock();
}

//----------------------------------------------------------------------
Deliver_Me::Transaction_Worker::Transaction_Worker(Deliver_Me *handle, Fixed_Header header, Meta_PDU pdu, dtn_bundle_spec_t spec)
     : Thread("Deliver_Me::Transaction_Worker"),                    
       handle_(handle),
       header_(header),
       spec_(spec),
       eof_({0,0,0,0,0,false}),
       should_work_(true)
{
  oasys::ScopeLock scoplok(&meta_buffer_lock_, __func__);
  meta_buffer_.push_back(pdu);
}

//----------------------------------------------------------------------
Deliver_Me::Transaction_Worker::~Transaction_Worker()
{
}

//----------------------------------------------------------------------
void
Deliver_Me::Transaction_Worker::run()
{
  int64_t offs = 0;
  int64_t file_size = 0;
  handle_->sequence_lock_.lock("Deliver_Me::Transaction_Worker::run");
  u_int sequence_number = ++handle_->sequence_number_;
  handle_->sequence_lock_.unlock();
  handle_->stats_lock_.lock("Deliver_Me::Transaction_Worker::run");
  handle_->stats_buffer_.push_back({sequence_number,0,0,false,"","","",""});
  int stats_index = handle_->stats_buffer_.size()-1;
  handle_->stats_lock_.unlock();
  while(should_work_){
    should_work_ = !handle_->should_shutdown_;
    std::string source_entity_id = std::to_string(header_.source_entity_id);
    if(source_entity_id != handle_->local_entity_id_){
      if(!file_buffer_.empty() && storage_.is_open()){
          oasys::ScopeLock scoplok(&file_buffer_lock_, __func__);

          storage_.seekp(offs,std::ios_base::beg);
          storage_.write(file_buffer_.front().data,file_buffer_.front().length);
          offs+=file_buffer_.front().length;
          handle_->stats_lock_.lock("Deliver_Me::Transaction_Worker::run");
          handle_->stats_buffer_[stats_index].bytes_processed += file_buffer_.front().length;
          handle_->stats_lock_.unlock();

          free(file_buffer_.front().data);

          file_buffer_.pop_front();
      }
      if(!meta_buffer_.empty()){
        oasys::ScopeLock scoplok(&meta_buffer_lock_, __func__);

        file_size = meta_buffer_.front().file_size;
        if(!storage_.is_open()){
          storage_.open(meta_buffer_.front().destination_filename, std::fstream::out | std::fstream::binary);
          if(!storage_){
            handle_->log_lock_.lock("Deliver_Me::Transaction_Worker::run");
            handle_->log_buffer_.push_back("Error: Could not open File Path");
            handle_->log_lock_.unlock();
          }
          handle_->stats_lock_.lock("Deliver_Me::Transaction_Worker::run");
          handle_->stats_buffer_[stats_index].total_bytes = file_size;
          handle_->stats_buffer_[stats_index].source_entity = std::to_string(header_.source_entity_id);
          handle_->stats_buffer_[stats_index].destination_entity = std::to_string(header_.destination_entity_id);
          handle_->stats_buffer_[stats_index].source_filename = meta_buffer_.front().source_filename;
          handle_->stats_buffer_[stats_index].destination_filename = meta_buffer_.front().destination_filename;
          handle_->stats_lock_.unlock();
          //fill file with fill data
          if(meta_buffer_.front().file_size > 0){
            for(int i=0;i<meta_buffer_.front().file_size;i++){
              //To-Do Change Fill to be configurable
              //create buffer based on the file_segment_size and write to storage
	          const char* fill = "Z";
              storage_.write(fill,1);
            }
          }
          meta_buffer_.pop_front();
        } else {
          
          //Oops Storage was already open???
        }
      } 
      if(eof_.set && handle_->stats_buffer_[stats_index].total_bytes == handle_->stats_buffer_[stats_index].bytes_processed){
        if(check_file()){
          storage_.close();
          should_work_=false;
        }
        handle_->stats_lock_.lock("Deliver_Me::Transaction_Worker::run");
        handle_->stats_buffer_[stats_index].eof_processed = true;
        handle_->stats_lock_.unlock();
        
      }
    }
  }



  //XXX TODO: need to inform the pool that this thread shoudl be deleted

}
//----------------------------------------------------------------------
void
Deliver_Me::Transaction_Worker::process_meta(Deliver_Me::Meta_PDU pdu)
{
  oasys::ScopeLock scoplok(&meta_buffer_lock_, __func__);

  meta_buffer_.push_back(pdu);
}
//----------------------------------------------------------------------
void
Deliver_Me::Transaction_Worker::process_eof(Deliver_Me::EOF_PDU pdu)
{
  eof_ = pdu;
}
//----------------------------------------------------------------------
void
Deliver_Me::Transaction_Worker::process_file(Deliver_Me::File_PDU pdu)
{
  oasys::ScopeLock scoplok(&file_buffer_lock_, __func__);

  if (!should_work_) {
      static bool first_msg = true;
      if (first_msg) {
	      //To-Do change this to log to log_worker
          printf("dzdebug - Transaction_Worker receivng file pdus after shutdown\n");
          first_msg = false;
      }

      return;
  }

  file_buffer_.push_back(pdu);
}

//----------------------------------------------------------------------
bool
Deliver_Me::Transaction_Worker::check_file()
{
  return true;
}
} // namespace deliver_me

int
main(int argc, char** argv)
{
    deliver_me::Deliver_Me::create();
    deliver_me::Deliver_Me::instance()->main(argc, argv);
}
