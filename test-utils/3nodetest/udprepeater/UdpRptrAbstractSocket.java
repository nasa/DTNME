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

import java.io.IOException;

public class UdpRptrAbstractSocket
{
  protected static UdpRptrAbstractSocket m_instance = null;

  protected static UdpRptrUiConstants m_uicon = UdpRptrUiConstants.Instance ();
  
  
  protected String m_errstring = "UdpRptrMulticastSocket : 'open' has not been invoked";
 
  protected boolean m_openError = true;
  
  /**singleton Instance method*/
  //static public UdpRptrAbstractSocket Instance ()
  //{
  //  if (m_instance == null)
  //  {
  //    m_instance = new UdpRptrMulticastSocket ();
  //  }
  //  return m_instance;
  //}
  
  /**
   * Creates a new instance of EsmAbstractSocket
   */
  public UdpRptrAbstractSocket ()
  {
  }
  

  /**open method - must be called once before the first use
   * of this object.  method returns true if successful,
   * or false if an error occurs (see getErrString)
   */
  public boolean open ()
  {
    return false;
  }
  
  /** overloaded open methods */
  public boolean open ( int statusport )
  {
    return open ();
  }
  
  public boolean open ( String ipmulticast )
  {
    return open ();
  }
  
  public boolean open ( int statusport, String ipmulticast )
  {
    return open ();
  }
  
  
  /**get open error string*/
  public String getErrString ()
  {
    return m_errstring;
  }
  
  
  /**true if there was an open error*/
  public boolean openError ()
  {
    return m_openError;
  }
  
  public void close ()
  {
  }
  
  /**read a packet*/
  public int readPacket ( byte[] buf )
  {
    return -1;
  }
  
  
  protected boolean sendPacket ( String pkt, String ipaddr, int port ) throws IOException
  {
    return false;
  }
  
  protected boolean sendPacket ( byte[] buf, int len, String ipaddr, int port ) throws IOException
  {
    return false;
  }
  
}

