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

import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.util.concurrent.ScheduledFuture;

public class UdpRptrPacketCollector implements Runnable
{
  private static UdpRptrPacketCollector m_instance;
  private static final Object m_sync_lock = new Object ();

  private Thread m_thread;
  private UdpRptrCoMainFrame m_controller;
  
  private UdpRptrAbstractSocket m_multisock;
  private UdpRptrUnicastSocket m_unisocket;

  private UdpRptrUiConstants m_uicon;
  private static final UdpRptrThreadPools m_thread_pool = UdpRptrThreadPools.Instance ();

  private boolean m_capture_mode;
  private boolean m_stop;

  private boolean m_capture_packet;
  private byte[] m_captured_rcvbuf;
  private int m_captured_rcvbuf_len;
  private StringBuilder m_captured_pkt;
  private StringBuilder m_sb_seq_ctr;
  
  private String m_input_ip_address;
  private int    m_input_port;
  private String m_output_ip_address;
  private int    m_transmit_to_port;
  private boolean m_add_seq_ctr;
  private int m_seq_ctr;

  private int m_gen_payload_len;
  private int m_gen_rate;
  private int m_gen_total_pkts;
  private byte[] m_gen_sndbuf;
  private ScheduledFuture<?> m_generate_task;
  

  private boolean m_capture_only;

  private boolean m_file_capture;
  private OutputStream m_file;
  private boolean m_first_file_error;

  private long m_packets_rcvd;
  private long m_packets_discarded;

  private boolean m_do_stats;
  private long m_packets_this_sec;
  private long m_bytes_this_sec;
  private int m_max_packet_len;
  private int m_min_packet_len;
  private long m_packets_per_sec;
  private long m_bits_per_sec;
  private long m_max_packets_per_sec;
  private ScheduledFuture<?> m_stats_task;

  private class statusTask implements Runnable
  {
    @Override
    public void run()
    {
      m_do_stats = true;

      if ( m_captured_rcvbuf_len > 0 )
      {
        m_captured_pkt.setLength ( 0 );
        for ( int ix=0; ix<m_captured_rcvbuf_len; ++ix )
        {
          m_captured_pkt.append ( String.format ( "%02x", m_captured_rcvbuf[ix] ) );
        }
        m_captured_rcvbuf_len = 0;
      }
    }
  }

  private class sendPacketTask implements Runnable
  {
    @Override
    public void run()
    {
      if ( m_do_stats )
      {
        m_do_stats = false;
        m_packets_per_sec = m_packets_this_sec;
        if ( m_packets_per_sec > m_max_packets_per_sec )
        {
          m_max_packets_per_sec = m_packets_per_sec;
        }
        m_bits_per_sec = m_bytes_this_sec * 8;
        m_packets_this_sec = 0;
        m_bytes_this_sec = 0;
      }

      if ( (m_gen_total_pkts == 0) || (m_packets_rcvd < m_gen_total_pkts) )
      {
        synchronized ( m_sync_lock )
        {
          if ( ! m_stop )
          {
            // TODO - send packet on its way...
            try
            {
              if ( m_add_seq_ctr )
              {
                ++m_seq_ctr;
                m_sb_seq_ctr.setLength(0);
                m_sb_seq_ctr.append(String.format("%010d", m_seq_ctr));
                System.arraycopy ( m_sb_seq_ctr.toString ().getBytes (), 0, m_gen_sndbuf, m_gen_payload_len-10, 10 );
              }
              m_unisocket.sendPacket(m_gen_sndbuf, m_gen_payload_len, m_output_ip_address, m_transmit_to_port);

              // update stats
              ++m_packets_rcvd;
              ++m_packets_this_sec;
              m_bytes_this_sec += m_gen_payload_len;
            }
            catch ( IOException ex )
            {
              m_uicon.popup_msgbox(0, "Error sending pkt: " + ex.getMessage() );
            }
          }
        }
      }
      else
      {
        m_stop = true;
      }
    }
  }


  /** singleton Instance method */
  public static UdpRptrPacketCollector Instance ( )
  {
    if (m_instance == null)
    {
      synchronized (m_sync_lock)
      {
        if (m_instance == null)
        {
          m_instance = new UdpRptrPacketCollector ();
        }
      }
    }

    return m_instance;
  }


  /**constructor*/
  private UdpRptrPacketCollector ()
  {
    m_uicon = UdpRptrUiConstants.Instance();

    m_captured_rcvbuf = new byte[65636];
    m_captured_pkt = new StringBuilder (4000);
    m_sb_seq_ctr = new StringBuilder(20);
  }

  public void init ( String input_ip_address, int input_port, 
                     String output_ip_address, int transmit_to_port,
                     boolean add_seq_ctr )
  {
    m_input_ip_address = input_ip_address;
    m_input_port = input_port;
    m_output_ip_address = output_ip_address;
    m_transmit_to_port = transmit_to_port;
    m_add_seq_ctr = add_seq_ctr;
    m_capture_only = false;
  }

  public void setCaptureOnly()
  {
    m_capture_only = true;
  }
  
  /** start the thread running */
  public void start (UdpRptrCoMainFrame controller, int totpkts)
  {
    synchronized (m_sync_lock)
    {
      if ( null == m_thread )
      {
        m_controller = controller;
        m_gen_total_pkts = totpkts;
        m_capture_mode = true;
        m_stop = false;

        m_thread = new Thread (this, "PacketCollector");
        m_thread.start ();
      }
    }
  }
  
  /** stop the thread */
  public void stop ()
  {
    synchronized ( m_sync_lock )
    {
      m_stop = true;
      if ( null != m_stats_task )
      {
        m_stats_task.cancel ( true );
        m_stats_task = null;
      }
      if ( null != m_generate_task )
      {
        m_generate_task.cancel( true );
        m_generate_task = null;
      }
    }
  }
  
  
  /**Runnable interface method*/
  @Override
  public void run ()
  {
    if (m_capture_mode) {
        run_capture_mode();
    }
    else {
        run_generate_mode();
    }
    
    m_thread = null;
  }
  
  public void run_capture_mode ()
  {
    m_capture_packet = false;
    m_captured_rcvbuf_len = 0;

    m_do_stats = false;
    m_packets_this_sec = 0;
    m_bytes_this_sec = 0;
    m_max_packet_len = 0;
    m_min_packet_len = Integer.MAX_VALUE;
    m_packets_per_sec = 0;
    m_max_packets_per_sec = 0;
    m_bits_per_sec = 0;

    m_packets_rcvd = 0;
    m_packets_discarded = 0;

    m_multisock = new UdpRptrMulticastSocket();
    if ( ! m_multisock.open ( m_input_port, m_input_ip_address ) )
    {
      UdpRptrUiConstants.popup_msgbox ( 0, "Error opening input socket: " + 
                                           m_multisock.m_errstring );
      return;
    }

    if ( !m_capture_only )
    {
      m_unisocket = new UdpRptrUnicastSocket();
      if ( ! m_unisocket.open ( 0, m_output_ip_address ) )
      {
        UdpRptrUiConstants.popup_msgbox ( 0, "Error opening output socket: " +
                                             m_unisocket.m_errstring );
        return;
      }
    }

    
    int len;
    // sequence ctr is now stored in the last 10 bytes of the send buffer
    byte[] rcvbuf = new byte[65536];
    
    m_stats_task = m_thread_pool.scheduleAtFixedRate ( new statusTask (), 1000, 1000);
    

    // read the multicast socket and update the model with the new data
    // within an infinite loop for as long as this program is running
    while ( ! m_stop )
    {

      len = m_multisock.readPacket ( rcvbuf );

      if ( m_do_stats )
      {
        m_do_stats = false;
        m_packets_per_sec = m_packets_this_sec;
        if ( m_packets_per_sec > m_max_packets_per_sec )
        {
          m_max_packets_per_sec = m_packets_per_sec;
        }
        m_bits_per_sec = m_bytes_this_sec * 8;
        m_packets_this_sec = 0;
        m_bytes_this_sec = 0;
      }

      if ( len > 0 )
      {
        if ( len < m_min_packet_len ) {
            m_min_packet_len = len;
        }
        if ( len > m_max_packet_len ) {
            m_max_packet_len = len;
        }

        if ( m_capture_packet )
        {
          m_capture_packet = false;
          System.arraycopy ( rcvbuf, 0, m_captured_rcvbuf, 0, len );
          m_captured_rcvbuf_len = len;
        }

        if ( m_file_capture )
        {
          try
          {
            m_file.write ( rcvbuf, 0, len);
          }
          catch ( IOException ex )
          {
            if ( m_first_file_error )
            {
              m_first_file_error = false;
              m_uicon.popup_msgbox(0, "Error writing to file: " + ex.getMessage() );
            }
          }
        }

        synchronized ( m_sync_lock )
        {
          if ( ! m_stop )
          {
            // TODO - send packet on its way...
            try
            {
              if ( m_add_seq_ctr )
              {
                ++m_seq_ctr;
                m_sb_seq_ctr.setLength(0);
                m_sb_seq_ctr.append(String.format("%010d", m_seq_ctr));
                System.arraycopy ( m_sb_seq_ctr.toString ().getBytes (), 0, rcvbuf, len, 10 );
                len += 10;
              }
              if ( !m_capture_only ) 
              {
                m_unisocket.sendPacket(rcvbuf, len, m_output_ip_address, m_transmit_to_port);
              }
              // update stats
              ++m_packets_rcvd;
              ++m_packets_this_sec;
              m_bytes_this_sec += len;
            }
            catch ( IOException ex )
            {
              m_uicon.popup_msgbox(0, "Error sending pkt: " + ex.getMessage() );
            }
          }
        }

        //???  m_scp_pool.releaseSCPacket(pkt);
      }

      if ( (m_gen_total_pkts > 0) && (m_packets_rcvd >= m_gen_total_pkts) )
      {
        m_controller.actionPerformed_Stop(null);
        break;
      }

    }
    
    m_multisock.close ();
    m_multisock = null;

    if ( !m_capture_only )
    {
      m_unisocket.close();
      m_unisocket = null;
    }
  }

  public void run_generate_mode ()
  {
    m_capture_packet = false;
    m_captured_rcvbuf_len = 0;

    m_do_stats = false;
    m_packets_this_sec = 0;
    m_bytes_this_sec = 0;
    m_max_packet_len = m_gen_payload_len;
    m_min_packet_len = m_gen_payload_len;
    m_packets_per_sec = 0;
    m_max_packets_per_sec = 0;
    m_bits_per_sec = 0;

    m_packets_rcvd = 0;
    m_packets_discarded = 0;

    m_unisocket = new UdpRptrUnicastSocket();
    
    if ( ! m_unisocket.open ( 0, m_output_ip_address ) )
    {
      UdpRptrUiConstants.popup_msgbox ( 0, "Error opening output socket: " +
                                           m_unisocket.m_errstring );
      return;
    }

    m_gen_sndbuf = new byte[65636];
    char letter = 'A';
    for (int ix=0; ix<m_gen_payload_len; ++ix)
    {
      m_gen_sndbuf[ix] = (byte) letter;
      if (++letter > 'Z')
      {
        letter = 'A';
      }
    }
    
    int rate_ms = 1000000 / m_gen_rate;
    m_generate_task =  m_thread_pool.scheduleAtFixedRateMicroSecs ( new sendPacketTask (), 10, rate_ms); 

    m_stats_task = m_thread_pool.scheduleAtFixedRate ( new statusTask (), 1010, 1000);
    
    // read the multicast socket and update the model with the new data
    // within an infinite loop for as long as this program is running
    while ( ! m_stop )
    {
      try
      {
        Thread.sleep(1000);
      } catch (Exception ex) {};
    }
    
    m_unisocket.close();

    m_multisock = null;
    m_unisocket = null;
    
    m_gen_sndbuf = null;
    
    if ( (m_gen_total_pkts > 0) && (m_packets_rcvd >= m_gen_total_pkts) )
    {
      m_controller.actionPerformed_StopGenerating(null);
    }
  }
  
  public long getPacketsReceived()
  {
    return m_packets_rcvd;
  }
  
  public long getPacketsDiscarded()
  {
    return m_packets_discarded;
  }

  
  /** start the thread running */
  public void startGenerating ( UdpRptrCoMainFrame controller,
                                int payloadlen, 
                                int rate, int totpkts )
  {
    synchronized (m_sync_lock)
    {
      if ( null == m_thread )
      {
        m_controller = controller;
        m_capture_mode = false;
        m_stop = false;

        m_gen_payload_len = payloadlen;
        m_gen_rate = rate;
        m_gen_total_pkts = totpkts;
        
        m_thread = new Thread (this, "PacketCollector");
        m_thread.start ();
      }
    }
  }
  
  public long getPacketsPerSec()
  {
    return m_packets_per_sec;
  }

  public long getMaxPacketsPerSec()
  {
    return m_max_packets_per_sec;
  }



  public long getBitsPerSec()
  {
    return m_bits_per_sec;
  }

  public int getMinPacketLen()
  {
    if ( Integer.MAX_VALUE == m_min_packet_len )
      return 0;
    else
      return m_min_packet_len;
  }

  public int getMaxPacketLen()
  {
    return m_max_packet_len;
  }

  public int getSeqCtr()
  {
    return m_seq_ctr;
  }

  public void capturePacket()
  {
    m_capture_packet = true;
  }

  public boolean haveCapturedPacket()
  {
    return (m_captured_pkt.length () > 0);
  }

  public void resetSeqCtr()
  {
    m_seq_ctr = 0;
  }
  
  public String getCapturedPacket()
  {
    String result = "";
    if ( m_captured_pkt.length () > 0 )
    {
      result = m_captured_pkt.toString ();
      m_captured_pkt.setLength ( 0 );
    }
    return result;
  }

  public boolean startFileCapture ( String fname )
  {
    boolean result = false;
    if ( ! m_file_capture )
    {
      try
      {
        m_file = new FileOutputStream(fname);

        m_file_capture = true;
        m_first_file_error = true;
        result = true;
      }
      catch (IOException ex)
      {
        m_uicon.popup_msgbox ( 0, "Error opening file: " + ex.getMessage () + " - Aborted" );
        m_file = null;
      }
    }
    return result;
  }

  public void stopFileCapture ()
  {
    if ( m_file_capture )
    {
      m_file_capture = false;

      try
      {
        m_file.close ();
      }
      catch (IOException ex)
      {
        m_uicon.popup_msgbox ( 0, "Error closing file: " + ex.getMessage () + " - Aborted" );
        m_file = null;
      }
      m_file = null;
    }
  }
}
