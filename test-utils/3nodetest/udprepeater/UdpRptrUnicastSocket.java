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
import java.io.*;

public class UdpRptrUnicastSocket extends UdpRptrAbstractSocket
{
  
  int m_port = 11412;  // default Daemon Port
  
  DatagramSocket m_unicastSocket = null;
  byte[] m_rcvbuf = new byte[65536];
  
  
  /** Creates a new instance of EsmUnicastSocket */
  public UdpRptrUnicastSocket ()
  {
    m_openError = true;
    m_errstring = "UdpRptrUnicastSocket : 'open' has not been invoked";
  }
  
  /**open method - must be called once before the first use
   * of this object.  method returns true if successful,
   * or false if an error occurs (see getErrString)
   */
  @Override
  public boolean open ()
  {
    m_openError = true;
    m_errstring = "EsmUnicastSocket : 'open' has not been invoked";
    
    // create the multicast socket
    try
    {
      m_unicastSocket = new DatagramSocket ( null );

      int bufsiz = m_unicastSocket.getReceiveBufferSize();
      //System.out.printf("RcvBufSize = %d\n", bufsiz);
      if ( bufsiz < 20 * 1024 * 1024 )
      {
        bufsiz = 20 * 1024 * 1024;
        m_unicastSocket.setReceiveBufferSize(bufsiz);
        bufsiz = m_unicastSocket.getReceiveBufferSize();
        //System.out.printf("New RcvBufSize = %d\n", bufsiz);
      }

      bufsiz = m_unicastSocket.getSendBufferSize();
      //System.out.printf("SndBufSize = %d\n", bufsiz);
      if ( bufsiz < 5 * 1024 * 1024 )
      {
        bufsiz = 5 * 1024 * 1024;
        m_unicastSocket.setSendBufferSize(bufsiz);
        bufsiz = m_unicastSocket.getSendBufferSize();
        //System.out.printf("New SndBufSize = %d\n", bufsiz);
      }


    }
    catch (IOException ex)
    {
      m_errstring =
              "EsmUnicastSocket.open : DatagramSocket create exception - " +
              ex.toString ();
      return false;
    }
    
    // return a good status
    m_openError = false;
    m_errstring = "";
    return true;
    
  }
  
  /** overloaded open methods */
  @Override
  public boolean open ( int port )
  {
    m_port = port;
    return open ();
  }

  public boolean open ( int port, String ipmulticast )
  {
    m_port = port;
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
    if ( null != m_unicastSocket ) m_unicastSocket.close ();
  }
  
  /**read a packet*/
  @Override
  public int readPacket ( byte[] buf )
  {
    // if open failed, just return null
    if (m_openError) return -1;
    
    // read the socket and update the model with the new data
    // within an infinite loop for as long as this program is running
    DatagramPacket rcvpkt = new DatagramPacket ( m_rcvbuf, m_rcvbuf.length );
    
    // note: must reset datagram packet length each time back to the actual
    // receive buffer length, otherwise, the next packet will be truncated
    // to the previous packets length, because the receive logic thinks
    // this is now the maximum length of the underlying receive buffer.
    rcvpkt.setLength (m_rcvbuf.length);
    
    try
    {
      m_unicastSocket.setSoTimeout ( 100 );
      m_unicastSocket.receive (rcvpkt);
    }
    catch (SocketTimeoutException stex )
    {
      // ignore this type
      return -1;
    }
    catch (Exception ex)
    {
      m_errstring = "EsmUnicastSocket.readPacket : " +
              "DatagramSocket receive exception - " + ex.toString ();
      UdpRptrUiConstants.popup_msgbox (0, m_errstring);
      return -1;
    }
    
    System.arraycopy ( m_rcvbuf, 0, buf, 0, rcvpkt.getLength() );
    
    return rcvpkt.getLength ();
  }
  
  
  /**send a packet*/
  @Override
  protected boolean sendPacket ( byte[] buf, int len, String ipaddr, int port ) throws IOException
  {
    // if open failed, just return
    if ( m_openError )
    {
      m_errstring = "EsmUnicastSocket.sendPacket : " +
                    "there was a previous socket 'open' error";
      return false;
    }
    
    // create Datagram packet to broadcast
    InetAddress inetAddr = InetAddress.getByName ( ipaddr );
    DatagramPacket datagram = new DatagramPacket (
            buf, len, inetAddr, port );
    
    // broadcast the packet
    try
    {
      m_unicastSocket.send ( datagram );
      return true;
    }
    catch ( Exception ex )
    {
      m_errstring = "EsmUnicastSocket.sendPacket : " + 
                    "DatagramSocket send exception - " + 
                    ex.toString ();
      return false;
    }
  }
  
}
