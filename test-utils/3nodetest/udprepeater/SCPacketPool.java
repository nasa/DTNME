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

import java.util.LinkedList;
import java.util.Queue;

public class SCPacketPool
{

  private static SCPacketPool m_instance;

  private Queue<SCPacket> m_scpacket_pool;

  private static final Object m_sync_lock = new Object();

  private int m_num_packets_created;

  /**singleton Instance method*/
  static public SCPacketPool Instance()
  {
    if (m_instance == null)
    {
      synchronized (m_sync_lock)
      {
        if (m_instance == null)
        {
          m_instance = new SCPacketPool ();
        }
      }
    }

    return m_instance;
  }

  /**constructor*/
  private SCPacketPool()
  {
    synchronized ( m_sync_lock )
    {
      m_scpacket_pool = new LinkedList<SCPacket>();
      m_num_packets_created = 0;
    }
  }

  /** get an SCPacket for use */
  public SCPacket getSCPacket()
  {
    synchronized ( m_sync_lock )
    {
      if ( m_scpacket_pool.isEmpty() )
      {
        ++m_num_packets_created;
        return new SCPacket();
      }
      else
      {
        return m_scpacket_pool.poll();
      }
    }
  }

  /** return an SCPacket to the pool for reuse */
  public void releaseSCPacket( SCPacket scpkt )
  {
    if ( null != scpkt )
    {
      synchronized ( m_sync_lock )
      {
        scpkt.clear();
        m_scpacket_pool.add(scpkt);
      }
    }
  }

  /** get the total number of SCPackets that have been created */
  public int getNumSCPacketsCreated()
  {
    synchronized ( m_sync_lock )
    {
      return m_num_packets_created;
    }
  }

  /** get the number of SCPackets currently available in the pool */
  public int getNumSCPacketsInPool()
  {
    synchronized ( m_sync_lock )
    {
      return m_scpacket_pool.size();
    }
  }
}
