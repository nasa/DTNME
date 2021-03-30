#ifndef _DELIVER_ME_H_
#define _DELIVER_ME_H_

#include <sys/types.h>
#include <APIBundleQueue.h>

#include <dtn_api.h>
#include <third_party/oasys/util/App.h>
#include <third_party/oasys/util/OptParser.h>
#include <third_party/oasys/thread/MsgQueue.h>
#include <third_party/oasys/util/Singleton.h>
#include <third_party/oasys/thread/Thread.h>
#include <third_party/oasys/thread/Mutex.h>
#include <third_party/oasys/util/ExpandableBuffer.h>
#include <third_party/oasys/util/Time.h>
#include <fstream>
#include <sstream>

namespace deliver_me {

class Deliver_Me : public oasys::App, public oasys::Singleton<Deliver_Me> 
{
public:
    // Constructor
    Deliver_Me();

    struct Bundle {
     dtn_bundle_spec_t      spec;
     char*                  data;
     int                    length;
    };

    struct Fixed_Header {
       u_int                version;
       bool                 pdu_type;
       bool                 direction;
       bool                 transmission_mode;
       bool                 crc_flag;
       bool                 large_file_flag;
       u_int                pdu_data_length;
       bool                 segmentation_control;
       u_int                entity_id_length;
       bool                 segment_metadata_flag;
       u_int                sequence_number_length;
       u_int                source_entity_id;
       u_int                sequence_number;
       u_int                destination_entity_id;
       u_int                length;
    };

    struct TLV {
      u_int                 type;
      u_int                 length;
      std::string           value;
    };

    struct Meta_PDU {
      u_int                 file_directive;
      bool                  reserve_a;
      bool                  closure_requested;
      unsigned short        reserve_b;
      u_int                 checksum_type;
      unsigned long long    file_size;
      u_int                 source_filename_length;
      std::string           source_filename;
      u_int                 destination_filename_length;
      std::string           destination_filename;
      std::deque<TLV>       tlv_buffer;
    };

    struct File_PDU {
      char*                 data;
      int                   length;
      unsigned long long    segment_offset;
    };

    struct File_Standby_PDU {
      struct Fixed_Header          header;
      struct File_PDU              pdu;
    };

    struct EOF_PDU {
      u_int                 file_directive;
      u_int                 condition_code;
      u_int                 spare;
      u_int                 file_checksum;
      unsigned long long    file_size;
      bool                  set;
    };

    struct Primitive {
      std::string           data;  
    };

    struct Put_Request {
      std::string           data;  
    };

    // Main application loop
    int main(int argc, char* argv[]);

    // Virtual from oasys::App
    void fill_options();
    void validate_options(int argc, char* const argv[], int remainder);

    void cli();

protected:

    class Transaction_Worker : public oasys::Thread {
    public:
        // Constructor
        Transaction_Worker(Deliver_Me *handle, Fixed_Header header, Meta_PDU pdu, dtn_bundle_spec_t spec);
 
        // Deconstructor
        ~Transaction_Worker();

        void process_meta(Deliver_Me::Meta_PDU pdu);
        void process_eof(Deliver_Me::EOF_PDU pdu);
        void process_file(Deliver_Me::File_PDU pdu);
        bool check_file();

        bool                    should_work_;
        struct Fixed_Header     header_;
        struct EOF_PDU          eof_;
        std::deque<Meta_PDU>    meta_buffer_;        
        std::deque<File_PDU>    file_buffer_;
        std::deque<std::string> indications_;
        dtn_bundle_spec_t       spec_;
        std::fstream            storage_;
        Deliver_Me*             handle_;
        oasys::SpinLock         meta_buffer_lock_;
        oasys::SpinLock         file_buffer_lock_;

    protected:
        //Main loop
        void run();
    };

    std::string               TRANSMITTER_SERVICE_TAG="64";
    std::string               RECEIVER_SERVICE_TAG="65";

    u_int                    segment_size_;

    std::string              api_address_;
    u_int                    api_port_;
    u_int                    checksum_type_;
    u_int                    bundle_version_;
    std::string              local_entity_id_;
    u_int                    file_segment_size_;
    u_int                    ttl_;
    std::string              log_path_;
    bool                     should_shutdown_;

    std::deque<std::string>  log_buffer_;
    oasys::SpinLock          log_lock_;
    std::deque<std::vector<std::string>>  primitive_buffer_;
    oasys::SpinLock          primitive_lock_;
    std::deque<Bundle>       receive_buffer_;
    oasys::SpinLock          receive_lock_;
    std::deque<Bundle>       transmit_buffer_;
    oasys::SpinLock          transmit_lock_;
    std::deque<Transaction_Worker*>  transaction_pool_;
    oasys::SpinLock                  transaction_lock_;

    void init_threads();

    class Bundle_Receiver : public oasys::Thread {
    public:
        // Constructor
        Bundle_Receiver(Deliver_Me *handle);
 
        // Deconstructor
        ~Bundle_Receiver();

    protected:
        //Main loop
        void run();
 
        u_int32_t          receive_regid_;  
        std::string        log_;    
        dtn_endpoint_id_t  receive_eid_;
        dtn_handle_t       receive_handle_;
        Deliver_Me*        handle_;
    };

    class Bundle_Transmitter : public oasys::Thread {
    public:
        // Constructor
        Bundle_Transmitter(Deliver_Me *handle);
 
        // Deconstructor
        ~Bundle_Transmitter();

    protected:
        //Main loop
        void run();
        
        u_int32_t          transmit_regid_;  
        std::string        log_;    
        dtn_endpoint_id_t  transmit_eid_;
        dtn_handle_t       transmit_handle_;
        Deliver_Me*        handle_;
    };

    class Process_PDU : public oasys::Thread {
    public:
        // Constructor
        Process_PDU(Deliver_Me *handle);
 
        // Deconstructor
        ~Process_PDU();

    protected:
        //Main loop
        void run();
        Deliver_Me::Fixed_Header process_header(char* buffer, int length);
        void process_meta(char* buffer, int length, Fixed_Header header, dtn_bundle_spec_t spec);
        void process_eof(char* buffer, int length, Fixed_Header header);
        void process_file(char* buffer, int length, Fixed_Header header);
        
        std::deque<File_Standby_PDU> file_buffer_;
        std::string        log_;    
        Deliver_Me*        handle_;
    };

    class Process_Primitive : public oasys::Thread {
    public:
        // Constructor
        Process_Primitive(Deliver_Me *handle);
 
        // Deconstructor
        ~Process_Primitive();

    protected:
        //Main loop
        void run();
        void put(Put_Request request);
        void cancel(u_int transaction_id);
        void resume(u_int transaction_id);
        void suspend(u_int transaction_id);
        void report(u_int transaction_id);
        
        u_int              sequence_number_;
        std::string        log_;    
        Deliver_Me*        handle_;
    };

    class Log_Worker : public oasys::Thread {
    public:
        // Constructor
        Log_Worker(Deliver_Me *handle);
 
        // Deconstructor
        ~Log_Worker();

    protected:
        //Main loop
        void run();
        
        Deliver_Me*        handle_;
    };
};

} // namespace deliver_me

#endif /* _DELIVER_ME_H_ */
