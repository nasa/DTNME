
// This class is the packet parsing class.  It handles
// parsing through the primary and secondary EHS headers,
// and all the other myriad of headers based on protocol
// type, freeing the caller from some of those headaches.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "EHSTime.h" // for matt's EHSTime object for tdm embedded time
#include "PacketParser.h"
#include "CUtils.h"

// initialize all data members
void PacketParser::init()
{
  m_count = -1;
  m_size = -1;
  m_framesize = 0;
  m_protocol = 0;
  m_startptr = NULL;
  m_pktptr = NULL;
  m_invalid_pktlen = 0;
  m_invalid_pktlen_detected = false;
  memset ( &m_headers, 0, sizeof(m_headers) );
}


// constructor
PacketParser::PacketParser()
{
  init();
}


// destructor
PacketParser::~PacketParser()
{
}


// set a new pointer.  you should call this method with the
// address of your frame receipt buffer, and the number of
// bytes in that buffer, to begin parsing a new frame buffer.
//
// this method returns false if any major problems are
// found with the data in the frame for the first packet.
bool PacketParser::setframeptr ( const unsigned char* dataptr,
  int framesize )
{
  m_count = -1;
  m_size = -1;

  m_framesize = framesize;
  m_startptr = dataptr;
  m_pktptr = dataptr;

  m_invalid_pktlen = 0;
  m_invalid_pktlen_detected = false;

  // copy header data to internal structures
  copyheaders();

  // make sure the first packet is not a partial packet
  if (! withinbounds(dataptr))
  {
    // don't invalidate pointers in case someone wants
    // to try and look at the info it would point to
    // m_startptr = NULL;
    // m_pktptr = NULL;
    return false;
  }

  // make another simple sanity check
  int sync = getsync();

  // looks like an invalid packet, so about all we can
  // do is treat this the same as a partial packet
  if (sync != PDSS_RESERVED_SYNC)
  {
    // don't invalidate pointers in case someone wants
    // to try and look at the info it would point to
    // m_startptr = NULL;
    // m_pktptr = NULL;
    return false;
  }

  return true;

}


// get the current packet pointer, which will be pointing to the
// start of the current packet, at its primary EHS packet header
const unsigned char* PacketParser::getpktptr()
{
  return (const unsigned char *)m_pktptr;
}


// reset the current packet pointer to the beginning of the
// frame buffer as initially set by the "setframeptr" call
void PacketParser::resetframeptr()
{
  m_pktptr = m_startptr;
  copyheaders();
}


// get a count of the number of packets in this frame buffer
int PacketParser::count()
{
  if (m_count != -1) return m_count;
  if (! m_startptr) return 0;

  long pktsize, diff;
  const unsigned char* ptr = (const unsigned char *)m_startptr;
  EHS_Primary_Header_Type primaryhdr;

  m_count = 0;
  m_size = 0;

  do
  {
    // partial packets should not be processed
    if (! withinbounds(ptr)) break;

    // copy data to an EHS primary header structure
    // and extract this packets size
    memcpy (&primaryhdr, ptr, sizeof(primaryhdr));

    // perform 4-byte integer byte swapping if necessary
    byte_swap4 ( (unsigned char *)&primaryhdr, sizeof(primaryhdr) );

    pktsize = (long)primaryhdr.HOSC_Packet_Size;
    int protocol = (int)primaryhdr.Protocol_Type;

    // check for an invalid packet length
    if (! validate_pktlen (pktsize, protocol) )
    {
      m_invalid_pktlen = pktsize;
      m_invalid_pktlen_detected = true;
      break;
    }

    ptr += pktsize;
    diff = (long)(ptr - m_startptr);

    // bump the packet count and total packet size
    ++m_count;
    m_size += pktsize;

  } while (diff < m_framesize);

  return m_count;

}


// get a total size for all the packets in this frame buffer
int PacketParser::size()
{
  if (m_size != -1) return m_size;
  count();  // the count method also calculates the total size
  return m_size;
}


// increment the current packet data pointer by the size of the
// current data packet, effectively "bumping" you on to the
// next packet.  both the prefix (++pkt) and postfix (pkt++)
// notation is supported, however, there is NOT a distinction
// between their operation, as would normally be the case with
// these operators.  both will attempt to increment the pointer,
// returning false if the attempt would exceed the frame size,
// always leaving you pointing to the last available packet, or
// returning true with the current data pointer already bumped
// on to the next packet.  you cannot exceed the frame size.
bool PacketParser::increment()
{
  if (! m_pktptr) return false;

  long pktsize = getpktsize();

  // check for an invalid packet length on the current packet.
  // if it's not valid, then we can't reasonably increment to
  // the next packet in the buffer, can we?
  if (! verify_pktlen (pktsize, m_protocol) )
  {
    // this was probably already detected and set before we
    // ever got here, but just in case not...
    m_invalid_pktlen = pktsize;
    m_invalid_pktlen_detected = true;
    return false;
  }

  long diff = (long)(m_pktptr - m_startptr);
  if ((diff + pktsize) >= m_framesize) return false;

  // if the next packet is only a partial one, do not increment to it
  const unsigned char* ptr =
    (const unsigned char *)(m_pktptr + pktsize);

  // on second thought, go ahead and increment to the next
  // packet anyway, just in case someone wants to take a
  // look at the trash it is pointing to
  //
  // if (! withinbounds(ptr)) return false;

  // volatile const unsigned char* pktptr = m_pktptr;
  m_pktptr += pktsize;
  copyheaders();

  // check boundary conditions after we've bumped the pointer
  if (! withinbounds(ptr)) return false;

  // make a simple sanity check
  int sync = getsync();

  // looks like an invalid packet, so either the length of the
  // previous packet was invalid, or this packet is trashed,
  // so about all we can is pretend there ain't no more in
  // this buffer.
  if (sync != PDSS_RESERVED_SYNC)
  {
    // this was probably already detected and set before we
    // ever got here, but just in case not...
    m_invalid_pktlen = pktsize;
    m_invalid_pktlen_detected = true;
    return false;
  }

  return true;

}


bool PacketParser::operator++()
{
  return increment();
}

bool PacketParser::operator++(int)
{
  return increment();
}


// get a pointer to the current packets actual data
//
// note:  this pointer is to the data zone itself,
// bypassing both the EHS primary and secondary
// headers, as well as the CCSDS primary and
// secondary headers (if any).
//
// note: for TDM packets, data starts past the variable-length 2ndary header
// and ccsds primary header
unsigned char* PacketParser::getdataptr()
{
  if (! m_pktptr) return NULL;

  // bump past the primary and secondary EHS headers
  unsigned char* ptr = (unsigned char *)
    ( m_pktptr + EHSPrimaryHeaderSize() + EHSSecondaryHeaderSize() );

  // if this is a CCSDS packet type, then bump past the
  // primary, and possibly secondary, CCSDS headers
  if ( isccsds() )
  {
    ptr += CCSDSPrimaryHeaderSize();
    // if we're sure we do have a secondary ccsds header
    if ( getccsdssecondaryheaderflag() > 0 ) 
    {
      ptr += CCSDSSecondaryHeaderSize();
    }
  }

  return ptr;

}


// get the size of the current packets actual data
//
// note:  as above, this size does NOT include
// either of the EHS headers, nor the CCSDS
// headers (if any).  this is the size of the
// data zone itself, minus any and all headers.
int PacketParser::getdatasize()
{
  if (! m_pktptr) return 0;

  // get full packet size
  long pktsize = getpktsize();

  // check for an invalid packet length
  if (! verify_pktlen (pktsize, m_protocol) )
  {
    m_invalid_pktlen = pktsize;
    m_invalid_pktlen_detected = true;
    return 0;
  }

  // subtract header(s) size
  long datasize = pktsize - (long)(getdataptr() - m_pktptr);

  if (datasize < 0) datasize = 0;

  // whatever is left should be the data size
  return datasize;

}



// *** primary EHS header info ***

// get the total size (in bytes) of this packet, including all
// headers as well as the data
int PacketParser::getpktsize()
{
  if (! m_pktptr) return 0;

  int pktsize = m_headers.std.primary.HOSC_Packet_Size;

  // check for an invalid packet length
  if (! verify_pktlen (pktsize, m_protocol) )
  {
    m_invalid_pktlen = pktsize;
    m_invalid_pktlen_detected = true;
    // return what we see, don't try to disguise the trash
    // pktsize = 0;
  }

  return pktsize;

}


// if any invalid packet length errors are detected these
// methods can be used to iterrogate the situation
int PacketParser::get_invalid_pktlen()
{
  return m_invalid_pktlen;
}


bool PacketParser::invalid_pktlen_detected()
{
  return m_invalid_pktlen_detected;
}


// general purpose method to validate an EHS packet length.
// this method is NOT tied to the packet currently being
// processed, this is just a general purpose packet length
// check to validate the specified number for "reasonableness",
// if there is such a word.
bool PacketParser::validate_pktlen ( int pktlen, int protocol ) const
{
  if ( pktlen < 1  ||  pktlen > (PDSS_CCSDS_MAX_PACKET_LENGTH + EHS_TOTAL_HEADER_SIZE) )
  {
    switch ( protocol )
    {
    case EHS_PROTOCOL__GSE_DATA:
      if ( pktlen < 1  ||  pktlen > PDSS_MAX_PACKET_LENGTH )
      {
        return false;
      }
      else
      {
        return true;
      }
      break;

    case EHS_PROTOCOL__TDM_TELEMETRY:
      // 512 (ehs prim hdr + sec hdr) + 64991 (ccsds packet) = 65503
      if ( pktlen < 1 || pktlen > 65503 )
      {
        return false;
      }
      else
      {
        return true;
      }
      break;
    
    case EHS_PROTOCOL__PSEUDO_TELEMETRY:
      // max packet size per SSP 50305 ~ipr 971
      if ( pktlen < 1 || pktlen > 61438 )
      {
        return false;
      }
      else
      {
        return true;
      }
      break;

    default:
      return false;
    }
  }

  return true;

}


// method to verify an EHS packet length, which first validates
// it as being a reasonable value (by calling validate_pktlen),
// but then takes it one step further, for ccsds packets only,
// by comparing the specified EHS packet length against the
// ccsds packet length for the packet currently being
// processed.
bool PacketParser::verify_pktlen ( int pktlen, int protocol )
{
  // perform general EHS packet length validation
  if ( ! validate_pktlen (pktlen, protocol) )
  {
    return false;
  }

  // if this is not a ccsds packet, then since it already
  // passed the check above, there's nothing else to do
  if ( ! isccsds() ) return true;

  // get the ccsds packet length
  int ccsds_pktlen = getccsdspktlen();

  // the EHS packet length should be equal to the ccsds
  // packet length (which includes the ccsds secondary
  // header length, if any), plus the ccsds primary
  // header length (which is not included in the ccsds
  // packet length value), plus the size of the EHS
  // primary and secondary headers.
  int total_pktlen = ccsds_pktlen + CCSDSPrimaryHeaderSize() +
    EHSPrimaryHeaderSize() + EHSSecondaryHeaderSize();

  if ( total_pktlen != pktlen )
  {
    return false;
  }

  return true;

}


// get the EHS version ID (0-15)
int PacketParser::getversion()
{
  if (! m_pktptr) return -1;
  return m_headers.std.primary.Version;
}


// get the project id, e.g. EHS_PROJECT__ISS
int PacketParser::getproject()
{
  if (! m_pktptr) return -1;
  return m_headers.std.primary.Project;
}


// get the operational support mode, e.g. EHS_SUPPORT_MODE__FLIGHT
int PacketParser::getopmode()
{
  if (! m_pktptr) return -1;
  return m_headers.std.primary.Support_Mode;
}


// get the data mode, e.g. EHS_DATA_MODE__REALTIME, etc.
int PacketParser::getdatamode()
{
  if (! m_pktptr) return -1;
  return m_headers.std.primary.Data_Mode;
}


// get the mission increment (0-255)
int PacketParser::getmission()
{
  if (! m_pktptr) return -1;
  return m_headers.std.primary.Mission;
}


// get the protocol type.  method returns one of the EHS protocol
// types, e.g. EHS_PROTOCOL__PDSS_PAYLOAD_CCSDS_PACKET or other
// EHS_PROTOCOL__ values
int PacketParser::getprotocol()
{
  if (! m_pktptr) return -1;
  return m_headers.std.primary.Protocol_Type;
}


// get the ground receipt time.  returns a pointer to the GRT
// which is in the EHS_Time_Type format.  this will be an actual
// pointer within your data buffer.
const unsigned char* PacketParser::getgrt()
{
  if (! m_pktptr) return NULL;

  // since you cannot take the address of a bit field, we must either
  // "hard code" this offset, or go through the hassle of allocating
  // space, and requiring the users code to then delete that space.
  // the author has chosen to "hard code" this for two reasons:
  //
  // 1) it is HIGHLY unlikely this will ever change, and if it
  //    ever does, it is encapsulated right here, and this is
  //    the only code that will have to be changed
  // 2) I do not want to hassle with deleting allocated space,
  //    as this is far more likely to create bugs than the
  //    "hard coding" of this value ever will
  //
  // m_pktptr points to the start of the EHS primary header.  the
  // ground receipt time begins at the bit field labeled "Year"
  // within this header, which is exactly 4 bytes past the start
  // of the header.
  return (const unsigned char *)(m_pktptr + 4);

}


// get the "new data" flag.  indicates the time has changed.
bool PacketParser::getnewdata()
{
  if (! m_pktptr) return false;
  return (m_headers.std.primary.New_Data_Flag == 1);
}


// get the hold(true)/no-hold(false) status
bool PacketParser::gethold()
{
  if (! m_pktptr) return false;
  return (m_headers.std.primary.Hold_Flag == 1);
}


// get the "time sign", true -> this is count-down time and
// false -> this is NOT count-down time
bool PacketParser::getcdt()
{
  if (! m_pktptr) return false;
  return (m_headers.std.primary.Sign_Flag == 1);
}



// *** secondary EHS header info ***
// this can be dependent on the protocol type, so some methods
// will return, e.g. -1 or NULL, to indicate the operation is
// invalid for the packets protocol type, or in some cases,
// simply unavailable.

// get the secondary header version
int PacketParser::getversion2()
{
  if (! m_pktptr) return -1;

  switch (m_protocol)
  {
  case EHS_PROTOCOL__TDM_TELEMETRY:
    return 0; // zero by definition

  case EHS_PROTOCOL__NASCOM_TELEMETRY:
    return -1;

  case EHS_PROTOCOL__PSEUDO_TELEMETRY:
    return 0; // zero by definition

  case EHS_PROTOCOL__TDS_DATA:
    return -1;

  case EHS_PROTOCOL__TEST_DATA:
    return -1;

  case EHS_PROTOCOL__GSE_DATA:
    // no version number in gse secondary header
    return -1;

  case EHS_PROTOCOL__CUSTOM_DATA:
    return -1;

  case EHS_PROTOCOL__HDRS_DQ:
    return -1;

  case EHS_PROTOCOL__CSS:
    return -1;

  case EHS_PROTOCOL__AOS_LOS:
    return m_headers.std.secondary.aoslos.Version;

  case EHS_PROTOCOL__PDSS_PAYLOAD_CCSDS_PACKET:
    return m_headers.std.secondary.payload.Version;

  case EHS_PROTOCOL__PDSS_CORE_CCSDS_PACKET:
    return m_headers.std.secondary.core.Version;

  case EHS_PROTOCOL__PDSS_PAYLOAD_BPDU:
    return m_headers.std.secondary.bpdu.Version;

  case EHS_PROTOCOL__PDSS_UDSM:
    return m_headers.std.secondary.udsm.Version;

  case EHS_PROTOCOL__PDSS_RPSM:
    return -1;

  default:
    return -1;

  }

}


// get the VCDU sequence number
int PacketParser::getseqno()
{
  if (! m_pktptr) return -1;

  switch (m_protocol)
  {
  case EHS_PROTOCOL__TDM_TELEMETRY:
    return -1;

  case EHS_PROTOCOL__NASCOM_TELEMETRY:
    return -1;

  case EHS_PROTOCOL__PSEUDO_TELEMETRY:
    return -1;

  case EHS_PROTOCOL__TDS_DATA:
    return -1;

  case EHS_PROTOCOL__TEST_DATA:
    return -1;

  case EHS_PROTOCOL__GSE_DATA:
    // no vcdu sequence counter in gse secondary header
    return -1;

  case EHS_PROTOCOL__CUSTOM_DATA:
    return -1;

  case EHS_PROTOCOL__HDRS_DQ:
    return -1;

  case EHS_PROTOCOL__CSS:
    return -1;

  case EHS_PROTOCOL__AOS_LOS:
    return m_headers.std.secondary.aoslos.VCDU_Sequence_Number;

  case EHS_PROTOCOL__PDSS_PAYLOAD_CCSDS_PACKET:
    return m_headers.std.secondary.payload.VCDU_Sequence_Number;

  case EHS_PROTOCOL__PDSS_CORE_CCSDS_PACKET:
    return m_headers.std.secondary.core.VCDU_Sequence_Number;

  case EHS_PROTOCOL__PDSS_PAYLOAD_BPDU:
    return m_headers.std.secondary.bpdu.VCDU_Sequence_Number;

  case EHS_PROTOCOL__PDSS_UDSM:
    return m_headers.std.secondary.udsm.VCDU_Sequence_Number;

  case EHS_PROTOCOL__PDSS_RPSM:
    return -1;

  default:
    return -1;

  }


}


// get the APID (application identifier)
int PacketParser::getapid ( bool* fromccsds )
{
  unsigned char* ptr = NULL; // for parsing through tdm pkt
  int apid = -1;    // the apid to be returned

  if (! m_pktptr) return -1;

  if (fromccsds) *fromccsds = false;

  switch (m_protocol)
  {
  // these special cases are handled differently due to the
  // unusual nature of these protocols.  these protocols contain
  // very different ehs secondary header structures, and cannot
  // be mapped into the m_headers union like the types we are
  // used to dealing with.  therefore the contents of the ehs
  // secondary, and ccsds portions, of m_headers is not valid.
  // however, the actual packet contents are simply directly
  // accessed at the bit level where necessary, so that the
  // parameters are extracted directly from the data buffer.
  //
  // note:  this same problem is handled in much the same way
  // in the PacketParserWriter derived class, and elsewhere
  // throughout this class.
  //
  case EHS_PROTOCOL__PSEUDO_TELEMETRY: // like TDM: + PrimHdr + SecHdr + 0
    // PSEUDO FALLS THROUGH TO TDM CODE!!! 
  case EHS_PROTOCOL__TDM_TELEMETRY:
    // note:  neither pseudo nor tdm ehs secondary headers
    // contain the apid number which MUST be extracted from
    // the ccsds primary header
    ptr = (unsigned char *)
           ( 
            m_pktptr +  // start of packet
            EHSPrimaryHeaderSize() +  // skip primary header
            EHSSecondaryHeaderSize() // skip variable length 2ndary header
           ); // now pointing at first byte of ccsds header (byte 0)
    // apid is bits 2-0 of byte 0, 7-0 of byte 1 so mask with 00000111 11111111
    //
    apid = 0; // INITIALIZE TO ZERO
    apid |= (unsigned char)((unsigned char)(*ptr) & 0x07); // get bits 2-0  00000111
    apid = apid << 8;    // shift 8
    ptr++;        // point to next byte (byte 1)
    apid |= *ptr;  // get bits 7-0 11111111
    return apid;

  case EHS_PROTOCOL__NASCOM_TELEMETRY:
    return -1;

  case EHS_PROTOCOL__TDS_DATA:
    return -1;

  case EHS_PROTOCOL__TEST_DATA:
    return -1;

  case EHS_PROTOCOL__GSE_DATA:
    apid = m_headers.nonstd1.secondary.gse.APID;
    break;

  case EHS_PROTOCOL__CUSTOM_DATA:
    return -1;

  case EHS_PROTOCOL__HDRS_DQ:
    return -1;

  case EHS_PROTOCOL__CSS:
    return -1;

  case EHS_PROTOCOL__AOS_LOS:
    apid = m_headers.std.secondary.aoslos.APID;
    break;

  case EHS_PROTOCOL__PDSS_PAYLOAD_CCSDS_PACKET:
    apid = m_headers.std.secondary.payload.APID;
    break;

  case EHS_PROTOCOL__PDSS_CORE_CCSDS_PACKET:
    apid = m_headers.std.secondary.core.Data_Stream_ID & 0x000007ff;
    break;

  case EHS_PROTOCOL__PDSS_PAYLOAD_BPDU:
    apid = m_headers.std.secondary.bpdu.APID;
    break;

  case EHS_PROTOCOL__PDSS_UDSM:
    apid = m_headers.std.secondary.udsm.APID;
    break;

  case EHS_PROTOCOL__PDSS_RPSM:
    return -1;

  default:
    return -1;

  }

  // if the apid is zero in the EHS secondary header
  // then look for it in the CCSDS header.  this can
  // happen due to the TSI handling of apids that are
  // not configured for processing.  rather than
  // just throwing them away, the apid and routing
  // information in the headers is set to zero so
  // that software such as the DSM software will
  // still be able to store the data and it is not
  // just tossed into the bit bucket entirely.
  if (apid > 0) return apid;

  // make sure this is a CCSDS packet type
  if (! isccsds() ) return apid;

  // fetch the apid from the CCSDS header instead
  if (fromccsds) *fromccsds = true;

  int cp = getcorepayload();

  switch (m_protocol)
  {
  case EHS_PROTOCOL__GSE_DATA:
    if ( cp == PacketParser::PAYLOAD_TYPE )
    {
      return m_headers.nonstd1.ccsds.gse.payload.CCSDS_Application_ID;
    }
    else
    {
      return m_headers.nonstd1.ccsds.gse.core.CCSDS_Application_ID;
    }

  default:
    return m_headers.std.ccsds.primary2.CCSDS_Application_ID;
  }

}


// get routing id
int PacketParser::getroutingid()
{
  if (! m_pktptr) return 0;

  switch (m_protocol)
  {
  case EHS_PROTOCOL__TDM_TELEMETRY:
    return 0;

  case EHS_PROTOCOL__NASCOM_TELEMETRY:
    return 0;

  case EHS_PROTOCOL__PSEUDO_TELEMETRY:
    return 0;

  case EHS_PROTOCOL__TDS_DATA:
    return 0;

  case EHS_PROTOCOL__TEST_DATA:
    return 0;

  case EHS_PROTOCOL__GSE_DATA:
    return m_headers.nonstd1.secondary.gse.Routing_ID;

  case EHS_PROTOCOL__CUSTOM_DATA:
    return 0;

  case EHS_PROTOCOL__HDRS_DQ:
    return 0;

  case EHS_PROTOCOL__CSS:
    return 0;

  case EHS_PROTOCOL__AOS_LOS:
    return m_headers.std.secondary.aoslos.Routing_ID;

  case EHS_PROTOCOL__PDSS_PAYLOAD_CCSDS_PACKET:
    return m_headers.std.secondary.payload.Routing_ID;

  case EHS_PROTOCOL__PDSS_CORE_CCSDS_PACKET:
    return m_headers.std.secondary.core.Routing_ID;

  case EHS_PROTOCOL__PDSS_PAYLOAD_BPDU:
    return m_headers.std.secondary.bpdu.Routing_ID;

  case EHS_PROTOCOL__PDSS_UDSM:
    return m_headers.std.secondary.udsm.Routing_ID;

  case EHS_PROTOCOL__PDSS_RPSM:
    return 0;

  default:
    return 0;

  }

}


// get Core vs. Payload, 0=Core 1=Payload.  this value is important
// and effectively doubles the number of APIDs available.  the
// same APID can be used for both Core (station status type data)
// and Payload (obviously payload data).
int PacketParser::getcorepayload()
{
  if (! m_pktptr) return -1;

  switch (m_protocol)
  {
  case EHS_PROTOCOL__TDM_TELEMETRY:
    // this value is undefined for the TDM format
    // so we are simply going to pick one to use
    return 0;

  case EHS_PROTOCOL__NASCOM_TELEMETRY:
    return -1;

  case EHS_PROTOCOL__PSEUDO_TELEMETRY:
    // this value is undefined for the PSEUDO format
    // so we are simply going to pick one to use
    return 0;

  case EHS_PROTOCOL__TDS_DATA:
    return -1;

  case EHS_PROTOCOL__TEST_DATA:
    return -1;

  case EHS_PROTOCOL__GSE_DATA:
    return m_headers.nonstd1.secondary.gse.Payload_vs_Core_ID;

  case EHS_PROTOCOL__CUSTOM_DATA:
    return -1;

  case EHS_PROTOCOL__HDRS_DQ:
    return -1;

  case EHS_PROTOCOL__CSS:
    return -1;

  case EHS_PROTOCOL__AOS_LOS:
    return m_headers.std.secondary.aoslos.Payload_vs_Core_ID;

  case EHS_PROTOCOL__PDSS_PAYLOAD_CCSDS_PACKET:
    return m_headers.std.secondary.payload.Payload_vs_Core_ID;

  case EHS_PROTOCOL__PDSS_CORE_CCSDS_PACKET:
    return (m_headers.std.secondary.core.Data_Stream_ID & 0x00000800) >> 11;

  case EHS_PROTOCOL__PDSS_PAYLOAD_BPDU:
    return m_headers.std.secondary.bpdu.Payload_vs_Core_ID;

  case EHS_PROTOCOL__PDSS_UDSM:
    return m_headers.std.secondary.udsm.Payload_vs_Core_ID;

  case EHS_PROTOCOL__PDSS_RPSM:
    return -1;

  default:
    return -1;

  }

}


// get type bit
// this value is the same as "getcorepayload", except that it
// is extracted from the ccsds primary header instead of the
// ehs header, assuming the packet in question is ccsds.
int PacketParser::gettypebit()
{
  if (! m_pktptr) return -1;

  // make sure this is a CCSDS packet type
  if (! isccsds() ) return -1;

  switch (m_protocol)
  {
  case EHS_PROTOCOL__PSEUDO_TELEMETRY:
    return 0; // zero by definition 

  case EHS_PROTOCOL__TDM_TELEMETRY:
    return 0; // zero by definition on TDM data

  case EHS_PROTOCOL__GSE_DATA:
    return m_headers.nonstd1.ccsds.gse.payload.CCSDS_Type_ID;

  default:
    return m_headers.std.ccsds.primary2.CCSDS_Type_ID;
  }
}


// pdss reserved sync pattern
int PacketParser::getsync()
{
  if (! m_pktptr) return -1;

  switch (m_protocol)
  {
  case EHS_PROTOCOL__TDM_TELEMETRY:
    return PDSS_RESERVED_SYNC;

  case EHS_PROTOCOL__NASCOM_TELEMETRY:
    return PDSS_RESERVED_SYNC;

  case EHS_PROTOCOL__PSEUDO_TELEMETRY:
    return PDSS_RESERVED_SYNC;

  case EHS_PROTOCOL__TDS_DATA:
    return PDSS_RESERVED_SYNC;

  case EHS_PROTOCOL__TEST_DATA:
    return PDSS_RESERVED_SYNC;

  case EHS_PROTOCOL__GSE_DATA:
    return m_headers.nonstd1.secondary.gse.PDSS_Reserved_Sync;

  case EHS_PROTOCOL__CUSTOM_DATA:
    return PDSS_RESERVED_SYNC;

  case EHS_PROTOCOL__HDRS_DQ:
    return PDSS_RESERVED_SYNC;

  case EHS_PROTOCOL__CSS:
    return PDSS_RESERVED_SYNC;

  case EHS_PROTOCOL__AOS_LOS:
    return m_headers.std.secondary.aoslos.PDSS_Reserved_Sync;

  case EHS_PROTOCOL__PDSS_PAYLOAD_CCSDS_PACKET:
    return m_headers.std.secondary.payload.PDSS_Reserved_Sync;

  case EHS_PROTOCOL__PDSS_CORE_CCSDS_PACKET:
    return m_headers.std.secondary.core.PDSS_Reserved_Sync;

  case EHS_PROTOCOL__PDSS_PAYLOAD_BPDU:
    return m_headers.std.secondary.bpdu.PDSS_Reserved_Sync;

  case EHS_PROTOCOL__PDSS_UDSM:
    return m_headers.std.secondary.udsm.PDSS_Reserved_Sync;

  case EHS_PROTOCOL__PDSS_RPSM:
    return PDSS_RESERVED_SYNC;

  default:
    return -1;

  }

}


// determine packet type
bool PacketParser::isbpdu()
{
  if (! m_pktptr) return false;

  // based on the protocol type, we can determine
  // if this is a bpdu packet type
  switch (m_protocol)
  {
  case EHS_PROTOCOL__PDSS_PAYLOAD_BPDU:
    return true;

  case EHS_PROTOCOL__TDM_TELEMETRY:
  case EHS_PROTOCOL__NASCOM_TELEMETRY:
  case EHS_PROTOCOL__PSEUDO_TELEMETRY:
  case EHS_PROTOCOL__TDS_DATA:
  case EHS_PROTOCOL__TEST_DATA:
  case EHS_PROTOCOL__GSE_DATA:
  case EHS_PROTOCOL__CUSTOM_DATA:
  case EHS_PROTOCOL__HDRS_DQ:
  case EHS_PROTOCOL__CSS:
  case EHS_PROTOCOL__AOS_LOS:
  case EHS_PROTOCOL__PDSS_PAYLOAD_CCSDS_PACKET:
  case EHS_PROTOCOL__PDSS_CORE_CCSDS_PACKET:
  case EHS_PROTOCOL__PDSS_UDSM:
  case EHS_PROTOCOL__PDSS_RPSM:
  default:
    return false;

  }

  return false;

}


// determine packet type
bool PacketParser::isccsds()
{
  if (! m_pktptr) return false;

  // based on the protocol type, we can determine
  // if this is a ccsds packet type
  switch (m_protocol)
  {
  case EHS_PROTOCOL__GSE_DATA:
  case EHS_PROTOCOL__AOS_LOS:
  case EHS_PROTOCOL__PDSS_PAYLOAD_CCSDS_PACKET:
  case EHS_PROTOCOL__PDSS_CORE_CCSDS_PACKET:
  case EHS_PROTOCOL__PDSS_UDSM:
  case EHS_PROTOCOL__TDM_TELEMETRY:   // yes, has a ccsds header
  case EHS_PROTOCOL__PSEUDO_TELEMETRY:  // yes, has a ccsds header
    return true;

  case EHS_PROTOCOL__NASCOM_TELEMETRY:
  case EHS_PROTOCOL__TDS_DATA:
  case EHS_PROTOCOL__TEST_DATA:
  case EHS_PROTOCOL__CUSTOM_DATA:
  case EHS_PROTOCOL__HDRS_DQ:
  case EHS_PROTOCOL__CSS:
  case EHS_PROTOCOL__PDSS_PAYLOAD_BPDU:
  case EHS_PROTOCOL__PDSS_RPSM:
  default:
    return false;

  }

  return false;

}


// method to extract bpdu type bit from ehs secondary header
bool PacketParser::getbpdutype()
{
  if (! m_pktptr) return false;

  // based on the protocol type, we can extract the bpdu type bit
  switch (m_protocol)
  {
  case EHS_PROTOCOL__PDSS_PAYLOAD_CCSDS_PACKET:
    return ( m_headers.std.secondary.payload.Data_Stream_ID == EHS_PDSS_CCSDS_BPDU_TYPE__BPDU );

  case EHS_PROTOCOL__PDSS_CORE_CCSDS_PACKET:
    return ( ((m_headers.std.secondary.core.Data_Stream_ID & 0x80000000) >> 31) == EHS_PDSS_CCSDS_BPDU_TYPE__BPDU );

  case EHS_PROTOCOL__PDSS_PAYLOAD_BPDU:
    return ( m_headers.std.secondary.bpdu.Data_Stream_ID == EHS_PDSS_CCSDS_BPDU_TYPE__BPDU );

  case EHS_PROTOCOL__PDSS_UDSM:
    return ( m_headers.std.secondary.udsm.Data_Stream_ID == EHS_PDSS_CCSDS_BPDU_TYPE__BPDU );


  case EHS_PROTOCOL__TDM_TELEMETRY:
  case EHS_PROTOCOL__NASCOM_TELEMETRY:
  case EHS_PROTOCOL__PSEUDO_TELEMETRY:
  case EHS_PROTOCOL__TDS_DATA:
  case EHS_PROTOCOL__TEST_DATA:
  case EHS_PROTOCOL__GSE_DATA:
  case EHS_PROTOCOL__CUSTOM_DATA:
  case EHS_PROTOCOL__HDRS_DQ:
  case EHS_PROTOCOL__CSS:
  case EHS_PROTOCOL__AOS_LOS:
  case EHS_PROTOCOL__PDSS_RPSM:
  default:
    return false;

  }

  return false;

}


// get apid extension (for nrt)
int PacketParser::getapidextension()
{
  if (! m_pktptr) return 0;

  // if this is not a CCSDS packet type look no further
  if (! isccsds() ) return 0;

  // make sure we do have a secondary ccsds header
  if ( ! ( getccsdssecondaryheaderflag() > 0 )  ) return 0;

  // make sure this is a core type packet
  if ( gettypebit() != CORE_TYPE ) return 0;

  return m_headers.std.ccsds.core.CCSDS_Extended_Format_ID;

}


// get zoe tlm bit
int PacketParser::getzoetlm()
{
  if (! m_pktptr) return 0;

  // if this is not a CCSDS packet type look no further
  if (! isccsds() ) return 0;

  // make sure we do have a secondary ccsds header
  if ( ! ( getccsdssecondaryheaderflag() > 0 )  ) return 0;

  // extract zoe tlm bit according to packet type
  if ( gettypebit() == CORE_TYPE )
    // why didn't we call this ZOE_Tlm as well?  i'm not sure,
    // i just know it was defined this way once upon a time
    // in the original EHS.h file...
    return m_headers.std.ccsds.core.CCSDS_Spare_1;
  else
    return m_headers.std.ccsds.payload.ZOE_Tlm;

}


// *** other header info ***


// *** CCSDS header info ***

// get the CCSDS sequence counter flags the primary CCSDS header,
int PacketParser::getccsdsseqflags()
{
  unsigned char* ptr = NULL; // for tdm packet parsing
  int seqflags = 0;

  if (! m_pktptr) return -1;

  // if this is not a CCSDS packet type look no further
  if (! isccsds() ) return -1;

  int cp = getcorepayload();

  switch (m_protocol)
  {
  case EHS_PROTOCOL__PSEUDO_TELEMETRY: // like tdm - +PrimHdr + SecHdr + 2
    // PSEUDO FALLS THROUGH TO TDM CODE!!
  case EHS_PROTOCOL__TDM_TELEMETRY:
    ptr = (unsigned char *)
           ( 
            m_pktptr +  // start of packet
            EHSPrimaryHeaderSize() +  // skip primary header
            EHSSecondaryHeaderSize() + // skip variable length 2ndary header
            2 // sequence flags are in byte 2 of ccsds primary header
           ); // now pointing at byte 2 of ccsds header (0-5)
    // sequence flag is bits 7-6 of byte 2, 11000000
    seqflags = (unsigned char)((unsigned char)(*ptr) & 0xc0); // get bits 7-6 - 11000000
    seqflags >>= 6;  // shift 6 bits to the right to justify the output value
    return seqflags;

  case EHS_PROTOCOL__GSE_DATA:
    if ( cp == PacketParser::PAYLOAD_TYPE )
    {
      return m_headers.nonstd1.ccsds.gse.payload.CCSDS_Sequence_Flags;
    }
    else
    {
      return m_headers.nonstd1.ccsds.gse.core.CCSDS_Sequence_Flags;
    }

  default:
    if ( cp == PacketParser::PAYLOAD_TYPE )
    {
      return m_headers.std.ccsds.payload.CCSDS_Sequence_Flags;
    }
    else
    {
      return m_headers.std.ccsds.core.CCSDS_Sequence_Flags;
    }
  }

}


// get the CCSDS sequence counter from the primary CCSDS header,
// or -1 if not available.  this is actually a 14-bit counter
// ranging from 0 to 16383, which of course can wrap around.
int PacketParser::getccsdsseqno()
{
  unsigned char* ptr = NULL; // for tdm packet parsing
  int seqno = 0; // the returned sequence number, initialized to 0 upon declaration

  if (! m_pktptr) return -1;

  // if this is not a CCSDS packet type look no further
  if (! isccsds() ) return -1;

  int cp = getcorepayload();

  switch (m_protocol)
  {
  case EHS_PROTOCOL__PSEUDO_TELEMETRY: // like tdm - +PrimHdr + SecHdr + 2
    // PSEUDO FALLS THROUGH TO TDM CODE!!
  case EHS_PROTOCOL__TDM_TELEMETRY:
    ptr = (unsigned char *)
           ( 
            m_pktptr +  // start of packet
            EHSPrimaryHeaderSize() +  // skip primary header
            EHSSecondaryHeaderSize() + // skip variable length 2ndary header
            2 // sequence number is bytes 2 and 3 of ccsds primary header
           ); // now pointing at byte 2 of ccsds header (0-5)
    // sequence number is bits 5-0 of byte 2, 7-0 of byte 3 so mask with 00111111 11111111
    seqno = 0; // clear it all out
    seqno |= (unsigned char)((unsigned char)(*ptr) & 0x3f); // get bits 5-0  00111111
    seqno = seqno << 8;    // shift 8
    ptr++;        // point to next byte ( byte 3 )
    seqno |= (*ptr);  // get bits 7-0 11111111
    return seqno;

  case EHS_PROTOCOL__GSE_DATA:
    if ( cp == PacketParser::PAYLOAD_TYPE )
    {
      return m_headers.nonstd1.ccsds.gse.payload.CCSDS_Sequence_Count;
    }
    else
    {
      return m_headers.nonstd1.ccsds.gse.core.CCSDS_Sequence_Count;
    }

  default:
    if ( cp == PacketParser::PAYLOAD_TYPE )
    {
      return m_headers.std.ccsds.payload.CCSDS_Sequence_Count;
    }
    else
    {
      return m_headers.std.ccsds.core.CCSDS_Sequence_Count;
    }
  }

}


// get the CCSDS secondary header flag
int PacketParser::getccsdssecondaryheaderflag()
{
  if (! m_pktptr) return -1;

  // if this is not a CCSDS packet type look no further
  if (! isccsds() ) return -1;

  int cp = getcorepayload();

  switch (m_protocol)
  {
  case EHS_PROTOCOL__TDM_TELEMETRY: // tdm has no secondary ccsds header
    return 0; // zero by definition

  case EHS_PROTOCOL__PSEUDO_TELEMETRY: // PSEUDO has no secondary ccsds header
    return 0; // zero by definition

  case EHS_PROTOCOL__GSE_DATA:
    if ( cp == PacketParser::PAYLOAD_TYPE )
    {
      return m_headers.nonstd1.ccsds.gse.payload.CCSDS_Secondary_Header_Flag;
    }
    else
    {
      return m_headers.nonstd1.ccsds.gse.core.CCSDS_Secondary_Header_Flag;
    }

  default:
    if ( cp == PacketParser::PAYLOAD_TYPE )
    {
      return m_headers.std.ccsds.payload.CCSDS_Secondary_Header_Flag;
    }
    else
    {
      return m_headers.std.ccsds.core.CCSDS_Secondary_Header_Flag;
    }
  }

}


// get the CCSDS packet length
int PacketParser::getccsdspktlen()
{
  int pktlen = 0; // will contain the packet length, initialized to zero upon declaration
  unsigned char* ptr = NULL; // for parsing through TDM packet

  if (! m_pktptr) return -1;

  // if this is not a CCSDS packet type look no further
  if (! isccsds() ) return -1;

  int cp = getcorepayload();

  switch (m_protocol)
  {
  case EHS_PROTOCOL__PSEUDO_TELEMETRY: // like TDM_Telemetry - +PrimHdr+SecHdr+4
    // PSEUDO FALLS THROUGH TO TDM CODE!!
  case EHS_PROTOCOL__TDM_TELEMETRY: 
    ptr = (unsigned char *)
           ( 
            m_pktptr +  // start of packet
            EHSPrimaryHeaderSize() +  // skip primary header
            EHSSecondaryHeaderSize() + // skip variable length 2ndary header
            4 // packet length is bytes 4 and 5 of ccsds primary header
           ); // now pointing at byte 4 of ccsds header (0-5)
    // sequence number is all bits of bytes 4 and 5
    pktlen = 0; // clear all the bits out
    pktlen |= (unsigned char)(*ptr); // get bits 7-0 
    pktlen = pktlen << 8;    // shift 8
    ptr++;        // point to next byte (byte 5)
    pktlen |= (*ptr);  // get bits 7-0 
    //
    //  the requirements say that this pktlen is one less than the actual data bytecount.
    //  if the data zone is empty (has an actual length of 0), then this number 
    //  (the pktlen) SHOULD be -1 to follow the rules, but that's impossible, so
    //  we hardcode an exception here for TDM LOS packets (and TDM LOS packets ONLY)
    //  that if the pktlen is zero, and it's a TDM LOS packet, return pktlen=0, else return pktlen+1.
    // 
    // PSEUDO code falls through here also - make sure you're checking a TDM packet here.
    if ( pktlen == 0 && m_protocol == EHS_PROTOCOL__TDM_TELEMETRY) 
    {
      // can't check for LOS, but mark holbrook said that a TDM packet
      // will never have a data field of ONE byte so you can assume
      // a pktlen=0 is ok.
      // do NOTHING with pktlen - leave it = 0 if you're here.
    }
    else
    {
      pktlen = pktlen + 1; // add one to be consistent with everybody else.
    }
    
    return pktlen; 

  case EHS_PROTOCOL__GSE_DATA:
    if ( cp == PacketParser::PAYLOAD_TYPE )
    {
      return 1 + m_headers.nonstd1.ccsds.gse.payload.CCSDS_Packet_Length;
    }
    else
    {
      return 1 + m_headers.nonstd1.ccsds.gse.core.CCSDS_Packet_Length;
    }

  default:
    if ( cp == PacketParser::PAYLOAD_TYPE )
    {
      return 1 + m_headers.std.ccsds.payload.CCSDS_Packet_Length;
    }
    else
    {
      return 1 + m_headers.std.ccsds.core.CCSDS_Packet_Length;
    }
  }

}


// get "embedded" time.  method returns a pointer to the 40-bit
// packet "embedded time" from the secondary CCSDS header, if
// available, or NULL if not.
const unsigned char* PacketParser::getemb()
{
  // pointer to find embedded time in TDM packets
  unsigned char *cp = NULL; 

  // flag to say CNT/MET time exists in TDM packet.
  bool cntmet_exists = false;  // default to false

  // flag to say OBT (onboard time) exists in TDM packet.
  // OBT is interpreted as 'embedded time' here.
  bool OBT_exists = false;  // default to false

  if (! m_pktptr) return NULL;

  // if this is not a CCSDS packet type look no further
  if (! isccsds() ) return NULL;

  // if this is a GSE packet type look no further.  there is no
  // embedded time in the GSE ccsds secondary header, which has
  // a different format than the PDSS_CORE and PDSS_PAYLOAD
  // ccsds packet types.
  //
  // also, PSEUDO packets have no embedded time
  if (m_protocol == EHS_PROTOCOL__GSE_DATA || 
      m_protocol == EHS_PROTOCOL__PSEUDO_TELEMETRY) return NULL;

  // TDM is a REALLY SPECIAL CASE!!! - embedded time is in 
  // the secondary header if it's there at all, and it 
  // MOVES based on whether cnt/met time exists in the packet
  // ahead of it. so find if-and-where it is and return the address 
  // from this if() statement - don't fall through.
  // EVERYTHING YOU NEED TO DO FOR TDM PACKETS IS DONE IN THIS 
  // if() STATEMENT.
  if ( m_protocol == EHS_PROTOCOL__TDM_TELEMETRY) // the aforementioned if() statement
  {
    unsigned int coarsetimeseconds = 0; // for embedded time

    // gotta look at some flags to determine where in cleveland
    // the embedded time is, if it's in the tdm packet at all.
    // the flags are in the "optional trailer field flags" byte,
    // which is the last byte of the 20-byte tdm 2ndary header.
    // do some math and get cp pointing at it.
    cp = (unsigned char *)(m_pktptr + // the start of this packet
         EHSPrimaryHeaderSize() + // skip primary header 
         19); // point at the last byte of the 20-byte tdm 2ndary header.

    // !!! OH! OH! OH!
    // note - you can't use "EHSSecondaryHeaderSize()-1" to find
    // the last byte in the secondary header (instead of 
    // the hardcoded "19") here because EHSSecondaryHeaderSize()
    // returns the length of the *entire* secondary header 
    // INCLUDING optional fields, not just the 20-byte info header.
    // the 19 is the last byte of the 20-byte info header, which
    // is the optional trailer field flags byte.
    //
    // note that right here, cp is pointing at the last byte of 
    // the 20-byte tdm secondary header, 
    // ***NOT*** the byte AFTER the header. so be sure to 
    // get your flags copied BEFORE moving the pointer.
    //
    // you GOTTA cast these ANDs as "(unsigned char)" or else they get converted to ints and don't work right
    if ((unsigned char)(*cp & 0x40) == (unsigned char)(0x40)) // if OBT (embedded time) exists
    {
      OBT_exists = true; // set this flag
    }
    else // if there's no OBT, return immediately
    {
      return NULL; // say there was no embedded time; no need to go further
    }

    if ((unsigned char)(*cp & 0x80) == (unsigned char)(0x80)) // if CNT/MET time exists
    {
      cntmet_exists = true; // set this flag
    }

    // add 1 to get past the tdm secondary header 
    cp += 1; // now pointing at 1st byte AFTER tdm 2ndary header

    // if there was a CNT/MET field, skip it
    if ( cntmet_exists == true )
    {
      // skip the 7-byte CNT/MET field
      cp += 7;
    }

    // now since you've already checked that OBT_exists,
    // you're pointing at the first byte of the OBT field.
    // it's up to the calling routine to only nab the
    // 7 bytes that make up the embedded time

    // since here in the TDM packet the format is (Year) (day)(day) (hour) (min) (sec) (fraction-of-a-second-in-MSnibble)
    // it has to be converted to embedded time format of 4-byte coarse time and 1-byte fine time.
    // the coarse time is seconds since midnight jan 6, 1980 (the zeroth second of jan 6, 1980).

    EHSTime et(cp);
    coarsetimeseconds = et.getcoarsetime();
    //copy coarse time seconds into 1st 4 bytes of embedded time buffer
    memcpy(&m_TDM_EmbeddedTime_buffer[0],&coarsetimeseconds, 4);  // copy four bytes of integer
    byte_swap4 ( (unsigned char *)&m_TDM_EmbeddedTime_buffer, 4); // byte swap if necessary

    m_TDM_EmbeddedTime_buffer[4] = et.getfinetime();
    

    // now we have a 4-byte int and a 1-byte finetime byte.
    // we don't care about the extra flags
   
    // return the member variable.
    // CALLING ROUTINE MUST USE THIS ADDRESS AND MAKE AN EmbeddedTime object
    return m_TDM_EmbeddedTime_buffer;

    // note that in NO circumstance do you fall through
    // to the next if() below here.
  }
  
  // if there is no secondary header or if we don't know 
  // then say we have no embedded time
  if (getccsdssecondaryheaderflag() <= 0  )
  {
    return NULL;
  }

  // see "getgrt" for explanation of why this is "hard coded"
  // to the exact location within the header
  //
  // bump past all the other headers to get to the CCSDS secondary
  // header, then we will tack on the "hard coded" offset to the
  // time, which it so happens, is located at the very beginning
  // of the secondary header, for both "core" and "payload" packet
  // types, hence the "+0" (easier to figure out and change later).

  return (const unsigned char *)( m_pktptr + EHSPrimaryHeaderSize() +
    EHSSecondaryHeaderSize() + CCSDSPrimaryHeaderSize() + 0 );

}


// get gse format id
int PacketParser::getgseformatid()
{
  if (! m_pktptr) return -1;

  // if this is not a GSE packet type then forget it
  if (m_protocol != EHS_PROTOCOL__GSE_DATA)
  {
    return -1;
  }

  // if this is not a CCSDS packet type look no further
  if (! isccsds() ) return -1;

  // if there is no secondary header then we have no format id
  if (! m_headers.nonstd1.ccsds.gse.core.CCSDS_Secondary_Header_Flag)
  {
    return -1;
  }

  // depending on the core/payload flag to determine which
  // header format to choose, return the gse format id
  if ( (int)m_headers.nonstd1.secondary.gse.Payload_vs_Core_ID == CORE_TYPE )
  {
    return m_headers.nonstd1.ccsds.gse.core.CCSDS_GSE_Format_Id;
  }
  else
  {
    return m_headers.nonstd1.ccsds.gse.payload.CCSDS_GSE_Format_Id;
  }

}


// get tdm format id
int PacketParser::gettdmformatid()
{
  if (! m_pktptr) return 0;

  // if this is not a TDM packet type then forget it
  if ( m_protocol != EHS_PROTOCOL__TDM_TELEMETRY )
  {
    return 0;
  }

  // pointer to location of format id in 20-byte TDM secondary header
  unsigned char *ptr = NULL; 

  ptr = (unsigned char *) ( m_pktptr +                // the start of this packet
                            EHSPrimaryHeaderSize() +  // skip primary ehs header 
                            15 );                     // point at the 15th byte (0-relative) of the 20-byte TDM secondary header

  // return the tdm format id
  return (int)*ptr;

}


// get AOS/LOS status from an AOS/LOS packet
// where AOS=1 LOS=0 undefined=-1
int PacketParser::getaoslos()
{
  if (! m_pktptr) return -1;

  const unsigned char* dataptr = NULL;

  switch (m_protocol)
  {
  case EHS_PROTOCOL__AOS_LOS:
    // get the actual data pointer of this packet
    dataptr = getdataptr();

    // the AOS/LOS status bit is the least significant bit
    // of the first data byte
    return (*dataptr & 0x01);

  default:
    return -1;
  }

}


// get EHS primary header size
int PacketParser::EHSPrimaryHeaderSize()
{
  return sizeof(EHS_Protocol_Header_Type);
}


// get EHS secondary header size
int PacketParser::EHSSecondaryHeaderSize()
{
  unsigned tdmsecondaryheaderlength = 0; // default to zero
  unsigned char* ptr = NULL;  // for tdm secondary header calculation

  if (! m_pktptr) return 0;

  // based on the protocol type we can determine
  // the secondary header size
  //
  // NOTE: values which were unknown to the author
  // are returned as zero to maintain a placeholder
  // for future updates if necessary.
  switch (m_protocol)
  {
  case EHS_PROTOCOL__TDM_TELEMETRY:
    // the secondary header of a TDM packet is a VARIABLE LENGTH and 
    // can include the length of a varying number of  optional trailer fields.
    // the length of the secondary header is given in the first two bytes of the header.
    //
    // point at the first byte of the 2ndary header
    ptr = (unsigned char *) (m_pktptr + EHSPrimaryHeaderSize()); 
    // secondary header length is in the first sixteen bits of the secondary header
    tdmsecondaryheaderlength |= (*ptr); // OR in the first byte
    ptr++; // point at the next byte in the packet
    tdmsecondaryheaderlength = tdmsecondaryheaderlength << 8; // multiply by 256 for 2nd byte
    tdmsecondaryheaderlength |= (*ptr); // OR in the second byte
    return (int)tdmsecondaryheaderlength;  // oo ah - there's your total length

  case EHS_PROTOCOL__NASCOM_TELEMETRY:
    return 0;

  case EHS_PROTOCOL__PSEUDO_TELEMETRY:
    return 8; // return the length of the pseudo header

  case EHS_PROTOCOL__TDS_DATA:
    return 0;

  case EHS_PROTOCOL__TEST_DATA:
    return 0;

  case EHS_PROTOCOL__GSE_DATA:
    return sizeof(EHS_GSE_Secondary_Protocol_Type);

  case EHS_PROTOCOL__CUSTOM_DATA:
    return 0;

  case EHS_PROTOCOL__HDRS_DQ:
    return 0;

  case EHS_PROTOCOL__CSS:
    return 0;

  case EHS_PROTOCOL__AOS_LOS:
    return sizeof(EHS_PDSS_AOS_LOS_Secondary_Protocol_Type);

  case EHS_PROTOCOL__PDSS_PAYLOAD_CCSDS_PACKET:
    return sizeof(EHS_PDSS_Payload_Secondary_Protocol_Type);

  case EHS_PROTOCOL__PDSS_CORE_CCSDS_PACKET:
    return sizeof(EHS_PDSS_Core_Secondary_Protocol_Type);

  case EHS_PROTOCOL__PDSS_PAYLOAD_BPDU:
    return sizeof(EHS_PDSS_BPDU_Secondary_Protocol_Type);

  case EHS_PROTOCOL__PDSS_UDSM:
    return sizeof(EHS_PDSS_UDSM_Secondary_Protocol_Type);

  case EHS_PROTOCOL__PDSS_RPSM:
    return 0;

  default:
    return 0;

  }

}


// get CCSDS primary header size
int PacketParser::CCSDSPrimaryHeaderSize()
{
  return EHS_CCSDS_PRIMARY_HEADER_SIZE;
}


// get CCSDS secondary header size
int PacketParser::CCSDSSecondaryHeaderSize()
{
  if (! m_pktptr) return 0;
  if ( ! isccsds() ) return 0;

  switch (m_protocol)
  {
  case EHS_PROTOCOL__GSE_DATA:
    return EHS_CCSDS_SECONDARY_HEADER_SIZE_GSE;
  case EHS_PROTOCOL__TDM_TELEMETRY:
    // TDM packets don't have a ccsds secondary header 
    return 0; 
  case EHS_PROTOCOL__PSEUDO_TELEMETRY:
    // PDSEUDO  packets don't have a ccsds secondary header 
    return 0; 
  default:
    return EHS_CCSDS_SECONDARY_HEADER_SIZE;
  }

}


// copy header data to internal structures.  this is necessary
// since we have no idea what kind of byte boundary each packet
// will start on, which depends on the size of the previous packet.
// this can cause "Bus Error" type of errors if you try to access
// this data with a structure pointer of some sort that does not
// start on even word boundaries.
void PacketParser::copyheaders()
{
  // check for stupid mistakes
  if (! m_pktptr)
  {
    m_protocol = 0;
    memset ( (char *)&m_headers, 0, sizeof(m_headers) );
    return;
  }

  // copy the header data
  memcpy ( (char *)&m_headers,
    (const char *)m_pktptr, sizeof(m_headers) );

  // perform 4-byte integer byte swapping if necessary
  byte_swap4 ( (unsigned char *)&m_headers, sizeof(m_headers) );

  // this value is needed by most methods so lets just fetch it once
  m_protocol = getprotocol();

}


// determine if the potential data packet pointer is within the
// bounds of the frame size.  if not, this is a partial packet
// at the end of a frame buffer and should not be processed.
bool PacketParser::withinbounds ( const unsigned char* pktptr )
{
  if (! m_startptr) return false;

  // figure the amount of space left in the buffer
  int remainder = m_framesize -
    ((long)pktptr - (long)m_startptr);

  // the remaining space must at least encompass a primary EHS header
  if (remainder < EHSPrimaryHeaderSize()) return false;

  // since we know there is at least a primary header, we can now
  // get the packet length and see if we are within the bounds
  // of the remaining space in the frame buffer
  EHS_Primary_Header_Type primaryhdr;
  memcpy (&primaryhdr, pktptr, sizeof(primaryhdr));

  // perform 4-byte integer byte swapping if necessary
  byte_swap4 ( (unsigned char *)&primaryhdr, sizeof(primaryhdr) );

  int pktsize = (int)primaryhdr.HOSC_Packet_Size;
  int protocol = (int)primaryhdr.Protocol_Type;

  // check for an invalid packet length
  if (! validate_pktlen (pktsize, protocol) )
  {
    m_invalid_pktlen = pktsize;
    m_invalid_pktlen_detected = true;
    return false;
  }

  if (remainder < pktsize) return false;

  // not a partial packet, ok to process
  return true;

}

