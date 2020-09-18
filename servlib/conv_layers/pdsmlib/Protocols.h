
#ifndef __PROTOCOLS_H__
#define __PROTOCOLS_H__

#if !defined(_REENTRANT)
#  define _REENTRANT
#endif 

#if defined(__linux)
#  include <endian.h>
#endif

#if defined(_REENTRANT)
#  undef _REENTRANT
#endif 

// this file contains the definitions of the data
// structures and constants for the various EHS
// packet protocols, CCSDS packet headers, and so on.

// IMPORTANT NOTE (for Intel based byte-swapped machines):
//
// on LITTLE_ENDIAN machines bit fields are somewhat
// tricky.  if your bit fields do NOT cross 4-byte word
// boundaries, you can byte swap your data, and apply
// the bit fields in reverse order (per 4-byte word)
// and it will work.  otherwise, you better get real
// well acquainted with the -shift-, -and-, -or-
// operators.  sorry, but thats about all you can do.
//
// fortunately, all of the EHS and CCSDS headers do
// qualify for these conditions (unlike the VCDU
// data structures).  therefore, this file contains
// definitions for both big and little endian for
// each protocol type.

// true size in bytes of various header types
//
// due to padding by the compiler, the sizeof()
// operator does not always return a true value
typedef enum EHS_Header_Sizes_Type
{
  EHS_PRIMARY_HEADER_SIZE = 16,
  EHS_SECONDARY_HEADER_SIZE = 12,
  EHS_TOTAL_HEADER_SIZE =
    EHS_PRIMARY_HEADER_SIZE + EHS_SECONDARY_HEADER_SIZE,

  EHS_SECONDARY_HEADER_SIZE_GSE = 16,

  EHS_CCSDS_PRIMARY_HEADER_SIZE = 6,

  EHS_CCSDS_SECONDARY_HEADER_SIZE = 10,
  EHS_CCSDS_TOTAL_HEADER_SIZE =
    EHS_CCSDS_PRIMARY_HEADER_SIZE + EHS_CCSDS_SECONDARY_HEADER_SIZE,

  EHS_CCSDS_SECONDARY_HEADER_SIZE_GSE = 1,
  EHS_CCSDS_TOTAL_HEADER_SIZE_GSE =
    EHS_CCSDS_PRIMARY_HEADER_SIZE + EHS_CCSDS_SECONDARY_HEADER_SIZE_GSE,

  EHS_PDSS_AOS_LOS_DATA_ZONE_SIZE = 2,
  EHS_PDSS_UDSM_DATA_ZONE_SIZE    = 28,
  EHS_TIME_TYPE_SIZE = 7
} EHS_Header_Sizes_Type_t;


// values for the EHS version number in the primary EHS header
typedef enum EHS_Version_Number
{
  EHS_VERSION__ALL_PROJECTS,
  EHS_VERSION__VERSION1,
  EHS_VERSION__VERSION2,
  NUMBER_EHS_VERSIONS = 2,
  EHS_VERSION__DEFAULT = EHS_VERSION__VERSION2
} EHS_Version_Number_t;


// values for the project ID in the primary EHS header
typedef enum EHS_Project_Type
{
  EHS_PROJECT__ALL_PROJECTS,
  EHS_PROJECT__STS,
  EHS_PROJECT__SL,
  EHS_PROJECT__ISS,
  EHS_PROJECT__AXAF,
  EHS_PROJECT__CXP,
  NUMBER_PROJECTS = 5
} EHS_Project_Type_t;


// values for the support mode in the EHS primary header
typedef enum EHS_Support_Mode_Type
{
  EHS_SUPPORT_MODE__ALL_SUPPORT_MODES, 
  EHS_SUPPORT_MODE__FLIGHT,
  EHS_SUPPORT_MODE__GROUND_TEST,
  EHS_SUPPORT_MODE__SIMULATION,
  EHS_SUPPORT_MODE__VALIDATION,
  EHS_SUPPORT_MODE__DEVELOPMENT,
  EHS_SUPPORT_MODE__TRAINING, 
  EHS_SUPPORT_MODE__OFFLINE,
  NUMBER_SUPPORT_MODES = 7
} EHS_Support_Mode_Type_t;


// values for the data mode in the EHS primary header
typedef enum EHS_Data_Mode_Type
{
  EHS_DATA_MODE__ALL_DATA_MODES,
  EHS_DATA_MODE__REALTIME,
  EHS_DATA_MODE__DUMP1,
  EHS_DATA_MODE__DUMP2,
  EHS_DATA_MODE__DUMP3,
  EHS_DATA_MODE__PLAYBACK1,
  EHS_DATA_MODE__PLAYBACK2,
  EHS_DATA_MODE__PLAYBACK3,
  EHS_DATA_MODE__PLAYBACK4,
  EHS_DATA_MODE__PLAYBACK5,
  EHS_DATA_MODE__PLAYBACK6,
  EHS_DATA_MODE__PLAYBACK7,
  EHS_DATA_MODE__PLAYBACK8,
  EHS_DATA_MODE__PLAYBACK9,
  EHS_DATA_MODE__PLAYBACK10,
  EHS_DATA_MODE__PLAYBACK11,
  EHS_DATA_MODE__MODE_INDEPENDENT = 0xFFFF,
  NUMBER_DATA_MODES = 16,
  NUMBER_DUMP_DATA_MODES = 3,
  NUMBER_STATIC_PB_DATA_MODES = 6,
  NUMBER_DYNAMIC_PB_DATA_MODES = 5
} EHS_Data_Mode_Type_t;

typedef enum EHS_Min_Max_Data_Mode_Type
{
  EHS_MIN_DUMP_DATA_MODE          = EHS_DATA_MODE__DUMP1,
  EHS_MAX_DUMP_DATA_MODE          = EHS_DATA_MODE__DUMP3,
  EHS_MIN_STATIC_PB_DATA_MODE     = EHS_DATA_MODE__PLAYBACK1,
  EHS_MAX_STATIC_PB_DATA_MODE     = EHS_DATA_MODE__PLAYBACK6,
  EHS_MIN_DYNAMIC_PB_DATA_MODE    = EHS_DATA_MODE__PLAYBACK7,
  EHS_MAX_DYNAMIC_PB_DATA_MODE    = EHS_DATA_MODE__PLAYBACK11
} EHS_Min_Max_Data_Mode_Type_t;


// values for the protocol type in the EHS primary header
typedef enum EHS_Protocol_Type
{
  EHS_PROTOCOL__ALL_PROTOCOLS,
  EHS_PROTOCOL__TDM_TELEMETRY,
  EHS_PROTOCOL__NASCOM_TELEMETRY,
  EHS_PROTOCOL__PSEUDO_TELEMETRY,
  EHS_PROTOCOL__TDS_DATA,
  EHS_PROTOCOL__TEST_DATA,
  EHS_PROTOCOL__GSE_DATA,
  EHS_PROTOCOL__CUSTOM_DATA,
  EHS_PROTOCOL__HDRS_DQ,
  EHS_PROTOCOL__CSS,
  EHS_PROTOCOL__AOS_LOS,
  EHS_PROTOCOL__PDSS_PAYLOAD_CCSDS_PACKET,
  EHS_PROTOCOL__PDSS_CORE_CCSDS_PACKET,
  EHS_PROTOCOL__PDSS_PAYLOAD_BPDU,
  EHS_PROTOCOL__PDSS_UDSM,
  EHS_PROTOCOL__PDSS_RPSM,
  NUMBER_PROTOCOLS = 15
} EHS_Protocol_Type_t;


// values for the AOS/LOS status in the AOS/LOS status packets
typedef enum EHS_PDSS_AOS_LOS_Status_Type
{
  EHS_PDSS_AOS_LOS_STATUS__LOS = 0,
  EHS_PDSS_AOS_LOS_STATUS__AOS = 1
} EHS_PDSS_AOS_LOS_Status_Type_t;


// values for the CORE/PAYLOAD type in the EHS secondary header
typedef enum EHS_PDSS_Core_Payload_Data_Type_Type
{
  EHS_PDSS_CORE_PAYLOAD_DATA_TYPE__CORE = 0,
  EHS_PDSS_CORE_PAYLOAD_DATA_TYPE__PAYLOAD = 1,
  NUMBER_DATA_TYPES = 2
} EHS_PDSS_Core_Payload_Data_Type_Type_t;


// values for the CCSDS/BPDU type in the EHS secondary header
typedef enum EHS_PDSS_Ccsds_Bpdu_Type
{
  EHS_PDSS_CCSDS_BPDU_TYPE__CCSDS = 0,
  EHS_PDSS_CCSDS_BPDU_TYPE__BPDU = 1
} EHS_PDSS_Ccsds_Bpdu_Type_t;


// values for data status of the PDSS Secondary Protocol Header
typedef enum EHS_PDSS_Parent_Stream_Error_Type
{
  EHS_PDSS_PARENT_STRM_ERROR__NO_ERROR = 0,
  EHS_PDSS_PARENT_STRM_ERROR__ERROR = 1
} EHS_PDSS_Parent_Stream_Error_Type_t;

typedef enum EHS_PDSS_VCDU_Sequence_Error_Type
{
  EHS_PDSS_VCDU_SEQ_ERROR__NO_ERROR = 0,
  EHS_PDSS_VCDU_SEQ_ERROR__ERROR = 1
} EHS_PDSS_VCDU_Sequence_Error_Type_t;

typedef enum EHS_PDSS_Pkt_Sequence_Error_Type
{
  EHS_PDSS_PKT_SEQ_ERROR__NO_ERROR = 0,
  EHS_PDSS_PKT_SEQ_ERROR__ERROR = 1
} EHS_PDSS_Pkt_Sequence_Error_Type_t;

typedef enum EHS_PDSS_GSE_Pkt_ID_Type
{
  EHS_PDSS_GSE_PKT_ID__NOT_GSE = 0,
  EHS_PDSS_GSE_PKT_ID__GSE = 1
} EHS_PDSS_GSE_Pkt_ID_Type_t;


// *************************************************************
// * This structure defines the EHS time format
// *************************************************************

typedef struct EHS_Time_Type
{
  // Numeric year as years since 1900
  unsigned char Year;
  // high 8 bits of 16 bit Julian day of year
  unsigned char Day_High;
  // low 8 bits of 16 bit Julian day of year
  unsigned char Day_Low;
  // hour of day
  unsigned char Hour;
  // minute of hour
  unsigned char Minute;
  // second of minute
  unsigned char Second;
  // tenth of seconds in 4 MSB's and time flags in 4 LSB's
  unsigned char Tenths_Flags;
} EHS_Time_Type_t;


// *************************************************************
// * EHS Protocol Primary Header Structure
// *************************************************************

typedef struct EHS_Protocol_Header_Type
{
#if BYTE_ORDER == BIG_ENDIAN

  unsigned Version : 4;
  unsigned Project : 4;
  unsigned Support_Mode : 4;
  unsigned Data_Mode : 4;
  unsigned Mission : 8;
  unsigned Protocol_Type : 8;

  // Numeric year as years since 1900
  unsigned Year : 8;
  // Julian day of year
  unsigned Day_High : 8;
  unsigned Day_Low : 8;
  // hour of day
  unsigned Hour : 8;
  // minute of hour
  unsigned Minute : 8;
  // second of minute
  unsigned Second : 8;
  // tenths of second
  unsigned Tenths : 4;
  // indicates the time has changed
  unsigned New_Data_Flag : 1;

  unsigned Pad1 : 1;

  // indicates a hold condition for CNT time
  unsigned Hold_Flag : 1;
  // indicates pre-mission time
  unsigned Sign_Flag : 1;

  unsigned Pad2 : 8;
  unsigned Pad3 : 8;
  unsigned Pad4 : 8;

  unsigned HOSC_Packet_Size : 16;

#else

  // LITTLE_ENDIAN definitions (4-byte words must be
  // byte swapped before using bit fields)

  // word 0
  unsigned Protocol_Type : 8;
  unsigned Mission : 8;
  unsigned Data_Mode : 4;
  unsigned Support_Mode : 4;
  unsigned Project : 4;
  unsigned Version : 4;

  // word 1
  // hour of day
  unsigned Hour : 8;
  // Julian day of year
  unsigned Day_Low : 8;
  unsigned Day_High : 8;
  // Numeric year as years since 1900
  unsigned Year : 8;

  // word 2
  unsigned Pad2 : 8;

  // indicates pre-mission time
  unsigned Sign_Flag : 1;

  // indicates a hold condition for CNT time
  unsigned Hold_Flag : 1;

  unsigned Pad1 : 1;

  // indicates the time has changed
  unsigned New_Data_Flag : 1;
  // tenths of second
  unsigned Tenths : 4;
  // second of minute
  unsigned Second : 8;
  // minute of hour
  unsigned Minute : 8;

  // word 3
  unsigned HOSC_Packet_Size : 16;

  unsigned Pad4 : 8;
  unsigned Pad3 : 8;

#endif

} EHS_Protocol_Header_Type_t;

// never understood why this one was not just called this instead,
// so from now on it can be
typedef EHS_Protocol_Header_Type_t EHS_Primary_Header_Type;
typedef EHS_Protocol_Header_Type_t EHS_Primary_Header_Type_t;


// ****************************************************************
// * PDSS Core Telemetry Secondary Protocol Header Structure  
// ****************************************************************

typedef struct EHS_PDSS_Core_Secondary_Protocol_Type
{
#if BYTE_ORDER == BIG_ENDIAN

  // PDSS protocol version
  unsigned Version : 2;
  // Unused
  unsigned Data_Status_Bit_5 : 1;
  // Unused
  unsigned Data_Status_Bit_4 : 1;
  // Generated and used by EHS to indicate OIU errors
  unsigned Parent_Stream_Error : 1;
  // Indicates a VCDU Sequence Error occurred
  unsigned VCDU_Sequence_Error : 1;
  // Indicates a Packet Sequence Error occurred
  unsigned Packet_Sequence_Error : 1;
  // Unused
  unsigned Data_Status_Bit_0 : 1;
  // VCDU sequence number from VCDU header
  unsigned VCDU_Sequence_Number : 24;
  // Unique identifier for data stream
  unsigned Data_Stream_ID : 32;
  // PDSS internal routing ID
  unsigned Routing_ID : 16;
  // PDSS reserved sync pattern
  unsigned PDSS_Reserved_Sync : 16;

#else

  // LITTLE_ENDIAN definitions (4-byte words must be
  // byte swapped before using bit fields)

  // word 0
  // VCDU sequence number from VCDU header
  unsigned VCDU_Sequence_Number : 24;
  // Unused
  unsigned Data_Status_Bit_0 : 1;
  // Indicates a Packet Sequence Error occurred
  unsigned Packet_Sequence_Error : 1;
  // Indicates a VCDU Sequence Error occurred
  unsigned VCDU_Sequence_Error : 1;
  // Generated and used by EHS to indicate OIU errors
  unsigned Parent_Stream_Error : 1;
  // Unused
  unsigned Data_Status_Bit_4 : 1;
  // Unused
  unsigned Data_Status_Bit_5 : 1;
  // PDSS protocol version
  unsigned Version : 2;

  // word 1
  // Unique identifier for data stream
  unsigned Data_Stream_ID : 32;

  // word 2
  // PDSS reserved sync pattern
  unsigned PDSS_Reserved_Sync : 16;
  // PDSS internal routing ID
  unsigned Routing_ID : 16;

#endif

} EHS_PDSS_Core_Secondary_Protocol_Type_t;

typedef EHS_PDSS_Core_Secondary_Protocol_Type_t 
  EHS_PDSS_Core_Secondary_Header_Type;
typedef EHS_PDSS_Core_Secondary_Protocol_Type_t 
  EHS_PDSS_Core_Secondary_Header_Type_t;


// ****************************************************************
// * PDSS Payload Telemetry Secondary Protocol Header Structure  
// ****************************************************************

typedef struct EHS_PDSS_Payload_Secondary_Protocol_Type
{
#if BYTE_ORDER == BIG_ENDIAN

  // PDSS protocol version
  unsigned Version : 2;
  // Unused
  unsigned Data_Status_Bit_5 : 1;
  // Unused
  unsigned Data_Status_Bit_4 : 1;
  // Unused
  unsigned Data_Status_Bit_3 : 1;
  // Indicates a VCDU Sequence Error occurred
  unsigned VCDU_Sequence_Error : 1;
  // Indicates a Packet Sequence Error occurred
  unsigned Packet_Sequence_Error : 1;
  // Unused
  unsigned Data_Status_Bit_0 : 1;
  // VCDU sequence number from VCDU header
  unsigned VCDU_Sequence_Number : 24;
  // Unique identifier for data stream, 0 = CCSDS, 1 = BPDU
  unsigned Data_Stream_ID : 1;
  // unused
  unsigned PDSS_Reserved_1 : 7;
  // unused
  unsigned PDSS_Reserved_2 : 8;
  // unused
  unsigned PDSS_Reserved_3 : 3;
  // 0 specifies not a POIC created GSE pkt
  unsigned GSE_Pkt_ID : 1;
  // 0 = Core, 1 = Payload
  unsigned Payload_vs_Core_ID : 1;
  // user's APID
  unsigned APID : 11;
  // PDSS internal routing ID
  unsigned Routing_ID : 16;
  // PDSS reserved sync pattern
  unsigned PDSS_Reserved_Sync : 16;

#else

  // LITTLE_ENDIAN definitions (4-byte words must be
  // byte swapped before using bit fields)

  // word 0
  // VCDU sequence number from VCDU header
  unsigned VCDU_Sequence_Number : 24;
  // Unused
  unsigned Data_Status_Bit_0 : 1;
  // Indicates a Packet Sequence Error occurred
  unsigned Packet_Sequence_Error : 1;
  // Indicates a VCDU Sequence Error occurred
  unsigned VCDU_Sequence_Error : 1;
  // Unused
  unsigned Data_Status_Bit_3 : 1;
  // Unused
  unsigned Data_Status_Bit_4 : 1;
  // Unused
  unsigned Data_Status_Bit_5 : 1;
  // PDSS protocol version
  unsigned Version : 2;

  // word 1
  // user's APID
  unsigned APID : 11;
  // 0 = Core, 1 = Payload
  unsigned Payload_vs_Core_ID : 1;
  // 0 specifies not a POIC created GSE pkt
  unsigned GSE_Pkt_ID : 1;
  // unused
  unsigned PDSS_Reserved_3 : 3;
  // unused
  unsigned PDSS_Reserved_2 : 8;
  // unused
  unsigned PDSS_Reserved_1 : 7;
  // Unique identifier for data stream, 0 = CCSDS, 1 = BPDU
  unsigned Data_Stream_ID : 1;

  // word 2
  // PDSS reserved sync pattern
  unsigned PDSS_Reserved_Sync : 16;
  // PDSS internal routing ID
  unsigned Routing_ID : 16;

#endif

} EHS_PDSS_Payload_Secondary_Protocol_Type_t;

typedef EHS_PDSS_Payload_Secondary_Protocol_Type_t
  EHS_PDSS_Payload_Secondary_Header_Type;
typedef EHS_PDSS_Payload_Secondary_Protocol_Type_t
  EHS_PDSS_Payload_Secondary_Header_Type_t;


// ****************************************************************
// * PDSS BPDU Telemetry Secondary Protocol Header Structure  
// ****************************************************************

typedef struct EHS_PDSS_BPDU_Secondary_Protocol_Type
{
#if BYTE_ORDER == BIG_ENDIAN

  // PDSS protocol version
  unsigned Version : 2;
  // Unused
  unsigned Data_Status_Bit_5 : 1;
  // Unused
  unsigned Data_Status_Bit_4 : 1;
  // Unused
  unsigned Data_Status_Bit_3 : 1;
  // Indicates a VCDU Sequence Error occurred
  unsigned VCDU_Sequence_Error : 1;
  // Unused
  unsigned Data_Status_Bit_1 : 1;
  // Unused
  unsigned Data_Status_Bit_0 : 1;
  // VCDU sequence number from VCDU header
  unsigned VCDU_Sequence_Number : 24;
  // Unique identifier for data stream, 0 = CCSDS, 1 = BPDU
  unsigned Data_Stream_ID : 1;
  // unused
  unsigned PDSS_Reserved_1 : 7;
  // unused
  unsigned PDSS_Reserved_2 : 8;
  // unused
  unsigned PDSS_Reserved_3 : 3;
  // 0 specifies not a POIC created GSE pkt
  unsigned GSE_Pkt_ID : 1;
  // 0 = Core, 1 = Payload
  unsigned Payload_vs_Core_ID : 1;
  // user's APID
  unsigned APID : 11;
  // PDSS internal routing ID
  unsigned Routing_ID : 16;
  // PDSS reserved sync pattern
  unsigned PDSS_Reserved_Sync : 16;

#else

  // LITTLE_ENDIAN definitions (4-byte words must be
  // byte swapped before using bit fields)

  // word 0
  // VCDU sequence number from VCDU header
  unsigned VCDU_Sequence_Number : 24;
  // Unused
  unsigned Data_Status_Bit_0 : 1;
  // Unused
  unsigned Data_Status_Bit_1 : 1;
  // Indicates a VCDU Sequence Error occurred
  unsigned VCDU_Sequence_Error : 1;
  // Unused
  unsigned Data_Status_Bit_3 : 1;
  // Unused
  unsigned Data_Status_Bit_4 : 1;
  // Unused
  unsigned Data_Status_Bit_5 : 1;
  // PDSS protocol version
  unsigned Version : 2;

  // word 1
  // user's APID
  unsigned APID : 11;
  // 0 = Core, 1 = Payload
  unsigned Payload_vs_Core_ID : 1;
  // 0 specifies not a POIC created GSE pkt
  unsigned GSE_Pkt_ID : 1;
  // unused
  unsigned PDSS_Reserved_3 : 3;
  // unused
  unsigned PDSS_Reserved_2 : 8;
  // unused
  unsigned PDSS_Reserved_1 : 7;
  // Unique identifier for data stream, 0 = CCSDS, 1 = BPDU
  unsigned Data_Stream_ID : 1;

  // word 2
  // PDSS reserved sync pattern
  unsigned PDSS_Reserved_Sync : 16;
  // PDSS internal routing ID
  unsigned Routing_ID : 16;

#endif

} EHS_PDSS_BPDU_Secondary_Protocol_Type_t;

typedef EHS_PDSS_BPDU_Secondary_Protocol_Type_t
  EHS_PDSS_BPDU_Secondary_Header_Type;
typedef EHS_PDSS_BPDU_Secondary_Protocol_Type_t
  EHS_PDSS_BPDU_Secondary_Header_Type_t;


// ****************************************************************
// * PDSS UDSM Telemetry Secondary Protocol Header Structure  
// ****************************************************************

typedef struct EHS_PDSS_UDSM_Secondary_Protocol_Type
{
#if BYTE_ORDER == BIG_ENDIAN

  // PDSS protocol version
  unsigned Version : 2;
  // Unused
  unsigned Data_Status_Bit_5 : 1;
  // Unused
  unsigned Data_Status_Bit_4 : 1;
  // Unused
  unsigned Data_Status_Bit_3 : 1;
  // Unused
  unsigned Data_Status_Bit_2 : 1;
  // Unused
  unsigned Data_Status_Bit_1 : 1;
  // Unused
  unsigned Data_Status_Bit_0 : 1;
  // Unused for UDSM
  unsigned VCDU_Sequence_Number : 24;
  // Unique identifier for data stream, 0 = CCSDS, 1 = BPDU
  unsigned Data_Stream_ID : 1;
  // unused
  unsigned PDSS_Reserved_1 : 7;
  // unused
  unsigned PDSS_Reserved_2 : 8;
  // unused
  unsigned PDSS_Reserved_3 : 3;
  // 0 specifies not a POIC created GSE pkt
  unsigned GSE_Pkt_ID : 1;
  // 0 = Core, 1 = Payload
  unsigned Payload_vs_Core_ID : 1;
  // user's APID
  unsigned APID : 11;
  // PDSS internal routing ID
  unsigned Routing_ID : 16;
  // PDSS reserved sync pattern
  unsigned PDSS_Reserved_Sync : 16;

#else

  // LITTLE_ENDIAN definitions (4-byte words must be
  // byte swapped before using bit fields)

  // word 0
  // Unused for UDSM
  unsigned VCDU_Sequence_Number : 24;
  // Unused
  unsigned Data_Status_Bit_0 : 1;
  // Unused
  unsigned Data_Status_Bit_1 : 1;
  // Unused
  unsigned Data_Status_Bit_2 : 1;
  // Unused
  unsigned Data_Status_Bit_3 : 1;
  // Unused
  unsigned Data_Status_Bit_4 : 1;
  // Unused
  unsigned Data_Status_Bit_5 : 1;
  // PDSS protocol version
  unsigned Version : 2;

  // word 1
  // user's APID
  unsigned APID : 11;
  // 0 = Core, 1 = Payload
  unsigned Payload_vs_Core_ID : 1;
  // 0 specifies not a POIC created GSE pkt
  unsigned GSE_Pkt_ID : 1;
  // unused
  unsigned PDSS_Reserved_3 : 3;
  // unused
  unsigned PDSS_Reserved_2 : 8;
  // unused
  unsigned PDSS_Reserved_1 : 7;
  // Unique identifier for data stream, 0 = CCSDS, 1 = BPDU
  unsigned Data_Stream_ID : 1;

  // word 2
  // PDSS reserved sync pattern
  unsigned PDSS_Reserved_Sync : 16;
  // PDSS internal routing ID
  unsigned Routing_ID : 16;

#endif

} EHS_PDSS_UDSM_Secondary_Protocol_Type_t;

typedef EHS_PDSS_UDSM_Secondary_Protocol_Type
  EHS_PDSS_UDSM_Secondary_Header_Type;
typedef EHS_PDSS_UDSM_Secondary_Protocol_Type
  EHS_PDSS_UDSM_Secondary_Header_Type_t;


// ****************************************************************
// * CCSDS Primary Header Structure
// *
// * NOTE: this header should not be used "in place" within a 
// * packet buffer since it is 2 bytes larger than the actual 
// * CCSDS primary header due to the compiler rounding up to a 
// * full word.  bytes must therefore be copied to/from the packet
// * buffer on write/reads.  in other words, you can't create a
// * super structure of both the primary and secondary CCSDS
// * headers just by putting them back to back, since this
// * primary header is padded, it would push the secondary
// * header down two bytes, which of course, is then just
// * plain wrong.
// *
// * there are, however, two super structures defined in this
// * include file which encompass both the primary and secondary
// * CCSDS headers in such a manner as to resolve this problem.
// * those structures are EHS_CCSDS_Core_Headers_Type and
// * EHS_CCSDS_Payload_Headers_Type, which are defined below.
// ****************************************************************

typedef struct EHS_CCSDS_Primary_Header_Type
{
#if BYTE_ORDER == BIG_ENDIAN

  // CCSDS Protocol version of 0
  unsigned Version : 3;
  // CCSDS Protocol Type of 0
  unsigned Type_ID : 1;
  // Set to 1 if there is a secondary header
  unsigned Secondary_Header_Flag : 1;
  // Application ID
  unsigned Application_ID : 11;
  // Sequence flags are 0
  unsigned Sequence_Flags : 2;
  // The packet sequence counter
  unsigned Sequence_Count : 14;
  // This is the CCSDS packet length in bytes
  unsigned Packet_Length : 16;

  // this structure is going to get padded by the
  // compiler anyway, so we might as well take
  // advantage of it and give ourselves access
  // to the beginning of the data zone or ccsds
  // secondary header as the case may be
  unsigned char pad[2];

#else

  // LITTLE_ENDIAN definitions (4-byte words must be
  // byte swapped before using bit fields)

  // word 0
  // The packet sequence counter
  unsigned Sequence_Count : 14;
  // Sequence flags are 0
  unsigned Sequence_Flags : 2;
  // Application ID
  unsigned Application_ID : 11;
  // Set to 1 if there is a secondary header
  unsigned Secondary_Header_Flag : 1;
  // CCSDS Protocol Type of 0
  unsigned Type_ID : 1;
  // CCSDS Protocol version of 0
  unsigned Version : 3;

  // word 1
  // this structure is going to get padded by the
  // compiler anyway, so we might as well take
  // advantage of it and give ourselves access
  // to the beginning of the data zone or ccsds
  // secondary header as the case may be
  unsigned char pad[2];

  // This is the CCSDS packet length in bytes
  unsigned Packet_Length : 16;

#endif

} EHS_CCSDS_Primary_Header_Type_t;


// this definition is the same as above except that
// the parameter names match the names in the combo
// headers structure type (EHS_CCSDS_Core_Headers_Type
// and EHS_CCSDS_Payload_Headers_Type structures)
typedef struct EHS_CCSDS_Primary_Header_Type2
{
#if BYTE_ORDER == BIG_ENDIAN

  // CCSDS Protocol version of 0
  unsigned CCSDS_Version : 3;
  // CCSDS Protocol Type of 0
  unsigned CCSDS_Type_ID : 1;
  // Set to 1 if there is a secondary header
  unsigned CCSDS_Secondary_Header_Flag : 1;
  // Application ID
  unsigned CCSDS_Application_ID : 11;
  // Sequence flags are 0
  unsigned CCSDS_Sequence_Flags : 2;
  // The packet sequence counter
  unsigned CCSDS_Sequence_Count : 14;
  // This is the CCSDS packet length in bytes
  unsigned CCSDS_Packet_Length : 16;

  // this structure is going to get padded by the
  // compiler anyway, so we might as well take
  // advantage of it and give ourselves access
  // to the beginning of the data zone or ccsds
  // secondary header as the case may be
  unsigned char pad[2];

#else

  // LITTLE_ENDIAN definitions (4-byte words must be
  // byte swapped before using bit fields)

  // word 0
  // The packet sequence counter
  unsigned CCSDS_Sequence_Count : 14;
  // Sequence flags are 0
  unsigned CCSDS_Sequence_Flags : 2;
  // Application ID
  unsigned CCSDS_Application_ID : 11;
  // Set to 1 if there is a secondary header
  unsigned CCSDS_Secondary_Header_Flag : 1;
  // CCSDS Protocol Type of 0
  unsigned CCSDS_Type_ID : 1;
  // CCSDS Protocol version of 0
  unsigned CCSDS_Version : 3;

  // word 1
  // this structure is going to get padded by the
  // compiler anyway, so we might as well take
  // advantage of it and give ourselves access
  // to the beginning of the data zone or ccsds
  // secondary header as the case may be
  unsigned char pad[2];

  // This is the CCSDS packet length in bytes
  unsigned CCSDS_Packet_Length : 16;

#endif

} EHS_CCSDS_Primary_Header_Type2_t;



// ****************************************************************
// * CCSDS primary+secondary Headers Structure for core packets.
// *
// * NOTE: Since these headers together are an even number of 4 byte 
// * words in size (16 bytes/4 words) it can be used "in place" 
// * within a packet buffer since the compiler will not round up to 
// * a full word.
// ****************************************************************

typedef struct EHS_CCSDS_Core_Headers_Type
{
#if BYTE_ORDER == BIG_ENDIAN

  // primary CCSDS header

  // CCSDS Protocol version number of 0
  unsigned CCSDS_Version : 3;
  // CCSDS Protocol Type of 0
  unsigned CCSDS_Type_ID : 1;
  // Set to 1
  unsigned CCSDS_Secondary_Header_Flag : 1;
  // APID
  unsigned CCSDS_Application_ID : 11;
  // Sequence flags set to 0
  unsigned CCSDS_Sequence_Flags : 2;
  // The packet sequence counter
  unsigned CCSDS_Sequence_Count : 14;
  // This is the CCSDS packet length in bytes
  unsigned CCSDS_Packet_Length : 16;

  // secondary CCSDS header

  // MSBs of coarse time
  unsigned CCSDS_Coarse_Time_MSB : 16;
  // LSBs of coarse time
  unsigned CCSDS_Coarse_Time_LSB : 16;
  // LSBs of fine time
  unsigned CCSDS_Fine_Time : 8;
  // Time ID
  unsigned CCSDS_Time_ID : 2;
  // Checksum indicator
  unsigned CCSDS_Chksum_Flag : 1;
  // Spare bit
  unsigned CCSDS_Spare_1 : 1;
  // Packet Type
  unsigned CCSDS_Packet_Contents : 4;
  // Spare bit
  unsigned CCSDS_Spare_2 : 1;
  // Element ID
  unsigned CCSDS_Element_ID : 4;
  // Command/Data Packet Indicator
  unsigned CCSDS_Cmd_Data_Packet : 1;
  // Format Version
  unsigned CCSDS_Format_Version_ID : 4;
  // Format ID
  unsigned CCSDS_Extended_Format_ID : 6;
  // spare
  unsigned CCSDS_Spare_3 : 8;
  // Format dependant processing frame count ID
  unsigned CCSDS_Frame_ID : 8;

#else

  // LITTLE_ENDIAN definitions (4-byte words must be
  // byte swapped before using bit fields)

  // word 0
  // The packet sequence counter
  unsigned CCSDS_Sequence_Count : 14;
  // Sequence flags set to 0
  unsigned CCSDS_Sequence_Flags : 2;
  // APID
  unsigned CCSDS_Application_ID : 11;
  // Set to 1
  unsigned CCSDS_Secondary_Header_Flag : 1;
  // CCSDS Protocol Type of 0
  unsigned CCSDS_Type_ID : 1;
  // CCSDS Protocol version number of 0
  unsigned CCSDS_Version : 3;

  // word 1
  // MSBs of coarse time
  unsigned CCSDS_Coarse_Time_MSB : 16;
  // This is the CCSDS packet length in bytes
  unsigned CCSDS_Packet_Length : 16;

  // word 2
  // Packet Type
  unsigned CCSDS_Packet_Contents : 4;
  // Spare bit
  unsigned CCSDS_Spare_1 : 1;
  // Checksum indicator
  unsigned CCSDS_Chksum_Flag : 1;
  // Time ID
  unsigned CCSDS_Time_ID : 2;
  // LSBs of fine time
  unsigned CCSDS_Fine_Time : 8;
  // LSBs of coarse time
  unsigned CCSDS_Coarse_Time_LSB : 16;

  // word 3
  // Format dependant processing frame count ID
  unsigned CCSDS_Frame_ID : 8;
  // spare
  unsigned CCSDS_Spare_3 : 8;
  // Format ID
  unsigned CCSDS_Extended_Format_ID : 6;
  // Format Version
  unsigned CCSDS_Format_Version_ID : 4;
  // Command/Data Packet Indicator
  unsigned CCSDS_Cmd_Data_Packet : 1;
  // Element ID
  unsigned CCSDS_Element_ID : 4;
  // Spare bit
  unsigned CCSDS_Spare_2 : 1;

#endif

} EHS_CCSDS_Core_Headers_Type_t;



// ****************************************************************
// * CCSDS primary+secondary Headers Structure for payload packets.
// *
// * NOTE: Since these headers together are an even number of 4 byte 
// * words in size (16 bytes/4 words) it can be used "in place" 
// * within a packet buffer since the compiler will not round up to 
// * a full word.
// ****************************************************************

typedef struct EHS_CCSDS_Payload_Headers_Type
{
#if BYTE_ORDER == BIG_ENDIAN

  // primary header

  // CCSDS Protocol version number of 0
  unsigned CCSDS_Version : 3;
  // CCSDS Protocol Type of 0
  unsigned CCSDS_Type_ID : 1;
  // Set to 1
  unsigned CCSDS_Secondary_Header_Flag : 1;
  // APID
  unsigned CCSDS_Application_ID : 11;
  // Sequence flags set to 0
  unsigned CCSDS_Sequence_Flags : 2;
  // The packet sequence counter
  unsigned CCSDS_Sequence_Count : 14;
  // This is the CCSDS packet length in bytes
  unsigned CCSDS_Packet_Length : 16;

  // secondary header

  // MSBs of coarse time
  unsigned CCSDS_Coarse_Time_MSB : 16;
  // LSBs of coarse time
  unsigned CCSDS_Coarse_Time_LSB : 16;
  // LSBs of fine time
  unsigned CCSDS_Fine_Time : 8;
  // Time ID
  unsigned CCSDS_Time_ID : 2;
  // Checksum indicator
  unsigned CCSDS_Chksum_Flag : 1;
  // Used by S-Band ZOE Packets
  unsigned ZOE_Tlm : 1;
  // Packet Type unused by Ku band
  unsigned Packet_Type : 4;
  // Specifies data field structure
  unsigned Version_ID : 16;
  // Identifies packet within multipacket data cycle
  unsigned Data_Cycle_Counter : 16;

#else

  // LITTLE_ENDIAN definitions (4-byte words must be
  // byte swapped before using bit fields)

  // word 0
  // The packet sequence counter
  unsigned CCSDS_Sequence_Count : 14;
  // Sequence flags set to 0
  unsigned CCSDS_Sequence_Flags : 2;
  // APID
  unsigned CCSDS_Application_ID : 11;
  // Set to 1
  unsigned CCSDS_Secondary_Header_Flag : 1;
  // CCSDS Protocol Type of 0
  unsigned CCSDS_Type_ID : 1;
  // CCSDS Protocol version number of 0
  unsigned CCSDS_Version : 3;

  // word 1
  // MSBs of coarse time
  unsigned CCSDS_Coarse_Time_MSB : 16;
  // This is the CCSDS packet length in bytes
  unsigned CCSDS_Packet_Length : 16;

  // word 2
  // Packet Type unused by Ku band
  unsigned Packet_Type : 4;
  // Used by S-Band ZOE Packets
  unsigned ZOE_Tlm : 1;
  // Checksum indicator
  unsigned CCSDS_Chksum_Flag : 1;
  // Time ID
  unsigned CCSDS_Time_ID : 2;
  // LSBs of fine time
  unsigned CCSDS_Fine_Time : 8;
  // LSBs of coarse time
  unsigned CCSDS_Coarse_Time_LSB : 16;

  // word 3
  // Identifies packet within multipacket data cycle
  unsigned Data_Cycle_Counter : 16;
  // Specifies data field structure
  unsigned Version_ID : 16;

#endif

} EHS_CCSDS_Payload_Headers_Type_t;



// ****************************************************************
// * CCSDS primary+secondary Headers Structure for GSE packets.
// ****************************************************************

typedef struct EHS_CCSDS_GSE_Headers_Type
{
#if BYTE_ORDER == BIG_ENDIAN

  // primary header

  // CCSDS Protocol version number of 0
  unsigned CCSDS_Version : 3;
  // CCSDS Protocol Type of 0
  unsigned CCSDS_Type_ID : 1;
  // Set to 1
  unsigned CCSDS_Secondary_Header_Flag : 1;
  // APID
  unsigned CCSDS_Application_ID : 11;
  // Sequence flags set to 0
  unsigned CCSDS_Sequence_Flags : 2;
  // The packet sequence counter
  unsigned CCSDS_Sequence_Count : 14;
  // This is the CCSDS packet length in bytes
  unsigned CCSDS_Packet_Length : 16;

  // secondary header - one byte in length

  // GSE format id
  unsigned CCSDS_GSE_Format_Id : 3;

  // GSE spare bits
  unsigned CCSDS_GSE_Spare_1 : 5;

  // this structure will be padded, and we can take advantage
  // of that and give ourselves access to the packet data area
  unsigned char pad[1];

#else

  // LITTLE_ENDIAN definitions (4-byte words must be
  // byte swapped before using bit fields)

  // word 0
  // The packet sequence counter
  unsigned CCSDS_Sequence_Count : 14;
  // Sequence flags set to 0
  unsigned CCSDS_Sequence_Flags : 2;
  // APID
  unsigned CCSDS_Application_ID : 11;
  // Set to 1
  unsigned CCSDS_Secondary_Header_Flag : 1;
  // CCSDS Protocol Type of 0
  unsigned CCSDS_Type_ID : 1;
  // CCSDS Protocol version number of 0
  unsigned CCSDS_Version : 3;

  // word 1
  // this structure will be padded, and we can take advantage
  // of that and give ourselves access to the packet data area
  unsigned char pad[1];

  // GSE spare bits
  unsigned CCSDS_GSE_Spare_1 : 5;

  // GSE format id
  unsigned CCSDS_GSE_Format_Id : 3;

  // This is the CCSDS packet length in bytes
  unsigned CCSDS_Packet_Length : 16;

#endif

} EHS_CCSDS_GSE_Headers_Type_t;

// actually there is no difference between gse ccsds headers
// for core vs. payload, but just so there is no confusion,
// and just in case i'm wrong, or in case it changes someday,
// we will define and use a type for each.
typedef EHS_CCSDS_GSE_Headers_Type_t EHS_CCSDS_GSE_Core_Headers_Type_t;
typedef EHS_CCSDS_GSE_Headers_Type_t EHS_CCSDS_GSE_Payload_Headers_Type_t;


// ******************************************************************
// * PDSS AOS/LOS Indicator Packet Secondary Protocol Header Structure  
// ******************************************************************

typedef struct EHS_PDSS_AOS_LOS_Secondary_Protocol_Type
{
#if BYTE_ORDER == BIG_ENDIAN

  // PDSS protocol version
  unsigned Version : 2;
  // Unused
  unsigned Data_Status_Bit_5 : 1;
  // Unused
  unsigned Data_Status_Bit_4 : 1;
  // Unused
  unsigned Data_Status_Bit_3 : 1;
  // Unused
  unsigned Data_Status_Bit_2 : 1;
  // Unused
  unsigned Data_Status_Bit_1 : 1;
  // Unused
  unsigned Data_Status_Bit_0 : 1;
  // Unused, set to zeros
  unsigned VCDU_Sequence_Number : 24;
  // Unique identifier for data stream, 0 = CCSDS
  unsigned Data_Stream_ID : 1;
  // unused
  unsigned PDSS_Reserved_1 : 7;
  // unused
  unsigned PDSS_Reserved_2 : 8;
  // unused
  unsigned PDSS_Reserved_3 : 3;
  // 0 specifies not a POIC created GSE pkt
  unsigned GSE_Pkt_ID : 1;
  // 0 = Core, 1 = Payload
  unsigned Payload_vs_Core_ID : 1;
  // user's APID
  unsigned APID : 11;
  // PDSS internal routing ID
  unsigned Routing_ID : 16;
  // PDSS reserved sync pattern 
  unsigned PDSS_Reserved_Sync : 16;

#else

  // LITTLE_ENDIAN definitions (4-byte words must be
  // byte swapped before using bit fields)

  // word 0
  // Unused, set to zeros
  unsigned VCDU_Sequence_Number : 24;
  // Unused
  unsigned Data_Status_Bit_0 : 1;
  // Unused
  unsigned Data_Status_Bit_1 : 1;
  // Unused
  unsigned Data_Status_Bit_2 : 1;
  // Unused
  unsigned Data_Status_Bit_3 : 1;
  // Unused
  unsigned Data_Status_Bit_4 : 1;
  // Unused
  unsigned Data_Status_Bit_5 : 1;
  // PDSS protocol version
  unsigned Version : 2;

  // word 1
  // user's APID
  unsigned APID : 11;
  // 0 = Core, 1 = Payload
  unsigned Payload_vs_Core_ID : 1;
  // 0 specifies not a POIC created GSE pkt
  unsigned GSE_Pkt_ID : 1;
  // unused
  unsigned PDSS_Reserved_3 : 3;
  // unused
  unsigned PDSS_Reserved_2 : 8;
  // unused
  unsigned PDSS_Reserved_1 : 7;
  // Unique identifier for data stream, 0 = CCSDS
  unsigned Data_Stream_ID : 1;

  // word 2
  // PDSS reserved sync pattern 
  unsigned PDSS_Reserved_Sync : 16;
  // PDSS internal routing ID
  unsigned Routing_ID : 16;

#endif

} EHS_PDSS_AOS_LOS_Secondary_Protocol_Type_t;

typedef EHS_PDSS_AOS_LOS_Secondary_Protocol_Type_t
  EHS_PDSS_AOS_LOS_Secondary_Header_Type;
typedef EHS_PDSS_AOS_LOS_Secondary_Protocol_Type_t
  EHS_PDSS_AOS_LOS_Secondary_Header_Type_t;



// ******************************************************************
// * PDSS AOS/LOS Indicator Packet Data Zone Structure  
// *
// * NOTE: this structure is only two bytes and not word aligned, but
// * if you just do a simple 2-byte swap, everything still works
// ******************************************************************

typedef struct EHS_PDSS_AOS_LOS_Data_Type
{
#if BYTE_ORDER == BIG_ENDIAN

  // PDSS reserved
  unsigned Reserved_1 : 6;
  // 0=S band indicator, 1=Ku band indicator
  unsigned Band_Indicator : 1;
  // 0=PDSS LOS, 1=PDSS AOS
  unsigned AOS_LOS_Indicator : 1;
  // unused
  unsigned Reserved_2 : 8;

#else

  // unused
  unsigned Reserved_2 : 8;
  // 0=PDSS LOS, 1=PDSS AOS
  unsigned AOS_LOS_Indicator : 1;
  // 0=S band indicator, 1=Ku band indicator
  unsigned Band_Indicator : 1;
  // PDSS reserved
  unsigned Reserved_1 : 6;

#endif

} EHS_PDSS_AOS_LOS_Data_Type_t;


// ******************************************************************
// * packet header definition that should allow you to access all
// * the pieces of a fully ehs encapsulated data packet.  this
// * definition is dependent on the fact that the ehs primary and
// * secondary headers are sized on full word boundaries.  since
// * this would likely require an act of congress to change, this
// * definition should be safe to use.
// ******************************************************************

typedef struct EHS_Packet_Type
{
  // ehs primary header
  EHS_Primary_Header_Type primary;

  // ehs secondary header types
  typedef union EHS_PDSS_Secondary_Header_Type
  {
    EHS_PDSS_Core_Secondary_Header_Type core;
    EHS_PDSS_Payload_Secondary_Header_Type payload;
    EHS_PDSS_BPDU_Secondary_Header_Type bpdu;
    EHS_PDSS_UDSM_Secondary_Header_Type udsm;
    EHS_PDSS_AOS_LOS_Secondary_Header_Type aoslos;
  } EHS_PDSS_Secondary_Header_Type_t;

  EHS_PDSS_Secondary_Header_Type_t secondary;

  // ccsds headers
  typedef union EHS_CCSDS_Header_Type
  {
    // primary only ccsds header
    EHS_CCSDS_Primary_Header_Type primary;
    EHS_CCSDS_Primary_Header_Type2 primary2;

    // primary+secondary ccsds headers
    EHS_CCSDS_Core_Headers_Type core;
    EHS_CCSDS_Payload_Headers_Type payload;

  } EHS_CCSDS_Header_Type_t;

  EHS_CCSDS_Header_Type_t ccsds;

  // data zone if there IS a secondary ccsds header
  unsigned char data_zone[1];

  // data zone if there is NOT a secondary ccsds header
#if !defined(data_zone2)
# define data_zone2 ccsds.primary.pad
#endif

} EHS_Packet_Type_t;


// ****************************************************************
// * EXCEPTIONS TO THE RULE
// *
// * what?  you thought everything would just fall into place
// * without any exceptions?  currently, one of the ehs types, 
// * namely GSE, is processed by the pdsm software for storage,
// * so we need to set up the structures for it as well.  and
// * as you have probably already guessed, it does not conform,
// * the secondary EHS header size is an additional 4 bytes in
// * length.  so it must be handled as an exception to the rule.
// *
// * also, the CCSDS secondary header for GSE packets is not the
// * same either, it is only one byte in length.
// ****************************************************************


// ****************************************************************
// * GSE Telemetry Secondary Protocol Header Structure  
// ****************************************************************

typedef struct EHS_GSE_Secondary_Protocol_Type
{
#if BYTE_ORDER == BIG_ENDIAN

  // packet routing flag, 0=TNS routed 1=PDSS routed
  unsigned Packet_Routing_Flag : 1;

  // user interface request type, 0=POIC 1=remote user
  unsigned User_Interface_Request_Type : 1;

  // unused
  unsigned GSE_Unused_1 : 6;
  unsigned GSE_Unused_2 : 8;

  // workstation ID
  unsigned Workstation_ID : 16;

  // unique identifier for data stream, 0 = CCSDS, 1 = BPDU
  unsigned Data_Stream_ID : 1;

  // unused
  unsigned GSE_Unused_3 : 7;
  unsigned GSE_Unused_4 : 8;
  unsigned GSE_Unused_5 : 3;

  // 0 specifies not a POIC created GSE pkt
  unsigned GSE_Pkt_ID : 1;

  // 0 = Core, 1 = Payload
  unsigned Payload_vs_Core_ID : 1;

  // application identifier
  unsigned APID : 11;

  // PDSS internal routing ID
  unsigned Routing_ID : 16;

  // PDSS reserved sync pattern 
  unsigned PDSS_Reserved_Sync : 16;

  // user ID
  unsigned User_ID : 16;

  // process ID
  unsigned Process_ID : 16;

#else

  // LITTLE_ENDIAN definitions (4-byte words must be
  // byte swapped before using bit fields)

  // word 0
  // packet routing flag, 0=TNS routed 1=PDSS routed
  // workstation ID
  unsigned Workstation_ID : 16;

  // unused
  unsigned GSE_Unused_2 : 8;
  unsigned GSE_Unused_1 : 6;

  // user interface request type, 0=POIC 1=remote user
  unsigned User_Interface_Request_Type : 1;

  unsigned Packet_Routing_Flag : 1;

  // word 1
  // application identifier
  unsigned APID : 11;

  // 0 = Core, 1 = Payload
  unsigned Payload_vs_Core_ID : 1;

  // 0 specifies not a POIC created GSE pkt
  unsigned GSE_Pkt_ID : 1;

  // unused
  unsigned GSE_Unused_5 : 3;
  unsigned GSE_Unused_4 : 8;
  unsigned GSE_Unused_3 : 7;

  // unique identifier for data stream, 0 = CCSDS, 1 = BPDU
  unsigned Data_Stream_ID : 1;

  // word 2
  // PDSS reserved sync pattern 
  unsigned PDSS_Reserved_Sync : 16;

  // PDSS internal routing ID
  unsigned Routing_ID : 16;

  // word 3
  // process ID
  unsigned Process_ID : 16;

  // user ID
  unsigned User_ID : 16;

#endif

} EHS_GSE_Secondary_Protocol_Type_t;

typedef EHS_GSE_Secondary_Protocol_Type_t EHS_GSE_Secondary_Header_Type;
typedef EHS_GSE_Secondary_Protocol_Type_t EHS_GSE_Secondary_Header_Type_t;


// ******************************************************************
// * packet header definition that should allow you to access all
// * the pieces of a fully ehs encapsulated GSE data packet.
// *
// * this definition can also be adapted for any other exceptions
// * to the rule whose secondary header length is 4 words like gse,
// * should any be required in the future.  all you will have to do
// * is add another entry to the secondary header type union, and to
// * the ccsds header type union (which may require additional #define
// * statements for the "data_zone" as well)
// ******************************************************************

typedef struct EHS_Packet_Type2
{
  // ehs primary header
  EHS_Primary_Header_Type primary;

  // ehs secondary header types
  typedef union EHS_PDSS_Secondary_Header_Type
  {
    EHS_GSE_Secondary_Header_Type gse;
  } EHS_PDSS_Secondary_Header_Type_t;

  EHS_PDSS_Secondary_Header_Type_t secondary;

  // ccsds headers
  typedef union EHS_CCSDS_Header_Type
  {
    // primary only ccsds header
    EHS_CCSDS_Primary_Header_Type primary;
    EHS_CCSDS_Primary_Header_Type2 primary2;

    // gse primary+secondary ccsds headers
    typedef union EHS_CCSDS_GSE_Header_Type
    {
      EHS_CCSDS_GSE_Core_Headers_Type_t core;
      EHS_CCSDS_GSE_Payload_Headers_Type_t payload;
    } EHS_CCSDS_GSE_Header_Type_t;

    EHS_CCSDS_GSE_Header_Type_t gse;

  } EHS_CCSDS_Header_Type_t;

  EHS_CCSDS_Header_Type_t ccsds;

  // gse core type data zone when there IS a secondary ccsds header
#if !defined gse_core_data_zone
# define gse_core_data_zone ccsds.gse.core.pad
#endif

  // gse payload type data zone when there IS a secondary ccsds header
#if !defined gse_payload_data_zone
# define gse_payload_data_zone ccsds.gse.payload.pad
#endif

  // gse core type data zone if there is NOT a secondary ccsds header
#if !defined(gse_core_data_zone2)
# define gse_core_data_zone2 ccsds.primary.pad
#endif

  // gse payload type data zone if there is NOT a secondary ccsds header
#if !defined(gse_payload_data_zone2)
# define gse_payload_data_zone2 ccsds.primary.pad
#endif

} EHS_Packet_Type2_t;


#endif // __PROTOCOLS_H__

