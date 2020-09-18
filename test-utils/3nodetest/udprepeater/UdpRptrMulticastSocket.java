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

import java.net.*;
import java.util.*;
import java.io.*;

public class UdpRptrMulticastSocket extends UdpRptrAbstractSocket
{
  int m_advertisementPort = 11400;
  MulticastSocket m_multicastSocket = null;
  String m_ipmulticast = "225.1.1.1";
  InetSocketAddress m_groupAddress = null;
  byte[] m_rcvbuf = new byte[65536];
  
  
  /**open method - must be called once before the first use
   * of this object.  method returns true if successful,
   * or false if an error occurs (see getErrString)
   */
  @Override
  public boolean open ()
  {
    m_openError = true;
    m_errstring = "UdpRptrMulticastSocket : 'open' has not been invoked";
    
    // create the multicast socket
    try
    {
      m_multicastSocket = new MulticastSocket (m_advertisementPort);

      int bufsiz = m_multicastSocket.getReceiveBufferSize();
      //System.out.printf("RcvBufSize = %d\n", bufsiz);
      if ( bufsiz < 20 * 1024 * 1024 )
      {
        bufsiz = 20 * 1024 * 1024;
        m_multicastSocket.setReceiveBufferSize(bufsiz);
        bufsiz = m_multicastSocket.getReceiveBufferSize();
        //System.out.printf("New RcvBufSize = %d\n", bufsiz);
      }

      bufsiz = m_multicastSocket.getSendBufferSize();
      //System.out.printf("SndBufSize = %d\n", bufsiz);
      if ( bufsiz < 5 * 1024 * 1024 )
      {
        bufsiz = 5 * 1024 * 1024;
        m_multicastSocket.setSendBufferSize(bufsiz);
        bufsiz = m_multicastSocket.getSendBufferSize();
        //System.out.printf("New SndBufSize = %d\n", bufsiz);
      }
      
      m_multicastSocket.setSoTimeout(100);  // 100 millisecs = 1/10th second
    }
    catch (IOException ex)
    {
      m_errstring =
              "UdpRptrMulticastSocket.open : MulticastSocket create exception - " +
              ex.toString ();
      return false;
    }
    
    // set the time-to-live ttl TTL value.  the default is just 1, and
    // sometimes with the spaghetti networks, and seemingly unecessary
    // firewalls etc we have around here, that just isn't enough.  so
    // how much is enough you might ask?  well, for sure we can't use
    // anything bigger than 255 anyway, so we'll just use it and hope...
    try
    {
      m_multicastSocket.setTimeToLive (255);
    }
    catch (Exception ex)
    {
      System.out.println (
              "UdpRptrMulticastSocket.open : " +
              "MulticastSocket setTimeToLive exception - " + ex.toString () );
    }
    
    // join the multicast group
    try
    {
      m_groupAddress = new InetSocketAddress ( m_ipmulticast, 0 );
    }
    catch (Exception ex)
    {
      m_errstring =
              "UdpRptrMulticastSocket.open : MulticastSocket InetSocketAddress create exception for " +
              m_ipmulticast + "- " + ex.toString ();
      return false;
    }
    
    try
    {
      Enumeration<NetworkInterface> nif_list = NetworkInterface.getNetworkInterfaces ();
      
      for ( ; nif_list.hasMoreElements (); )
      {
        NetworkInterface nif = nif_list.nextElement ();
        
        try
        {
          m_multicastSocket.joinGroup ( m_groupAddress, nif );
        }
        catch (Exception ex)
        {
          m_errstring = "UdpRptrMulticastSocket.open : MulticastSocket joinGroup exception for group " +
                  m_groupAddress.toString () + " on nic " + nif.toString () + " - " + ex.toString ();
        }
      }
    }
    catch (Exception ex)
    {
      m_errstring =
              "UdpRptrMulticastSocket.open : MulticastSocket joinGroup exception - " +
              ex.toString ();
      return false;
    }
    
    // return a good status
    m_openError = false;
    m_errstring = "";
    return true;
    
  }
  
  
  /** constructor*/
  public UdpRptrMulticastSocket ()
  {
  }
  
  
  /** overloaded open methods */
  @Override
  public boolean open ( int statusport )
  {
    m_advertisementPort = statusport;
    return open ();
  }
  
  @Override
  public boolean open ( String ipmulticast )
  {
    m_ipmulticast = ipmulticast;
    return open ();
  }
  
  @Override
  public boolean open ( int statusport, String ipmulticast )
  {
    m_advertisementPort = statusport;
    m_ipmulticast = ipmulticast;
    return open ();
  }
  
  
  /**get open error string*/
  @Override
  public String getErrString ()
  {
    return m_errstring;
  }
  
  
  /**true if there was an open error*/
  @Override
  public boolean openError ()
  {
    return m_openError;
  }
  
  @Override
  public void close ()
  {
    m_multicastSocket.close ();
  }
  
  /**read an advertisement packet*/
  @Override
  public int readPacket ( byte[] buf )
  {
    // if open failed, just return null string
    if (m_openError) return -1;
    
    // read the multicast socket and update the model with the new data
    // within an infinite loop for as long as this program is running
    DatagramPacket rcvpkt = new DatagramPacket ( m_rcvbuf, m_rcvbuf.length );
    
    while (true)
    {
      // note: must reset datagram packet length each time back to the actual
      // receive buffer length, otherwise, the next packet will be truncated
      // to the previous packets length, because the receive logic thinks
      // this is now the maximum length of the underlying receive buffer.
      rcvpkt.setLength ( m_rcvbuf.length );
      
      try
      {
        m_multicastSocket.receive (rcvpkt);
        System.arraycopy ( m_rcvbuf, 0, buf, 0, rcvpkt.getLength() );
      }
      catch (SocketTimeoutException ex)
      {
        return 0;
      }
      catch (Exception ex)
      {
        m_errstring = "UdpRptrMulticastSocket.readPacket : " +
                "MulticastSocket receive exception - " + ex.toString ();
        UdpRptrUiConstants.popup_msgbox (0, m_errstring);
        continue;
      }
      
      return rcvpkt.getLength ();
    }
    
  }
}

