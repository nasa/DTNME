/* CGBAConvergenceLayer.cc
 *
 */

//#define HEXD 1

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <sys/poll.h>

#include <oasys/io/NetUtils.h>
#include <oasys/thread/Timer.h>
#include <oasys/util/OptParser.h>
#include <oasys/util/StringBuffer.h>

#include "CGBAConvergenceLayer.h"
#include "pdsmlib/PacketParser.h"
#include "bundling/Bundle.h"
#include "bundling/BundleEvent.h"
#include "bundling/BundleDaemon.h"
#include "bundling/BundleList.h"
#include "bundling/BundleProtocol.h"
#include "bundling/PrimaryBlockProcessor.h"
#include "bundling/SDNV.h"
#include "bundling/Dictionary.h"

//#define TEST_WITH_PIPE 1
//#define USE_PIPE_FOR_PROCESSING 1
//#define DO_CBHE 1   -- supposedly built into DTNME.8 so not needed here???

#if(defined( TEST_WITH_PIPE) || defined (USE_PIPE_FOR_PROCESSING))
#include <channel.h>
static const u_int DEFAULT_CHANNEL = 20;
#endif

namespace dtn {

struct CGBAConvergenceLayer::Params CGBAConvergenceLayer::defaults_;

//----------------------------------------------------------------------
void
CGBAConvergenceLayer::Params::serialize(oasys::SerializeAction *a)
{
    a->process("local_addr", oasys::InAddrPtr(&local_addr_));
    a->process("remote_addr", oasys::InAddrPtr(&remote_addr_));
    a->process("local_port", &local_port_);
    a->process("remote_port", &remote_port_);
    a->process("rate", &rate_);
    a->process("bucket_depth", &bucket_depth_);
}
//----------------------------------------------------------------------
CGBAConvergenceLayer::CGBAConvergenceLayer()
    : IPConvergenceLayer("CGBAConvergenceLayer", "cgba")
{
    defaults_.local_addr_               = INADDR_ANY;
    defaults_.local_port_               = CGBACL_DEFAULT_PORT;
    defaults_.remote_addr_              = INADDR_NONE;
    defaults_.remote_port_              = 0;
    defaults_.rate_                     = 0; // unlimited
    defaults_.bucket_depth_             = 0; // default
/* Was uninitialized.  On 64-bit systems, was coming out NULL anyway.
 * On 32-bit systems, was coming out garbage.
 */
    sender = 0;
}

//----------------------------------------------------------------------
bool
CGBAConvergenceLayer::parse_params(Params* params,
                                  int argc, const char** argv,
                                  const char** invalidp)
{
    oasys::OptParser p;

    p.addopt(new oasys::InAddrOpt("local_addr", &params->local_addr_));
    p.addopt(new oasys::UInt16Opt("local_port", &params->local_port_));
    p.addopt(new oasys::InAddrOpt("remote_addr", &params->remote_addr_));
    p.addopt(new oasys::UInt16Opt("remote_port", &params->remote_port_));
    p.addopt(new oasys::UInt64Opt("rate", &params->rate_));
    p.addopt(new oasys::UInt64Opt("bucket_depth_", &params->bucket_depth_));

    if (! p.parse(argc, argv, invalidp)) {
        return false;
    }

    return true;
};


//----------------------------------------------------------------------
CLInfo*
CGBAConvergenceLayer::new_link_params()
{
    return new CGBAConvergenceLayer::Params(CGBAConvergenceLayer::defaults_);
}

//----------------------------------------------------------------------
bool
CGBAConvergenceLayer::interface_up(Interface* iface,
                                  int argc, const char* argv[])
{

    log_debug("adding interface %s", iface->name().c_str());
//dz debug
    log_crit("adding interface %s", iface->name().c_str());

    // parse options (including overrides for the local_addr and
    // local_port settings from the defaults)
    Params params = CGBAConvergenceLayer::defaults_;
    const char* invalid;
    if (!parse_params(&params, argc, argv, &invalid)) {
        log_err("error parsing interface options: invalid option '%s'",
                invalid);
        return false;
    }

    log_debug(" params.local_addr_ = %s:%d",
        intoa(params.local_addr_),params.local_port_);

    // check that the local interface / port are valid
    if (params.local_addr_ == INADDR_NONE) {
        log_err("invalid local address setting of 0");
        return false;
    }

    if (params.local_port_ == 0) {
        log_err("invalid local port setting of 0");
        return false;
    }

#if(defined( TEST_WITH_PIPE) || defined(USE_PIPE_FOR_PROCESSING))
    channel_set_server_hostname("localhost");
    //channel_set_server_port();
#endif

    // create a new server socket for the requested interface
    Receiver* receiver = new Receiver(&params);
    receiver->logpathf("%s/iface/%s", logpath_, iface->name().c_str());
    
    if (receiver->bind(params.local_addr_, params.local_port_) != 0) {
        return false; // error log already emitted
    }

    // check if the user specified a remote addr/port to connect to
    if (params.remote_addr_ != INADDR_NONE) {
        if (receiver->connect(params.remote_addr_, params.remote_port_) != 0) {
            return false; // error log already emitted
        }
    }

    // start the thread which automatically listens for data
    //dzdebug receiver->start();
    
    // store the new listener object in the cl specific portion of the
    // interface
    iface->set_cl_info(receiver);
    
    
    return true;
}

//----------------------------------------------------------------------
void
CGBAConvergenceLayer::interface_activate(Interface* iface)
{
    Receiver* receiver = (Receiver*)iface->cl_info();
    receiver->start();
}

//----------------------------------------------------------------------
bool
CGBAConvergenceLayer::interface_down(Interface* iface)
{
    // grab the listener object, set a flag for the thread to stop and
    // then close the socket out from under it, which should cause the
    // thread to break out of the blocking call to accept() and
    // terminate itself
    Receiver* receiver = (Receiver*)iface->cl_info();
    receiver->set_should_stop();
    //receiver->interrupt_from_io();
    
    while (! receiver->is_stopped()) {
        oasys::Thread::yield();
    }

    delete receiver;
    return true;
}

//----------------------------------------------------------------------
void
CGBAConvergenceLayer::dump_interface(Interface* iface,
                                    oasys::StringBuffer* buf)
{
    Params* params = &((Receiver*)iface->cl_info())->params_;
    
    buf->appendf("\tlocal_addr: %s local_port: %d\n",
                 intoa(params->local_addr_), params->local_port_);
    
    if (params->remote_addr_ != INADDR_NONE) {
        buf->appendf("\tconnected remote_addr: %s remote_port: %d\n",
                     intoa(params->remote_addr_), params->remote_port_);
    } else {
        buf->appendf("\tnot connected\n");
    }
}

//----------------------------------------------------------------------
bool
CGBAConvergenceLayer::init_link(const LinkRef& link,
                                int argc, const char* argv[])
{
    in_addr_t addr;
    u_int16_t port = 0;

    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() == NULL);

    log_debug("adding %s link %s", link->type_str(), link->nexthop());

    // Parse the nexthop address but don't bail if the parsing fails,
    // since the remote host may not be resolvable at initialization
    // time and we retry in open_contact
    parse_nexthop(link->nexthop(), &addr, &port);

    // Create a new parameters structure, parse the options, and store
    // them in the link's cl info slot
    Params* params = new Params(defaults_);
    params->local_addr_ = INADDR_NONE;
    params->local_port_ = 0;

    const char* invalid;
    if (! parse_params(params, argc, argv, &invalid)) {
        log_err("error parsing link options: invalid option '%s'", invalid);
        delete params;
        return false;
    }

    if (link->params().mtu_ > MAX_BUNDLE_LEN) {
        log_err("error parsing link options: mtu %d > maximum %d",
                link->params().mtu_, MAX_BUNDLE_LEN);
        delete params;
        return false;
    }

    link->set_cl_info(params);


    return true;
}

//----------------------------------------------------------------------
void
CGBAConvergenceLayer::delete_link(const LinkRef& link)
{
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() != NULL);

    log_debug("CGBAConvergenceLayer::delete_link: "
              "deleting link %s", link->name());

    delete link->cl_info();
    link->set_cl_info(NULL);
}

//----------------------------------------------------------------------
void
CGBAConvergenceLayer::dump_link(const LinkRef& link, oasys::StringBuffer* buf)
{
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() != NULL);
        
    Params* params = (Params*)link->cl_info();
    
    buf->appendf("\tlocal_addr: %s local_port: %d\n",
                 intoa(params->local_addr_), params->local_port_);

    buf->appendf("\tremote_addr: %s remote_port: %d\n",
                 intoa(params->remote_addr_), params->remote_port_);
}

//----------------------------------------------------------------------
bool
CGBAConvergenceLayer::open_contact(const ContactRef& contact)
{
    LinkRef link = contact->link();
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());

/* Section removed to try to fix a crash-upon-shutdown problem.
    if(sender) {
        //log_err("Already a CGBA contact open.");
        return true;
    }

    sender = new Sender(link->contact());
    contact->set_cl_info(sender);
    BundleDaemon::post(new ContactUpEvent(link->contact()));

    log_debug("CGBAConvergenceLayer::open_contact: "
              "opening contact for link *%p", link.object());
*/

    return true;
}

//----------------------------------------------------------------------
bool
CGBAConvergenceLayer::close_contact(const ContactRef& contact)
{
    log_info("close_contact *%p", contact.object());

    return true;
}
//----------------------------------------------------------------------
void
CGBAConvergenceLayer::bundle_queued(const LinkRef& link, const BundleRef& bref)
{
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    
    const ContactRef contact = link->contact();
    Sender* sender = (Sender*)contact->cl_info();
    if (!sender) {
        log_crit("send_bundles called on contact *%p with no Sender!!",
                 contact.object());
        return;
    }
    ASSERT(contact == sender->contact_);

    int len = sender->send_bundle(bref);

    if (len > 0) {
        link->del_from_queue(bref, len);
        link->add_to_inflight(bref, len);
        BundleDaemon::post(
            new BundleTransmittedEvent(bref.object(), contact, link, len, 0));
    }
}

//----------------------------------------------------------------------
CGBAConvergenceLayer::Receiver::Receiver(CGBAConvergenceLayer::Params* params)
    : IOHandlerBase(new oasys::Notifier("/dtn/cl/cgba/receiver")),
      UDPClient("/dtn/cl/cgba/receiver"),
      Thread("CGBAConvergenceLayer::Receiver")
{
    logfd_  = false;
    params_ = *params;

    m_data_frame_ptr = NULL;
    m_data_frame_len = 0; 
    m_remaining = 0;  
    m_chan_pkt_len = 0; 
    m_bytes_saved = 0;
    m_last_ric_count = -1;
    m_ric_count = -1;
    m_save_buffer = NULL;
    m_save_packet_buffer = NULL;
}

//----------------------------------------------------------------------
// find a channel header that preceeds a DTN bundle block
// This takes into account for possible embedded byte escaped 0x7D and 
// 0x7E in the crc and packet length fields of the channel packet header
// Optionally check if the DTN bundle block is a Primary block or a 
// Payload block.
// If found, returns 1 else 0
int CGBAConvergenceLayer::Receiver::Find_Channel_Hdr(
  unsigned char *pptr,            //pointer into packet where 0x7E was located
  unsigned char **data_frame_ptr, //returned location for start of bundle block
  int *chan_pkt_len,     //returned length of channel packet
  unsigned short *crc,            //crc from channel packet header
  int check_for_prime             //set when looking for a primary bundle block
) 
{
  // if the crc is byte escaped, then allow for the extra
  // byte when looking for the channel number
  if ((pptr[1] == 0x7D || pptr[2] == 0x7D) && pptr[4] == CGBA_CHANNEL) 
  {
    // save the crc
    if (pptr[1] == 0x7D && pptr[2] == 0x5E)
      *crc = 0x7E + (pptr[3] << 8);
    else if (pptr[1] == 0x7D && pptr[2] == 0x5D)
      *crc = 0x7D + (pptr[3] << 8);
    else if (pptr[2] == 0x7D && pptr[3] == 0x5E)
      *crc = pptr[1] + (0x7E << 8);
    else if (pptr[2] == 0x7D && pptr[3] == 0x5D)
      *crc = pptr[1] + (0x7D << 8);  

    //now look for the DTN version number that marks the start
    //of the primary bundle, but allow for a byte escaped 
    //packet length
    if ((pptr[5] == 0x7D || pptr[6] == 0x7D) && 
        (!check_for_prime || pptr[8] == BundleProtocol::CURRENT_VERSION))
    {
      //found a primary bundle, so get the start address and the 
      //length
      *data_frame_ptr = &pptr[8];
      if(pptr[5] == 0x7D && pptr[6] == 0x5E)
        *chan_pkt_len = 0x7E + (pptr[7] << 8);
      else if (pptr[5] == 0x7D && pptr[6] == 0x5D)
        *chan_pkt_len = 0x7D + (pptr[7] << 8);
      else if(pptr[6] == 0x7D && pptr[7] == 0x5E)
  *chan_pkt_len = pptr[5] + (0x7E << 8);
      else if (pptr[6] == 0x7D && pptr[7] == 0x5D)
        *chan_pkt_len = pptr[5] + (0x7D << 8);

      log_debug(" -F1- %x %x %x %x %x %x %x",pptr[0],pptr[1],pptr[2],pptr[3],pptr[4],pptr[5],pptr[6]);

      return 1;
    }
    // else the length was not byte escaped
    else if (!check_for_prime || pptr[7] == BundleProtocol::CURRENT_VERSION)  
    {
      //found a primary bundle, so get the start address, the 
      //length, and start the crc computation
      *data_frame_ptr = &pptr[7];
      *chan_pkt_len = pptr[5] + (pptr[6] << 8);

    log_debug("-F2- %x %x %x %x %x %x %x",pptr[0],pptr[1],pptr[2],pptr[3],pptr[4],pptr[5],pptr[6]);

      return 1;
    }
  }
  // the channel number should be where it was expected and so should the crc
  else if (pptr[3] == CGBA_CHANNEL)
  {
    *crc = pptr[1] + (pptr[2] << 8);

    //now look for the DTN version number that marks the start
    //of the primary bundle, but allow for a byte escaped 
    //packet length
    if ((pptr[4] == 0x7D || pptr[5] == 0x7D) && 
        (!check_for_prime || pptr[7] == BundleProtocol::CURRENT_VERSION))
    {

      //found a primary bundle, so get the start address and the 
      //length
      *data_frame_ptr = &pptr[7];
      if(pptr[4] == 0x7D && pptr[5] == 0x5E)
  *chan_pkt_len = 0x7E + (pptr[6] << 8);
      else if (pptr[4] == 0x7D && pptr[5] == 0x5D)
        *chan_pkt_len = 0x7D + (pptr[6] << 8);
      else if(pptr[5] == 0x7D && pptr[6] == 0x5E)
  *chan_pkt_len = pptr[4] + (0x7E << 8);
      else if (pptr[5] == 0x7D && pptr[6] == 0x5D)
        *chan_pkt_len = pptr[4] + (0x7D << 8);
      *data_frame_ptr = &pptr[7];

      log_debug(" -F3- %x %x %x %x %x %x %x len = %d",pptr[0],pptr[1],pptr[2],pptr[3],pptr[4],pptr[5],pptr[6],*chan_pkt_len);

      return 1;
    }
    // else the length was not byte escaped
    else if (pptr[6] == BundleProtocol::CURRENT_VERSION || check_for_prime == 0)
    {
      *data_frame_ptr = &pptr[6];
      *chan_pkt_len = pptr[4] + (pptr[5] << 8);

      log_debug(" -F4- %x %x %x %x %x %x %x len = %d",pptr[0],pptr[1],pptr[2],pptr[3],pptr[4],pptr[5],pptr[6],*chan_pkt_len);

      return 1;
    }
    else
    {

    log_debug(" -F5- %x %x %x %x %x %x %x len = %d",pptr[0],pptr[1],pptr[2],pptr[3],pptr[4],pptr[5],pptr[6],*chan_pkt_len);

    }
  }
  return 0;
}

//----------------------------------------------------------------------
// Remove the byte escaped values in the channel packet replacing them
// with the correct 0x7D or 0x7E values. 
// Returns the number of bytes remaining in the buffer after the 
// end of the channel packet and after bytes used for escaped bytes. 
// A negative number indicates not all the escaped bytes could be restored
int CGBAConvergenceLayer::Receiver::Remove_Escaped_Bytes(
  unsigned char *pptr,             //pointer to top of channel packet
  unsigned int chan_pkt_len,       //length of the channel packet
  unsigned int remaining,          //remaining bytes in buffer
  unsigned int *escaped_bytes_used //returns number of escaped bytes
) 
{
unsigned char *eptr;
int bytes_left_in_chan_pkt = 0;
unsigned char *last_ptr = 0;
int bytes_left_in_buffer = 0;

  eptr = pptr; //point to beginning of channel packet 
  last_ptr = pptr + remaining - 1;  // get pointer to last byte available in data block
  bytes_left_in_chan_pkt = chan_pkt_len;
  bytes_left_in_buffer = remaining;
  *escaped_bytes_used = 0;

  log_debug("rem - bytes_left_in_chan_pkt = %d, bytes_left_in_buffer = %d",
   bytes_left_in_chan_pkt,bytes_left_in_buffer);

  // keep searching until we run out of bytes in the EHS packet or
  // there are no more escaped bytes or 
  // we reached the end of the channel packet
  while( bytes_left_in_buffer >= 0 &&
         bytes_left_in_chan_pkt > 0 &&
         (eptr = (unsigned char *)memchr(eptr,0x7D,
              bytes_left_in_chan_pkt)) != 0)
  {

    log_debug(" rem - [%x] bytes_left_in_chan_pkt = %d, bytes_left_in_buffer = %d, offset = %ld",
   eptr[1],bytes_left_in_chan_pkt,bytes_left_in_buffer, eptr-pptr);

    // if a 0x7D is encountered with no more bytes to copy from,
    // then all that can be restored has been restored
    if (bytes_left_in_buffer == 0)
      break;
    *escaped_bytes_used = *escaped_bytes_used+1; // count the escaped bytes
    if (eptr[1] == 0x5E)  // replace the 0x7d with 0x7E
       eptr[0] = 0x7E;
    // shift the remainder of the EHS packet forward one byte
    // over the escaped byte
    memmove(&eptr[1],&eptr[2], last_ptr - &eptr[2] + 1);
    // determine number of bytes left to search in channel pkt
    //    bytes_left_in_chan_pkt = chan_pkt_len + *escaped_bytes_used - 
    bytes_left_in_chan_pkt = chan_pkt_len - 
      (eptr - pptr + 1);   
    eptr = eptr + 1;
    bytes_left_in_buffer = remaining - *escaped_bytes_used - chan_pkt_len;

  }

  log_debug(" escaped bytes removed = %d",*escaped_bytes_used);

  return(bytes_left_in_buffer);
}

//----------------------------------------------------------------------
// Count the byte escaped values in the channel packet to determine if 
// the channel packet is not yet complete due to escaped bytes that have
// to be removed. Returns a -1 if there are insufficient bytes to have
// a complete channel packet in the current EHS packet buffer.
int CGBAConvergenceLayer::Receiver::Check_Escaped_Bytes(
  unsigned char *pptr,             //pointer to top of channel packet
  unsigned int chan_pkt_len,       //length of the channel packet
  unsigned int remaining           //remaining bytes in EHS packet
) 
{
unsigned char *eptr;
int bytes_left_in_chan_pkt = 0;
int bytes_left_in_ehs_pkt = 0;
unsigned int escaped_bytes = 0;

  eptr = pptr; //point to beginning of channel packet 
  bytes_left_in_chan_pkt = chan_pkt_len;
  bytes_left_in_ehs_pkt = remaining;

  log_debug("rem - bytes_left_in_chan_pkt = %d, bytes_left_in_ehs_pkt = %d",
   bytes_left_in_chan_pkt,bytes_left_in_ehs_pkt);

  // keep searching until we run out of bytes in the EHS packet or
  // there are no more escaped bytes or 
  // we reached the end of the channel packet
  while( bytes_left_in_ehs_pkt >= 0 &&
         bytes_left_in_chan_pkt > 0 &&
         (eptr = (unsigned char *)memchr(eptr,0x7D,
              bytes_left_in_chan_pkt+escaped_bytes)) != 0)
  {

    log_debug(" rem - [%x] bytes_left_in_chan_pkt = %d, bytes_left_in_ehs_pkt = %d, offset = %ld",
   eptr[1],bytes_left_in_chan_pkt,bytes_left_in_ehs_pkt, eptr-pptr);

    // if a 0x7D is encountered with no more bytes to copy from,
    // then all that can be counted have been counted
    //if (bytes_left_in_ehs_pkt < 0)
    //  break;
    escaped_bytes = escaped_bytes+1; // count the escaped bytes
    eptr = eptr + 2;                 
    // determine number of bytes left to search in channel pkt
    //bytes_left_in_chan_pkt = chan_pkt_len + escaped_bytes - 
    bytes_left_in_chan_pkt = chan_pkt_len - 
      (eptr - pptr + 1);   
    bytes_left_in_ehs_pkt = remaining - escaped_bytes - chan_pkt_len;

  }

  log_debug(" escaped bytes found = %d",escaped_bytes);

  return(bytes_left_in_ehs_pkt);

}


//----------------------------------------------------------------------
// Do a hexdump of data from packets for debug
void CGBAConvergenceLayer::Receiver::hexdump(
    char title[],  void *start_addr,
    int length,  int no_char 
)
{
    unsigned char *ptr, *end_addr;
    char line[160];
    char value[10];
    int i;

    if ( length == 0) return;

    /* Compute ending address and print dump title */
    if (title)
  log_debug("#** Hex dump of %s:",title);
    end_addr = (unsigned char *)start_addr + length;

    line[0] = '\0';
    /* Loop and print every no_char bytes */
    for (ptr = (unsigned char *)start_addr; 
             ptr < end_addr; 
             ptr +=no_char)
    {
  if ((ptr + no_char) >= end_addr)
        no_char = end_addr - ptr;
  /* print the hex part */
  strcat(line,"  ");
  for (i = 0; i < no_char; i++)
  { 
    sprintf(value,"%4.2x", *(ptr + i ));  
          strcat(line,value);
        }

        /* output the line */
  log_debug("%s",line);
  /* terminate the line */
  line[0] = '\0';
    }

    return;
}

//----------------------------------------------------------------------
//Take apart the primary block to determine if there is any CBHE
//compression. The dictionary length will be zero if there is and will
//be non-zero if there is not. If there is CBHE compression the primary
//block it will be modified to include the expanded EIDs and anything else 
//in the buffer will be shifted down. The number of bytes added to the 
//primary block is returned as a positive integer if there was CBHE 
//decompressed. If there was no CBHE decompression then a zero value
//is returned and the buffer was unmodified. 
//If there is an error, a negative error code value will be returned. 
int CGBAConvergenceLayer::Receiver::uncompress_cbhe(
  u_char*         buffer, //buffer containing all blocks to be forwarded
        size_t          buflen  //length of buffer 
)
{
    PrimaryBlock_t pblock; //used to build new primary block 
    int eplen = 0;  //length of existing primary block determined as it is
                    //read in
    Dictionary dict; //used to buid new dictionary
    u_char temp[1000]; // temporary buffer to build new primary block before
                       // replacing the existing primary block with it.    
    u_int64_t frag_offset = 0;
    u_int64_t frag_length = 0;
    u_char *buf = buffer;

    memset(&pblock, 0, sizeof(PrimaryBlock_t));
    
    //read the DTN version number and verify it is the same as the current
    //before continuing
    pblock.version = *(u_int8_t*)buf;
    buf += 1;
    eplen++;
    if (pblock.version != BundleProtocol::CURRENT_VERSION) 
    {
      log_err("Not correct DTN Version  %d  %d",pblock.version,
          BundleProtocol::CURRENT_VERSION);
        return -10;
    }
    
    // create a local macro to uncompress the SDNV values and
    // accumulate the length of the primary block
#define PBP_READ_SDNV(location) { \
    int sdnv_len = SDNV::decode(buf, buflen, location); \
    if (sdnv_len < 0) \
      return(-15);  \
    buf += sdnv_len; \
    eplen += sdnv_len;}
    
    // Grab the SDNVs representing the flags and the block length,
    // storring them in the temporary primary block.
    PBP_READ_SDNV(&pblock.processing_flags);
    PBP_READ_SDNV(&pblock.block_length);

    // Read the various SDNVs up to the start of the dictionary.
    PBP_READ_SDNV(&pblock.dest_scheme_offset);
    PBP_READ_SDNV(&pblock.dest_ssp_offset);
    PBP_READ_SDNV(&pblock.source_scheme_offset);
    PBP_READ_SDNV(&pblock.source_ssp_offset);
    PBP_READ_SDNV(&pblock.replyto_scheme_offset);
    PBP_READ_SDNV(&pblock.replyto_ssp_offset);
    PBP_READ_SDNV(&pblock.custodian_scheme_offset);
    PBP_READ_SDNV(&pblock.custodian_ssp_offset);
    PBP_READ_SDNV(&pblock.creation_time);
    PBP_READ_SDNV(&pblock.creation_sequence);
    PBP_READ_SDNV(&pblock.lifetime);
    PBP_READ_SDNV(&pblock.dictionary_length);

    // If the bundle is a fragment, grab the fragment offset and original
    // bundle size.
    if (pblock.processing_flags & CGBA_BUNDLE_IS_FRAGMENT) {
        PBP_READ_SDNV(&frag_offset);
        PBP_READ_SDNV(&frag_length);
    }

    //log_debug("CBHE decompress: block length = %lld, dsc = %lld, dssp = %lld, ssc = %lld, sssp = %lld, rsc = %lld, rssp = %lld, csc = %lld, cssp = %lld, ct = %lld,cs = %lld, life=%lld, dic len = %lld",
    //pblock.block_length,pblock.dest_scheme_offset,pblock.dest_ssp_offset,pblock.source_scheme_offset,pblock.source_ssp_offset,pblock.replyto_scheme_offset,pblock.replyto_ssp_offset,pblock.custodian_scheme_offset,pblock.custodian_ssp_offset,pblock.creation_time,pblock.creation_sequence,pblock.lifetime,pblock.dictionary_length);

    char eid[128];
    EndpointID source_eid;
    EndpointID dest_eid;
    EndpointID replyto_eid;
    EndpointID custodian_eid;

    //log_debug("CBHE decompress: dictionary length = %lld",pblock.dictionary_length);

    /*
     * If the dictionary is a 0 length, then assume cbhe compression
     * of the dictionary values exists and keep going. Otherwise just
     * exit here and return the length of the existing buffer without
     * modification
     */
    if (pblock.dictionary_length == 0)
    {
      // create destination EID
      sprintf(eid,"ipn:%lu.%lu",
       pblock.dest_scheme_offset,pblock.dest_ssp_offset);
      dest_eid = EndpointID(eid);
      dict.add_eid(dest_eid);

      //log_debug("CBHE decompress : dest = [%llu . %llu ]  [%s]",pblock.dest_scheme_offset,pblock.dest_ssp_offset,eid);

      // create source EID
      sprintf(eid,"ipn:%lu.%lu",
    pblock.source_scheme_offset,pblock.source_ssp_offset);
      source_eid = EndpointID(eid);
      dict.add_eid(source_eid);

      //log_debug("CBHE decompress: source = [%llu . %llu ] [%s]",pblock.source_scheme_offset,pblock.source_ssp_offset,eid);

      // create reply to EID
      sprintf(eid,"ipn:%lu.%lu",
        pblock.replyto_scheme_offset,pblock.replyto_ssp_offset);
      replyto_eid = EndpointID(eid);
      dict.add_eid(replyto_eid);

      //log_debug("CBHE decompress: reply = [ %llu . %llu ] [%s]",pblock.replyto_scheme_offset,pblock.replyto_ssp_offset,eid);

      // create custodian EID
      sprintf(eid,"ipn:%lu.%lu",
        pblock.custodian_scheme_offset,pblock.custodian_ssp_offset);
      custodian_eid = EndpointID(eid);
      dict.add_eid(custodian_eid);

      //log_debug("CBHE decompress: cust = [ %llu . %llu ] [%s]",pblock.custodian_scheme_offset,pblock.custodian_ssp_offset,eid);

      // update the new dictionary length
      pblock.dictionary_length = dict.length();
    }
    else
    {
      // just return the length of the existing buffer
      //dz - bad!!!  return(buflen);
      return 0;
    }

#undef PBP_READ_SDNV
  
    //set the dictionary offsets for the newly created EIDs that were 
    //previously the node.service values

    dict.get_offset(dest_eid.scheme_str(),
        &pblock.dest_scheme_offset);
    dict.get_offset(dest_eid.ssp(),
                    &pblock.dest_ssp_offset);
    dict.get_offset(source_eid.scheme_str(),
                    &pblock.source_scheme_offset);
    dict.get_offset(source_eid.ssp(),
                    &pblock.source_ssp_offset);
    dict.get_offset(replyto_eid.scheme_str(),
                    &pblock.replyto_scheme_offset);
    dict.get_offset(replyto_eid.ssp(),
                    &pblock.replyto_ssp_offset);
    dict.get_offset(custodian_eid.scheme_str(),
        &pblock.custodian_scheme_offset);
    dict.get_offset(custodian_eid.ssp(),
                    &pblock.custodian_ssp_offset);

    // Create a new Primary block that includes the new dictionary
    // and return its length

    int newplen = generate_primary(&dict,frag_offset,frag_length,&pblock,temp);
#ifdef HEXD
    hexdump(" CBHE decompress: new primary block",temp,newplen,20);
    hexdump("CBHE decompress: old blocks ",buffer,buflen,20);
#endif
    // shift the buffer down by the difference between the existing
    // primary block and the newly created one

    memmove(buffer+newplen,buffer+eplen,buflen-eplen);   

    //move the new primary block into place
    
    memcpy(buffer,temp,newplen);
#ifdef HEXD
    hexdump("CBHE decompress: new blocks ",buffer,buflen+newplen-eplen,20);
#endif

    return newplen-eplen;

}

//----------------------------------------------------------------------
int CGBAConvergenceLayer::Receiver::generate_primary(Dictionary *dict,
                                        u_int64_t frag_offset,
          u_int64_t frag_length,
          PrimaryBlock_t *primary,
          u_char *buffer)
{
    /*
     * Advance buf and increment len as we go through the process.
     */
    u_char* buf = buffer;
    int     len = 0;
    
    // Stick the version number in the first byte.
    *buf = BundleProtocol::CURRENT_VERSION;
    ++buf;
    ++len;
    
#define PBP_WRITE_SDNV(value) { \
    int sdnv_len = SDNV::encode(value, buf, 1000); \
    ASSERT(sdnv_len > 0); \
    buf += sdnv_len; \
    len += sdnv_len; }

    primary->block_length = 0;  // initialize for a single byte
    
    // Write out all of the SDNVs
    PBP_WRITE_SDNV(primary->processing_flags);
    PBP_WRITE_SDNV(primary->block_length); // assume 1 byte is enough initially
    len = 0;  //only count length after block length field

    PBP_WRITE_SDNV(primary->dest_scheme_offset);
    PBP_WRITE_SDNV(primary->dest_ssp_offset);
    PBP_WRITE_SDNV(primary->source_scheme_offset);
    PBP_WRITE_SDNV(primary->source_ssp_offset);
    PBP_WRITE_SDNV(primary->replyto_scheme_offset);
    PBP_WRITE_SDNV(primary->replyto_ssp_offset);
    PBP_WRITE_SDNV(primary->custodian_scheme_offset);
    PBP_WRITE_SDNV(primary->custodian_ssp_offset);
    PBP_WRITE_SDNV(primary->creation_time);
    PBP_WRITE_SDNV(primary->creation_sequence);
    PBP_WRITE_SDNV(primary->lifetime);
    PBP_WRITE_SDNV(primary->dictionary_length);

    // Add the dictionary.
    memcpy(buf, dict->dict(), dict->length());
    buf += dict->length();
    len += dict->length();
    
    /*
     * If the bundle is a fragment, stuff in SDNVs for the fragment
     * offset and original length.
     */
    if (frag_offset != 0) {
        PBP_WRITE_SDNV(frag_offset);
        PBP_WRITE_SDNV(frag_length);
    }
    

    /* 
     * we now have the block length assuming 1 byte was enough
     */
   
    // if length requires 2 bytes, adjust
    if (len > 0x7F)
      len++;

    primary->block_length = len;  // initialize for a single byte

    // now redo with correct length
    buf = buffer;
    len = 0;
    // Stick the version number in the first byte.
    *buf = BundleProtocol::CURRENT_VERSION;
    ++buf;
    ++len;

    // Write out all of the SDNVs a second time, with the correct block length
    PBP_WRITE_SDNV(primary->processing_flags);
    PBP_WRITE_SDNV(primary->block_length);  // now contains actual length
    PBP_WRITE_SDNV(primary->dest_scheme_offset);
    PBP_WRITE_SDNV(primary->dest_ssp_offset);
    PBP_WRITE_SDNV(primary->source_scheme_offset);
    PBP_WRITE_SDNV(primary->source_ssp_offset);
    PBP_WRITE_SDNV(primary->replyto_scheme_offset);
    PBP_WRITE_SDNV(primary->replyto_ssp_offset);
    PBP_WRITE_SDNV(primary->custodian_scheme_offset);
    PBP_WRITE_SDNV(primary->custodian_ssp_offset);
    PBP_WRITE_SDNV(primary->creation_time);
    PBP_WRITE_SDNV(primary->creation_sequence);
    PBP_WRITE_SDNV(primary->lifetime);
    PBP_WRITE_SDNV(primary->dictionary_length);

    // Add the dictionary.
    memcpy(buf, dict->dict(), dict->length());
    buf += dict->length();
    len += dict->length();
    
    /*
     * If the bundle is a fragment, stuff in SDNVs for the fragment
     * offset and original length.
     */
    if (frag_offset != 0) {
        PBP_WRITE_SDNV(frag_offset);
        PBP_WRITE_SDNV(frag_length);
    }




#undef PBP_WRITE_SDNV
    
    return(len);

}


//-----------------------------------------------------------------------
// Look for channel packet in the EHS packet and check for one from 
// the channel for the CGBA payloads. If it is, get the complete channel
// packet and then process the bundles found within this channel packet
// by sending them on to the DTN BP. 
void
CGBAConvergenceLayer::Receiver::Extract_Process_Data(u_char* packet_buffer, 
    size_t len)
{

  PacketParser pp;
      // parses through EHS packets
  int post_process_m_remaining_bytes = 0;
  unsigned int escaped_bytes = 0;
  unsigned char *cptr;
  unsigned short saved_crc;

#ifdef HEXD
hexdump("======EHS PACKET======",packet_buffer,len,20);
#endif

    // initialize a packet parser for current EHS packet that maybe contains
    // a bioserve channel packet
    pp.setframeptr ( packet_buffer, len );

    // point to start of ccsds data and its size
    if ((m_data_frame_ptr = pp.getdataptr()) == NULL)
    {
        free(packet_buffer); //dz
        log_err("CGBAConvergenceLayer::Receiver - error parsing EHS packet: could not locate start of CCSDS");
        return;
    } 

    // save last RIC header counter and get the RIC header counter for this 
    // new packet

    m_last_ric_count = m_ric_count;
    m_ric_count = (m_data_frame_ptr[10] * 256) + m_data_frame_ptr[11];

    // if we were in the middle of collecting a channel packet and expected
    // this packet to continue it and there was a RIC header counter dropout,
    // then drop the partial channel packet and just process this packet as
    // if we received nothing yet.

    if (CGBAConvergenceLayer::Receiver::m_estate == ES_STATE_PPRIME &&
        m_last_ric_count > -1 )
    {
      int diff = m_ric_count - m_last_ric_count;
      if (diff < 0)
  diff += 0x10000;
      //log_debug("last = %d  this = %d",m_last_ric_count,m_ric_count);
      if(diff > 2)
      {
        CGBAConvergenceLayer::Receiver::m_estate = ES_STATE_PRIME;
        //since there may be the beginning of a new channel packet in the
        //most recent EHS packet received, move it to the top of the buffer
        //and just drop all the other packet data that was accumulated before
        //it
        if (m_bytes_saved != 0) {
          //dz delete the previsouly saved buffer!!
          free(m_save_packet_buffer);
        }
        m_save_packet_buffer = NULL;
        m_bytes_saved = 0;
        m_remaining = 0;
        m_estate = ES_STATE_PRIME;
        log_err("**DATA LOST** The RIC header counter indicated part of a channel packet was lost (ctr=%d prev=%d)", 
                m_ric_count, m_last_ric_count);
      }
    }

    // move to location of the payload data which is just after the RIC
    // header 
    m_data_frame_ptr = m_data_frame_ptr + 16;

    // get m_remaining number of data bytes in this EHS packet
    m_data_frame_len = pp.getdatasize() - 16; 

    // also don't use the RIC checkword at the end of the packet
    m_data_frame_len = m_data_frame_len - 2;    

    unsigned char *last_ptr;

    log_debug("There are %d bytes in RIC packet",m_data_frame_len);

    //dz debug
    if ( m_data_frame_len <= 0 )
    {
      free(packet_buffer); //dz
      log_debug("Received small packet that cannot have a RIC Header");
      return;
    }

    // If there are any bytes left over from processing a previous
    // EHS packet that contained a channel packet that was larger than
    // the EHS packet, add these new bytes to the end of the previously
    // read in bytes

    if (m_bytes_saved != 0)
    {
        last_ptr = m_save_buffer + m_bytes_saved;
        memcpy(last_ptr,m_data_frame_ptr,m_data_frame_len);

        // free the packet
        free(packet_buffer);

        m_data_frame_ptr = m_save_buffer;
        m_bytes_saved += m_data_frame_len;

        m_remaining = m_bytes_saved;
        m_data_frame_len = m_bytes_saved; 

        log_debug("*****bytes accumulated = %u at %lx",m_bytes_saved,(unsigned long)m_data_frame_ptr);

        // if even more bytes are still needed from another packet to complete the
        // channel packet, just keep reading more EHS packets
        if (m_remaining < m_chan_pkt_len) {
    return;
        }

    }
    else
    {
        ASSERT(NULL == m_save_packet_buffer);
        m_save_packet_buffer = packet_buffer;
        m_save_buffer = packet_buffer;
        m_remaining = m_data_frame_len;

        log_debug("*****bytes m_remaining = %u in save buffer %lx",m_data_frame_len,(unsigned long)m_save_buffer);
    }
  
    retry:
      // process this packet's content based on the current 
      // "state" of this flow's processing
      // current assumption is packets are in order, if a packet is
      // out of sequence, drop the out of sequence packet, drop the 
      // payload being extracted, and reset the extraction state to 
      // "ES_STATE_PRIME"
    switch (CGBAConvergenceLayer::Receiver::m_estate)
      {
 
      // If the state is "ES_STATE_PRIME" then search for a primary
      // bundle after a channel packet header. Each ION bundle will 
      // be contained in a channel packet with a channel header.
      //
      // The channel packet header consists of the following 6 bytes:
      // Off  Len  Field
      // 0    1   Channel Header Marker: 0x7E
      // 1    2   CRC16 of bytes [3...end] of packet (little endian)
      // 3    1   Channel number (always 20 or 0x14 for ION bundles)
      // 4    2   Length of channel packet in bytes (little endian)
      //
      // The first byte of channel packet data for a primary bundle
      // should start with BundleProtocol::CURRENT_VERSION which is the 
      // current version of the DTN protocol. 
      //
      // Also important is that all the 0x7E's in the data after the 
      // RIC header other that the one at the start of the channel header 
      // are byte escaped to a 0x7D 0x5E pair. So an extra byte is inserted. 
      // Also to make the 0x7D unique for byte escaping, all 0x7D's are 
      // replaced with 0x7D 0x5D. So an extra byte is inserted again for any
      // 7D's in the packet.
      // 
      //
      // Therefore there must be at least 9 bytes left in the packet when 
      // searching to find the 0x7E xx xx 0x14 xx xx 0x06 which marks the 
      // start of a Primary ION bundle where 0x14 is the channel number and 
      // 0x06 is the bundle protocol version. This is because the CRC16 could be 
      // byte escaped as well as the packet length. When there are 
      // fewer than 9 bytes m_remaining to be searched which include a 0x7E, 
      // these must be saved and processed with the next EHS packet read.
      case ES_STATE_PRIME:

        // get a pointer to the last byte in the EHS packet
        last_ptr = m_data_frame_ptr + m_data_frame_len - 1;

        while(m_remaining > 0)
  {

          log_debug("***** -1-starting search at %lx with %u bytes m_remaining",(unsigned long)m_data_frame_ptr,m_remaining);
          // if there was a 0x7E in the EHS packet, look for a channel 20
          // packet that contains a primary DTN protocol bundle and a payload
          if ((cptr = (unsigned char *)memchr(m_data_frame_ptr,0x7E,m_remaining)) 
        != NULL)
          {

            // get bytes left in EHS packet inclusive of the 0x7E
            m_remaining = last_ptr - cptr + 1;
            // are there enough to guarantee a search for a primary bundle
            if (m_remaining >= 9)
      {

              // when a primary bundle is found, check if chan pkt was completed
              if ( Find_Channel_Hdr(cptr, &m_data_frame_ptr,&m_chan_pkt_len,&saved_crc,1))
        {
#ifdef HEXD
    hexdump("CCSDS Data found 0x7E",pp.getdataptr(),pp.getdatasize(),20);
#endif
                // get m_remaining bytes from the top of the DTN primary block
                m_remaining = last_ptr - m_data_frame_ptr + 1;

                log_debug("*****-1-found primary bundle header, m_remaining = %u, chan pkt len = %u, dfp = %lx\n",
        m_remaining,m_chan_pkt_len,(unsigned long)m_data_frame_ptr);fflush(stdout);
                log_debug("*****-1-%x %x %x %x %x %x %x",cptr[0],cptr[1],cptr[2],cptr[3],cptr[4],cptr[5],cptr[6]);


                // If the chan pkt was complete, process it
                if (m_remaining >= m_chan_pkt_len &&
                    Check_Escaped_Bytes(
                         m_data_frame_ptr,m_chan_pkt_len,m_remaining) >= 0)
          {
                  log_debug("-1- processing complete chan packet ");
                  // look for any byte escaped 0x7E's or 0x7D's that use up 
                  // additional bytes. Remove the extra bytes but stop short
                  // at the end of the data if there are too many escaped
                  // bytes 
#ifdef HEXD
                  hexdump("-1-before esc removed",m_data_frame_ptr-6,m_remaining,20);
#endif
                  if ((post_process_m_remaining_bytes = Remove_Escaped_Bytes(
                         m_data_frame_ptr,m_chan_pkt_len,m_remaining,
                         &escaped_bytes)) >= 0) 
      {

                    //process all blocks in the channel packet
#ifdef HEXD
                    hexdump("Packet With Payload",packet_buffer, len, 20 );
#endif
                    log_debug("*****-1- processing data ");
                    m_remaining -= escaped_bytes;
                    process_data(m_data_frame_ptr,&m_chan_pkt_len,&m_remaining);
                    m_data_frame_ptr += m_chan_pkt_len; 

                    // if any, move the m_remaining bytes to the top of the buffer
                    if (m_remaining > 0)
              {

                      log_debug("*****-1- moving %d m_remaining bytes to save buffer ",m_remaining);
                      memmove(m_save_buffer,m_data_frame_ptr,m_remaining);
                      m_bytes_saved = m_remaining;
                      m_data_frame_len = m_remaining;
                      m_data_frame_ptr = m_save_buffer;
#ifdef HEXD
                      hexdump("-1-a m_remaining bytes",m_save_buffer,m_remaining,20);
#endif
                      // retry processing the m_remaining bytes without reading
                      // another packet

                      goto retry;
                    }
                    else
              {
                      // free the packet
                      free(m_save_packet_buffer);
                      m_save_packet_buffer = NULL;
                      m_bytes_saved = 0;
                      break;
                    }
                  }
                  // else there were not enough m_remaining bytes in the packet
                  // to process all the escaped bytes, so reduce the m_remaining
                  // bytes by the number of escaped bytes so we will not 
                  // use them next time
                  else
      {
                    log_debug ("*****-1- not enough m_remaining bytes ");

                    m_remaining -= escaped_bytes;
#ifdef HEXD
                    hexdump("-1-Part pkt with escb removed",m_data_frame_ptr,m_remaining,20);
#endif
                  }
                }
                
                // else not all the chan pkt was received then , move the 
                // part received so far to the top of the packet buffer
                memmove(m_save_buffer,m_data_frame_ptr,m_remaining);
                // and get the rest of the chan pkt when the next 
                // EHS packet for that apid is received
                m_bytes_saved = m_remaining;
                // flag that more bytes are to be added
                CGBAConvergenceLayer::Receiver::m_estate = ES_STATE_PPRIME;

                log_debug("*****-1-bytes read = %d",m_remaining);fflush(stdout);
#ifdef HEXD
                hexdump("-1- Partial channel pkt ",m_save_buffer,m_remaining,20);
#endif
                break;
              }
              else
        {
                log_debug("*****-1- No channel header found ");

                // if there are any bytes left to search
                // set the pointer to just after the channel header and                
                // set m_remaining bytes accordingly
                // and keep searching for a channel packet header
                m_data_frame_ptr = cptr;
                // get bytes left in EHS packet inclusive of the 0x7E
                m_remaining = last_ptr - cptr + 1;
                if (m_remaining > 6)
    {
                  m_data_frame_ptr = m_data_frame_ptr + 6;
                  m_remaining -= 6;
                  continue;

                }
                else
    {
                  log_debug ("-1- freeing save buffer = %lx",(unsigned long)m_save_buffer);

                  // nothing in packet to process so free it
                  free(m_save_packet_buffer);
                  m_save_packet_buffer = NULL;
                  m_bytes_saved = 0;
                  m_remaining = 0;
                }
                break;
              }
      } // end if m_remaining >= 9
            // else there was a 0x7E but not enough bytes to check the header
      else
      {
              // move the m_remaining bytes in the packet to the top of the
              // current packet buffer and set to process them 
              // when the next packet is received
              m_remaining = last_ptr - cptr + 1;
              m_bytes_saved = m_remaining;
              memmove(m_save_buffer,cptr,m_remaining);

              log_debug("*****-1- moved %d m_remaining bytes to top ",m_remaining);
#ifdef HEXD
              hexdump("-1-b bytes moved to top",m_save_buffer,m_remaining,20);
#endif
              break;
            }
    } // end if 0x7E in packet
          // no channel header (0x7E) was found in this packet so go get another pkt
          // m_remaining in ES_STATE_PRIME state
    else
          {
            log_debug ("*****-1- freeing save buffer = %lx",(unsigned long)m_save_buffer);

            // free the packet 
            free(m_save_packet_buffer);
            m_save_packet_buffer = NULL;
            m_bytes_saved = 0;
            m_remaining = 0;
            break;
          }
        }  // end while m_remaining > 0

        break;

      // If only part of a chan pkt was previously received,
      // bytes from the next EHS packet have been added to it and it is now ready
      // to process
      case ES_STATE_PPRIME:

        log_debug("***** -2-looking for rest of chan packet ");fflush(stdout);
#ifdef HEXD
        hexdump("-2- Complete Buffer",m_save_buffer,m_remaining,20);
#endif
        // If the channel packet is now complete, process it
        if (m_remaining >= m_chan_pkt_len &&
            Check_Escaped_Bytes(
                  m_data_frame_ptr,m_chan_pkt_len,m_remaining) >= 0)
  {
          log_debug(" -2-packet is complete m_data_frame_ptr = %lx, chn pkt len = %u, m_remaining = %u",(unsigned long)m_data_frame_ptr,m_chan_pkt_len,m_remaining);

          // look for any byte escaped 0x7E's or 0x7D's that use up 
          // additional bytes. Remove the extra bytes but stop short
          // at the end of the data if there are too many escaped
          // bytes 
          if ((post_process_m_remaining_bytes = Remove_Escaped_Bytes(
                 m_data_frame_ptr,m_chan_pkt_len,m_remaining,
                 &escaped_bytes)) >= 0) 
    {
            //process the bundle
            log_debug("*****-2- escaped bytes = %d, processing data length=%d",escaped_bytes,m_chan_pkt_len);
            log_debug("before - m_chan_pkt_len %d m_remaining %d",m_chan_pkt_len,m_remaining);
            m_remaining -= escaped_bytes;

            process_data(m_data_frame_ptr,&m_chan_pkt_len,&m_remaining);
            log_debug("after - m_chan_pkt_len %d m_remaining %d",m_chan_pkt_len,m_remaining);

            // setup to process bytes after channel pkt 
            m_data_frame_ptr += m_chan_pkt_len; 

            // flag that a new channel packet is to be read
            CGBAConvergenceLayer::Receiver::m_estate = ES_STATE_PRIME;

            log_debug("*****-2- m_remaining bytes to process %u ",m_remaining);

            // if any, move the m_remaining bytes to the top of the buffer

            if (m_remaining > 0)
      {
              memmove(m_save_buffer,m_data_frame_ptr,m_remaining);
              m_bytes_saved = m_remaining;
              m_data_frame_len = m_remaining;
              m_data_frame_ptr = m_save_buffer;
#ifdef HEXD
              hexdump("-2-bytes moved to top",m_save_buffer,m_remaining,20);
#endif
              // retry processing the m_remaining bytes without reading
              // another packet
              goto retry;
            }
            else
      {
              log_debug (" -2-freeing save buffer = %lx",(unsigned long)m_save_buffer);
              
              // free the packet
              free(m_save_packet_buffer);
              m_save_packet_buffer = NULL;
              m_bytes_saved = 0;
              m_remaining = 0;
              break;
            }

          }

          m_remaining -= escaped_bytes;
#ifdef HEXD
          hexdump("partial pkt",m_data_frame_ptr,m_remaining,20);
#endif
          log_debug("*****-2- m_remaining = %u",m_remaining);

          // else not all the block was received then setup to add more 
          // bytes from the next EHS packet
          m_bytes_saved = m_remaining;
        }
        break;

    }
    log_debug(" leaving extract process data ");

}
//----------------------------------------------------------------------
void
CGBAConvergenceLayer::Receiver::process_data(u_char* bp, int *len, 
     int *remaining)
{
    // prepair to process a full bundle sequence
    Bundle* bundle = new Bundle();
    bool complete = false;
    int cc;

    //If we are using the PIPE to process bundles or just to test
    //the channel packet extraction within this CLA, wait here to 
    //receive a channel packet before continuing
#if(defined( TEST_WITH_PIPE) || defined(USE_PIPE_FOR_PROCESSING)) 
    int ret;    
    u_char chanbuf[16636];

    while (1)
    {
        log_debug("cgba process_data - reading on CHANNEL PIPE...");
        ret = channel_read(DEFAULT_CHANNEL, chanbuf, 16636);

        if (ret == 0) {
            //FIXME: I'll sleep when I'm dead (use channel_open_read and select)
            sleep(1);
            continue;
        }

        log_debug("cgba process_data - found input on CHANNEL PIPE, len = %d",ret);


        if (ret < 0) {   
            if (errno == EINTR) {
                continue;
            }
            log_err("error in channel_read() from CHANNEL PIPE: %d %s",
                errno, strerror(errno));
            close();
            break;
        }


        //if we are testing the chan packet extraction, do the 
        //comparison here between what was received through the 
        //pipe and what was extracted from the network packet
        //in this CLA
#if(defined(TEST_WITH_PIPE))
        if (memcmp(chanbuf,bp,ret) != 0)
  {
    log_debug(" RIC CHANNEL PIPE input did not match bundles extracted in CLA ");
      hexdump("CHANNEL PIPE INPUT",chanbuf,ret,20);

          hexdump("CLA BUNDLE INPUT",bp,ret,20);
        }
        else
  {
          log_debug(" RIC CHANNEL PIPE input matched bundles extracted in CLA ");
        }
#else
        //if we are not testing but processing with the pipe, do the 
        //bundle processing here
#ifdef DO_CBHE
        int delta=uncompress_cbhe(chanbuf,ret); //remove the cbhe compression

        cc = BundleProtocol::consume(bundle, chanbuf, ret+delta, &complete);
        *len = *len + delta;
        *remaining = *remaining - *len + delta;
#else
        cc = BundleProtocol::consume(bundle, chanbuf, ret, &complete);
        *remaining = *remaining - *len;
#endif


#endif
        break;
    }
#endif

    //if we are not using the PIPE to receive the bundles to process,
    //then process the them here from what was extracted from the 
    //network packet with this CLA
#if(!defined(USE_PIPE_FOR_PROCESSING))

    log_debug("cgba process_data - processed a bundle from Network Packet");
#ifdef DO_CBHE

    int delta = uncompress_cbhe(bp,*remaining); //remove the cbhe compression
                                           //for the primary block at the top
                                           //of the buffer

    cc = BundleProtocol::consume(bundle, bp, *len+delta, &complete);
    *len = *len + delta;
    *remaining = *remaining - *len + delta;
#else
    cc = BundleProtocol::consume(bundle, bp, *len, &complete);
    *remaining = *remaining - *len;
#endif


#endif

    //check for errors processing the bundles regardless of where
    //they came from

    if (cc < 0) {
        log_err("process_data: bundle protocol error");
        delete bundle;
        return;
    }

    if (!complete) {
        log_err("process_data: incomplete bundle");
        delete bundle;
        return;
    }

    log_debug("process_data: new bundle id %"PRIbid" arrival, length %u (payload %zu)",
              bundle->bundleid(), *len, bundle->payload().length());

    BundleDaemon::post(
        new BundleReceivedEvent(bundle, EVENTSRC_PEER, *len, EndpointID::NULL_EID()));
}

//----------------------------------------------------------------------
void
CGBAConvergenceLayer::Receiver::run()
{
    static unsigned int ret;
    in_addr_t addr;
    u_int16_t port;
    u_char *buf;

    log_debug("CGBA Receiver Running for %s:%d",intoa(local_addr_), local_port_);            
    no_conn_socket_.init_socket();
    no_conn_socket_.bind(local_addr_,local_port_);

#if(defined(USE_PIPE_FOR_PROCESSING))
    static unsigned int remaining;
#else
    //fd_set readfds;
    //struct timeval timeout;
    int status = 0;
    //FD_ZERO ( &readfds );
    //FD_SET ( no_conn_socket_.fd(), &readfds );
#endif

    CGBAConvergenceLayer::Receiver::m_estate = ES_STATE_PRIME;

    while (1) {

        if (should_stop())
            break;

#if(defined(USE_PIPE_FOR_PROCESSING))
        process_data(buf,&ret,&remaining);
#else

        //timeout.tv_sec = 0;
        //timeout.tv_usec = 900000; // 1 millisecond timeout
        //timeout.tv_usec = 900000; // 1 second timeout
        //errno = 0;
        //status = select ( no_conn_socket_.fd()+1, &readfds, 0, 0, &timeout );

        errno = 0;
        status = no_conn_socket_.poll_sockfd(POLLIN, NULL, 900);

        // anything to read?
        if ( status >= 1 ) // yes
        {
            /* make the buffer big enough to read in the packet and then
               add the contents of addition packets later incase the 
               channel packet spans multiple packets and we need to keep
               adding to the original packet that was read in */
            buf = (u_char *)malloc(MAX_CGBA_BUFFER);
        
            ret = no_conn_socket_.recvfrom((char*)buf, MAX_CGBA_PACKET, 0, &addr, &port);
            if (ret <= 0) {   
                if (errno == EINTR) 
                {
              free(buf);
                    continue;
                }
                free(buf);
                log_err("error in recvfrom(): %d %s",
                        errno, strerror(errno));
                close();
                break;
            }
            log_debug("got %d byte packet from %s:%d",
                      ret, intoa(addr), port);               

            Extract_Process_Data(buf, ret);
        } // ignore status return values of <= 0
        else {
            log_debug("status from select = %d  errno = %d", status, errno);
        }
#endif
    }

    no_conn_socket_.close();
}

//----------------------------------------------------------------------
CGBAConvergenceLayer::Sender::Sender(const ContactRef& contact)
    : Logger("CGBAConvergenceLayer::Sender",
             "/dtn/cl/cgba/sender/%p", this),
      socket_(logpath_),
      rate_socket_(logpath_, 0, 0, (oasys::RateLimitedSocket::BUCKET_TYPE) 0),
      contact_(contact.object(), "CGBACovergenceLayer::Sender")
{
}

//----------------------------------------------------------------------
bool
CGBAConvergenceLayer::Sender::init(Params* params,
                                    in_addr_t, u_int16_t)
//                                  in_addr_t addr, u_int16_t port)
    
{
    log_debug("initializing sender");
    params_ = params;
    
    return true;
}
    
//----------------------------------------------------------------------
int
CGBAConvergenceLayer::Sender::send_bundle(const BundleRef& bref)
{
    Bundle* bundle = bref.object();

    BlockInfoVec* blocks = bundle->xmit_blocks()->find_blocks(contact_->link());
    ASSERT(blocks != NULL);

    bool complete = false;
    size_t total_len = BundleProtocol::produce(bundle, blocks,
                                               buf_, 0, sizeof(buf_),
                                               &complete);
    if (!complete) {
        size_t formatted_len = BundleProtocol::total_length(blocks);
        log_err("send_bundle: bundle too big (%zu > %u)",
                formatted_len, CGBAConvergenceLayer::MAX_BUNDLE_LEN);
        return -1;
    }
        
    // write it out the socket and make sure we wrote it all
    int cc = socket_.write((char*)buf_, total_len);
    if (cc == (int)total_len) {
        log_debug("send_bundle: successfully sent bundle length %d", cc);
        return total_len;
    } else {
        log_err("send_bundle: error sending bundle (wrote %d/%zu): %s",
                cc, total_len, strerror(errno));
        return -1;
    }
}




} // namespace dtn
