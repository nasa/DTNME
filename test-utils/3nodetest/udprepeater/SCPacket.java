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


package udprepeater;

public class SCPacket
{
  // packet request type strings
  private static final String[] PacketRequestTypeStrings =
  {
    "InvalidPacketType",
    "NoPacketAvailable",

    "Advertisement",

    "Command",
    "CommandResponse",
    
    "Status",

    "DataExchangeRequest",
    "DataExchangeResponse",

    "LoginRequest",
    "LoginResponse",
    "LogoutRequest",

    "StatusRequest",
    "StatusResponse",
    "StatusTerminateRequest",
    
    "RequestToControlUser",
  };

  // packet request types
  public static final int InvalidPacketType      = 0;
  public static final int NoPacketAvailable      = InvalidPacketType +  1;
  public static final int Advertisement          = InvalidPacketType +  2;
  public static final int Command                = InvalidPacketType +  3;
  public static final int CommandResponse        = InvalidPacketType +  4;
  public static final int Status                 = InvalidPacketType +  5;
  public static final int DataExchangeRequest    = InvalidPacketType +  6;
  public static final int DataExchangeResponse   = InvalidPacketType +  7;
  public static final int LoginRequest           = InvalidPacketType +  8;
  public static final int LoginResponse          = InvalidPacketType +  9;
  public static final int LogoutRequest          = InvalidPacketType + 10;
  public static final int StatusRequest          = InvalidPacketType + 11;
  public static final int StatusResponse         = InvalidPacketType + 12;
  public static final int StatusTerminateRequest = InvalidPacketType + 13;
  public static final int RequestToControlUser   = InvalidPacketType + 14;
  public static final int NumPacketRequestTypes  = InvalidPacketType + 15;
  
  // decryption types
  public static final int Invalid_Encryption               = -1;
  public static final int No_Encryption                    = 0;
  public static final int Compression_Flag                 = 1;
  public static final int RSA_Advertisement_Key_Encryption = 2;
  public static final int AES_Advertisement_Key_Encryption = 4;
  public static final int AES_Command_Key_Encryption       = 8;
  public static final int AES_Status_Key_Encryption        = 16;
  public static final int Requester_Key_Encryption         = 32;
  
  // various protocol strings
  public static final String TOKEN_PKT_PACKET_REQUEST_TYPE = "PacketRequestType";
  public static final String TOKEN_PKT_HOST_ID = "HostID";
  public static final String TOKEN_PKT_RETURN_IP_ADDR = "ReturnIPAddr";
  public static final String TOKEN_PKT_RETURN_PORT = "ReturnPort";
  public static final String TOKEN_PKT_REQUESTERS_ENCRYPTION_KEY = "RequestersEncryptionKey";
  public static final String TOKEN_PKT_GUI_SEQUENCE_COUNTER = "GuiSequenceCounter";
  public static final String TOKEN_PKT_SOURCE_SEQUENCE_COUNTER = "SourceSequenceCounter";
  public static final String TOKEN_PKT_PACKET_SEQUENCE_COUNTER = "PacketSequenceCounter";
  public static final String TOKEN_PKT_OF_THIS_MANY = "OfThisMany";

  public static final String TOKEN_PKT_DATA_ZONE = "DataZone";

  public static final String TOKEN_PKT_CRC32CKSUM = "CRC32Cksum";
  
  
  // Packet fields
  private int m_pkt_type;
  private String m_host_id;
  //protected String m_return_ip;
  //protected String m_return_port;
  //protected String m_return_encryption_key;
  private StringBuilder m_data_zone = new StringBuilder();
  private long m_gui_seqctr;
  private int m_decryption_type;
  
  
  /** Creates a new instance of SCPacket */
  public SCPacket ()
  {
    m_pkt_type = InvalidPacketType;
    m_host_id ="";
    //m_return_ip = "";
    //m_return_port = "";
    //m_return_encryption_key = "";
    m_gui_seqctr = -1;
    m_decryption_type = Invalid_Encryption;
  }

  // get the packet type string from an integer packet type
  public static String getPacketTypeStringFromType ( int pkttype )
  {
    String result = PacketRequestTypeStrings[InvalidPacketType];
    if ( ( pkttype >= 0 ) && ( pkttype < NumPacketRequestTypes ) )
    {
      result = PacketRequestTypeStrings[pkttype];
    }
    return result;
  }

  // get the integer packet type of this packet
  public int getPacketType()
  {
    return m_pkt_type;
  }
  
  // get the packet type string of this packet
  public String getPacketTypeString()
  {
    return PacketRequestTypeStrings[m_pkt_type];
  }
  
  // set packet type by integer
  public void setPacketType ( int pkttype )
  {
    if ( ( pkttype >= 0 ) && ( pkttype < NumPacketRequestTypes ) )
    {
      m_pkt_type = pkttype;
    }
    else
    {
      m_pkt_type = InvalidPacketType;
    }
  }
  
  // set packet type by string
  public void setPacketTypeString ( String pkttypestr )
  {
    m_pkt_type = InvalidPacketType;
    for ( int ix=0; ix<NumPacketRequestTypes; ++ix )
    {
      if ( PacketRequestTypeStrings[ix].equals ( pkttypestr ) )
      {
        m_pkt_type = ix;
        break;
      }
    }
  }

  // is this a valid packet?
  public boolean isValidPacket()
  {
    return ( m_pkt_type != InvalidPacketType );
  }
  
  // is this an invalid packet?
  public boolean isInvalidPacket()
  {
    return ( m_pkt_type == InvalidPacketType );
  }
  
  // is this a specific type of packet?
  public boolean isPacketType ( int pkttype )
  {
    return ( pkttype == m_pkt_type );
  }
  
  // get the Host ID of the sender of this packet
  public String getHostID()
  {
    return m_host_id;
  }
  
  // set the Host ID of the sender of this packet
  public void setHostID( String hostID )
  {
    m_host_id = hostID.intern();
  }

  // get the IP Address of the sender of this packet
  //public String getReturnIP()
  //{
  //  return m_return_ip;
  //}
  
  // set the IP Address of the sender of this packet
  //public void setReturnIP ( String ip )
  //{
  //  m_return_ip = ip;
  //}
  
  // get the Port of the sender of this packet
  //public String getReturnPort()
  //{
  //  return m_return_port;
  //}
  
  // set the Port of the sender of this packet
  //public void setReturnPort ( String port )
  //{
  //  m_return_port = port;
  //}
  
  // get the Public RSA Key of the sender of this packet
  //public String getReturnEncryptionKey()
  //{
  //  return m_return_encryption_key;
  //}
  
  // set the Public RSA Key of the sender of this packet
  //public void setReturnEncryptionKey ( String key )
  //{
  //  m_return_encryption_key = key;
  //}

  // get the data zone of this packet
  public String getDataZone()
  {
    return m_data_zone.toString();
  }

  // returns the internal StringBuilder for appending to another
  public StringBuilder getDataZoneSB()
  {
    return m_data_zone;
  }

  // set the data zone of this packet
  public void setDataZone ( String data )
  {
    m_data_zone.setLength(0);
    m_data_zone.append ( data );
  }

  // set the data zone of this packet
  public void setDataZone ( StringBuilder data )
  {
    m_data_zone.setLength(0);
    m_data_zone.append ( data );
  }

  // append more data to the data zone ( for multi-packet messages )
  public void appendDataZone ( String moredata )
  {
    m_data_zone.append ( moredata );
  }
  
  // append more data to the data zone ( for multi-packet messages )
  public void appendDataZone ( StringBuilder moredatasb )
  {
    m_data_zone.append ( moredatasb );
  }

  // set the GUI's sequence counter
  public void setGuiSeqCtr ( long seqctr )
  {
    m_gui_seqctr = seqctr;
  }
  
  // get the GUI's sequence counter
  public long getGuiSeqCtr ()
  {
    return m_gui_seqctr;
  }

  // get the encryption type
  public int getDecryptionType ()
  {
    return m_decryption_type;
  }
  
  // set the encryption type
  public void setDecryptionType ( int type )
  {
    m_decryption_type = type;
  }

  // was the packet encrypted with the Status Key?
  public boolean isStatusKeyEncryption ()
  {
    return ( m_decryption_type == AES_Status_Key_Encryption);
  }
  
  // was the packet encrypted with the Requester's Key?
  public boolean isRequesterKeyEncryption ()
  {
    return ( m_decryption_type == Requester_Key_Encryption);
  }
  
  // clean up to speed up garbage collection
  public void clear()
  {
    //m_return_ip = "";
    //m_return_port = "";
    //m_return_encryption_key = "";
    m_data_zone.setLength(0);
  }
}
