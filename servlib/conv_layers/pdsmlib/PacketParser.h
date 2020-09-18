
#ifndef __PACKETPARSER_H__
#define __PACKETPARSER_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Protocols.h"


// This class is the packet parsing class.  It handles
// parsing through the primary and secondary EHS headers,
// the CCSDS primary and secondary headers, and handles
// all of the myriad of EHS secondary header types which
// are based on the protocol type, freeing the caller from
// some of those headaches.
//
// NOTE: this class assumes that when a new packet frame
// buffer address is input that it DOES start on a packet
// boundary.  However, it does NOT assume that it ends on
// a packet boundary.  Partial packets at the end of a
// frame buffer will not be processed in this class, but
// neither will they cause it a problem.

class PacketParser
{

// *** member variables ***
public:

  enum
  {
    // some shortened names for a few basic ehs parameter types

    // ccsds vs. bpdu
    CCSDS_TYPE = EHS_PDSS_CCSDS_BPDU_TYPE__CCSDS,
    BPDU_TYPE = EHS_PDSS_CCSDS_BPDU_TYPE__BPDU,

    // typebit - core vs. payload
    CORE_TYPE = EHS_PDSS_CORE_PAYLOAD_DATA_TYPE__CORE,
    PAYLOAD_TYPE = EHS_PDSS_CORE_PAYLOAD_DATA_TYPE__PAYLOAD,

    // aos vs. los status indication
    AOS = EHS_PDSS_AOS_LOS_STATUS__AOS,
    LOS = EHS_PDSS_AOS_LOS_STATUS__LOS,

    // sync pattern for ehs header
    PDSS_RESERVED_SYNC = 0x7bde,

    // basically this is a maximum unsigned short
    PDSS_MAX_PACKET_LENGTH = 0xffff,

    // the supposed ccsds maximum
    PDSS_CCSDS_MAX_PACKET_LENGTH = 4096,

    // maximum value of apid extension.  this is a 6 bit field defined
    // in the secondary ccsds header by the "CCSDS_Extended_Format_ID"
    // data field, hence this maximum value definition.
    PDSS_MAX_APID_EXTENSION = 0x3f

  };


protected:
  // pointer to start of packet frame buffer
  volatile const unsigned char* m_startptr;

  // pointer to current packet within packet frame buffer
  volatile const unsigned char* m_pktptr;

  // number of packets in the packet frame buffer
  volatile int m_count;

  // total size of packet frame buffer not including any
  // trailing partial packets
  volatile int m_size;

  // user specified packet frame buffer size
  volatile int m_framesize;

  // the packet protocol of the current packet.  this value is
  // heavily used throughout the code, and is therefore extracted
  // once when the current packet is first referenced.
  volatile int m_protocol;

  // invalid packet length errors
  volatile int m_invalid_pktlen;
  volatile bool m_invalid_pktlen_detected;

  // storage area for the packet headers for the current packet.
  // these headers are extracted when the current packet is first
  // referenced, and stored in this structure to provide
  // properly aligned structure references throughout the code.
  // since it is possible for a packet within the packet frame
  // buffer to start on any byte boundary, it is not possible
  // to reliably reference the structure types through the
  // current packet pointer without encountering the possibility
  // of "bus errors" in the code.
  typedef union EHS_Packet_Header
  {
    EHS_Packet_Type std;
    EHS_Packet_Type2 nonstd1;
  } EHS_Packet_Header_t;

  EHS_Packet_Header_t m_headers;

  // 5-byte buffer for saving converted 7-byte-TDM-OBT-to-EmbTime format
  unsigned char m_TDM_EmbeddedTime_buffer[5];

private:


// *** methods ***
public:
  // constructor
  PacketParser();

  // destructor
  virtual ~PacketParser();

  // set a new pointer.  you should call this method with the
  // address of your frame receipt buffer, and the number of
  // bytes in that buffer, to begin parsing a new frame buffer.
  //
  // this method returns false if any major problems are
  // found with the data in the frame for the first packet.
  virtual bool setframeptr ( const unsigned char* dataptr,
    int framesize );

  // get the current packet pointer, which will be pointing to the
  // start of the current packet, at its primary EHS packet header
  virtual const unsigned char* getpktptr();

  // reset the current packet pointer to the beginning of the
  // frame buffer as initially set by the "setframeptr" call
  virtual void resetframeptr();

  // get a count of the number of packets in this frame buffer
  virtual int count();

  // get a total size for all the packets in this frame buffer
  // not including any trailing partial packets
  virtual int size();

  // increment the current packet data pointer by the size of the
  // current data packet, effectively "bumping" you on to the
  // next packet.  both the prefix (++pkt) and postfix (pkt++)
  // notation is supported, however, there is NOT a distinction
  // between their operation, as would normally be the case with
  // these operators.  both will attempt to increment the pointer,
  // returning false if the attempt would exceed the total full
  // packet count within the frame buffer, which will therefore
  // always leaving you pointing to the last available packet.
  // or it returns true with the current data pointer bumped
  // on to the next packet.
  virtual bool operator++();
  virtual bool operator++(int);

  // get a pointer to the current packets actual data
  //
  // note:  this pointer is to the data zone itself,
  // bypassing both the EHS primary and secondary
  // headers, as well as the CCSDS primary and
  // secondary headers (if any).
  virtual unsigned char* getdataptr();

  // get the size of the current packets actual data
  //
  // note:  as above, this size does NOT include
  // either of the EHS headers, nor the CCSDS
  // headers (if any).  this is the size of the
  // data zone itself, minus any and all headers.
  virtual int getdatasize();


  // *** primary EHS header info ***

  // get the total size (in bytes) of this packet, including all
  // headers as well as the data
  virtual int getpktsize();

  // if any invalid packet length errors are detected these
  // methods can be used to iterrogate the situation
  virtual int get_invalid_pktlen();
  virtual bool invalid_pktlen_detected();

  // general purpose method to validate an EHS packet length.
  // this method is NOT tied to the packet currently being
  // processed, this is just a general purpose packet length
  // check to validate the specified number for "reasonableness",
  // if there is such a word.
  virtual bool validate_pktlen ( int pktlen, int protocol ) const;

  // method to verify an EHS packet length, which first validates
  // it as being a reasonable value (by calling validate_pktlen),
  // but then takes it one step further, for ccsds packets only,
  // by comparing the specified EHS packet length against the
  // ccsds packet length for the packet currently being
  // processed.
  virtual bool verify_pktlen ( int pktlen, int protocol );

  // get the EHS version ID (0-15)
  virtual int getversion();

  // get the project id, e.g. EHS_PROJECT__ISS
  virtual int getproject();

  // get the operational support mode, e.g. EHS_SUPPORT_MODE__FLIGHT
  virtual int getopmode();

  // get the data mode, e.g. EHS_DATA_MODE__REALTIME, etc.
  virtual int getdatamode();

  // get the mission increment (0-255)
  virtual int getmission();

  // get the protocol type.  method returns one of the EHS protocol
  // types, e.g. EHS_PROTOCOL__PDSS_PAYLOAD_CCSDS_PACKET or other
  // EHS_PROTOCOL__ values
  virtual int getprotocol();

  // get the ground receipt time.  returns a pointer to the GRT
  // which is in the EHS_Time_Type format.  this will be an actual
  // pointer within your data buffer.
  virtual const unsigned char* getgrt();

  // get the "new data" flag.  indicates the time has changed.
  virtual bool getnewdata();

  // get the hold(true)/no-hold(false) status
  virtual bool gethold();

  // get the "time sign", true -> this is count-down time and
  // false -> this is NOT count-down time
  virtual bool getcdt();


  // *** secondary EHS header info ***
  //
  // this can be dependent on the protocol type, so some methods
  // will return, e.g. -1 or NULL, to indicate the operation is
  // invalid for the packets protocol type, or in some cases,
  // simply unavailable.

  // get the secondary header version
  virtual int getversion2();

  // get the VCDU sequence number
  virtual int getseqno();

  // get the APID (application identifier)
  //
  // note: if the "fromccsds" parameter is not null a status
  // is returned indicating whether the apid value had to be
  // obtained from the CCSDS header rather than the EHS header.
  virtual int getapid ( bool* fromccsds = NULL );

  // get the routing id
  virtual int getroutingid();

  // get Core vs. Payload
  // this value is important and effectively doubles the number
  // of APIDs available.  the same APID can be used for both
  // Core (station status type data) and Payload (obviously,
  // payload type data).
  virtual int getcorepayload();

  // get type bit
  // this value is the same value as above, except that it is
  // extracted from the ccsds primary header instead of the
  // ehs header, assuming the packet in question is ccsds.
  virtual int gettypebit();

  // pdss reserved sync pattern
  virtual int getsync();

  // determine packet type
  //
  // note: it is possible for both of these methods to return false
  virtual bool isbpdu();
  virtual bool isccsds();

  // get bpdu type bit as opposed to protocol.  this method can be
  // used to determine if, for example, a udsm packet is intended
  // for its corresponding bpdu or ccsds apid.
  virtual bool getbpdutype();

  // get apid extension (for nrt)
  virtual int getapidextension();

  // get zoe tlm bit
  virtual int getzoetlm();


  // *** other header info ***

  // get the CCSDS sequence counter flags the primary CCSDS header,
  virtual int getccsdsseqflags();

  // get the CCSDS sequence counter from the primary CCSDS header,
  // or -1 if not available.  this is actually a 14-bit counter
  // ranging from 0 to 16383, which of course can wrap around.
  virtual int getccsdsseqno();

  // get the CCSDS secondary header flag
  virtual int getccsdssecondaryheaderflag();

  // get the CCSDS packet length
  virtual int getccsdspktlen();

  // get "embedded" time.  method returns a pointer to the 40-bit
  // packet "embedded time" from the secondary CCSDS header, if
  // available, or NULL if not.
  virtual const unsigned char* getemb();

  // get gse format id
  virtual int getgseformatid();

  // get tdm format id
  virtual int gettdmformatid();


  // *** data info ***

  // get AOS/LOS status from an AOS/LOS packet
  // where AOS=1 LOS=0 undefined=-1
  virtual int getaoslos();

  // get the size of the various header types for the current packet
  virtual int EHSPrimaryHeaderSize();
  virtual int EHSSecondaryHeaderSize();
  virtual int CCSDSPrimaryHeaderSize();
  virtual int CCSDSSecondaryHeaderSize();

protected:
  // increment the current packet pointer to the next packet
  virtual bool increment();

  // copy the headers of the current packet
  // to properly aligned header structures
  virtual void copyheaders();

  // check to see if the specified packet pointer is fully
  // contained within the bounds of the packet frame buffer
  virtual bool withinbounds ( const unsigned char* pktptr );

private:
  // object initialization method
  void init();

};

#endif  // __PACKETPARSER_H__

