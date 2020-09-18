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

import java.awt.*;
import java.awt.datatransfer.Clipboard;
import java.awt.datatransfer.StringSelection;
import java.awt.event.*;
import javax.swing.DefaultListModel;
import javax.swing.JCheckBox;
import javax.swing.JTextField;

public class UdpRptrVwMainFrame extends UdpRptrUiMainFrame
        implements UdpRptrViewInterface
{
  private static final long serialVersionUID = 1L;
  
  private UdpRptrControllerBase m_controller;
  private UdpRptrUiConstants m_uicon = UdpRptrUiConstants.Instance ();
  private UdpRptrPacketCollector m_collector = UdpRptrPacketCollector.Instance ();
  private UdpRptrResourceFile m_rsrc = UdpRptrResourceFile.Instance ();

  private boolean m_enabled;
  private boolean m_started;
  
  // object used for synchronization
  private final Object m_sync_lock = new Object ();

  private DefaultListModel m_listModel;

  /** constructor */
  public UdpRptrVwMainFrame ()
  {
    // view is disabled by default
    disableEvents (AWTEvent.WINDOW_EVENT_MASK);
    jButton_stop.setVisible(false);
    jButton_stopGenerating.setVisible(false);

    jButton_capturePkt.setVisible(false);
    jButton_StopFileCapture.setVisible(false);
    jButton_StartFileCapture.setVisible (false);
    jTextField_filename.setVisible (false);

    m_enabled = false;
    m_started = false;

    this.setTitle ( "UDP Repeater (" + m_uicon.m_host_name + "  " +
                    m_uicon.m_ip_address + ")" );

    m_listModel = new DefaultListModel ();
    jList_messages.setModel ( m_listModel );


    // Read in the saved settings from the resource file
    loadSettings();

    pack ();
  }

  /** UdpRptrViewInterface method implementation */
  @Override
  public void enableView ()
  {
    enableEvents (AWTEvent.WINDOW_EVENT_MASK | AWTEvent.COMPONENT_EVENT_MASK);
    m_enabled = true;
  }
  
  /** UdpRptrViewInterface method implementation */
  @Override
  public void disableView ()
  {
    disableEvents (AWTEvent.WINDOW_EVENT_MASK);
    m_enabled = false;
  }
  
  /** UdpRptrViewInterface method implementation */
  @Override
  public boolean isViewEnabled ()
  {
    return m_enabled;
  }
  
  
  /** UdpRptrViewInterface method implementation */
  @Override
  public void showView ()
  {
    enableView ();
    if ( ! this.isVisible () ) this.setVisible (true);
  }
  
  /** UdpRptrViewInterface method implementation */
  @Override
  public void hideView ()
  {
    disableView ();
    this.setVisible (false);
  }
  
  /** UdpRptrViewInterface method implementation */
  @Override
  public void closeView ()
  {
    hideView();

    unregisterListener();
    
    m_controller = null;
    
    m_uicon = null;
  }
  
  /** UdpRptrViewInterface method implementation */
  @Override
  public void updateView ()
  {
    m_uicon.swingInvokeLater ( this, "edtUpdateView" );
  }

  public void edtUpdateView()
  {
    jLabel_totalPacktes.setText( String.valueOf(m_collector.getPacketsReceived()));
    jLabel_pktsPerSec.setText ( String.valueOf ( m_collector.getPacketsPerSec () ) );
    jLabel_kbitsPerSec.setText ( m_uicon.fmtDataRate ( m_collector.getBitsPerSec () ) );
    jLabel_maxPktsPerSec.setText ( String.valueOf ( m_collector.getMaxPacketsPerSec () ) );
    jLabel_minPktLen.setText ( String.valueOf ( m_collector.getMinPacketLen () ) );
    jLabel_maxPktLen.setText ( String.valueOf ( m_collector.getMaxPacketLen () ) );
    jLabel_seqCtr.setText ( String.valueOf ( m_collector.getSeqCtr () ) );
    if ( m_collector.haveCapturedPacket () )
    {
      append_msg ( m_collector.getCapturedPacket () );
    }
  }
  
  /** UdpRptrViewInterface method implementation */
  @Override
  public void registerListener (UdpRptrControllerBase listener)
  {
    // save a reference to the controller
    m_controller = listener;
    
    // pull down menus
    jMenuItem_fileExit.addActionListener ( m_controller );
    jMenuItem_copyToClipboard.addActionListener ( m_controller );
    
    // buttons
    jButton_capturePkt.addActionListener ( m_controller );
    jButton_StopFileCapture.addActionListener ( m_controller );
    jButton_StartFileCapture.addActionListener ( m_controller );

    jButton_resetSeqCtr1.addActionListener(m_controller);
    jButton_startCaptureOnly.addActionListener(m_controller);
    jButton_start.addActionListener ( m_controller );
    jButton_stop.addActionListener ( m_controller );
    
    jButton_startGenerating.addActionListener ( m_controller );
    jButton_stopGenerating.addActionListener ( m_controller );
  }
  
  /** UdpRptrViewInterface method implementation */
  @Override
  public void unregisterListener ()
  {
    if ( m_controller != null )
    {
      // pull down menus
      jMenuItem_fileExit.removeActionListener ( m_controller );
      jMenuItem_copyToClipboard.removeActionListener ( m_controller );

      // buttons
      jButton_capturePkt.removeActionListener ( m_controller );
      jButton_StopFileCapture.removeActionListener ( m_controller );
      jButton_StartFileCapture.removeActionListener ( m_controller );

      jButton_resetSeqCtr1.removeActionListener(m_controller);
      jButton_startCaptureOnly.removeActionListener(m_controller);
      jButton_start.removeActionListener ( m_controller );
      jButton_stop.removeActionListener ( m_controller );

      jButton_startGenerating.removeActionListener ( m_controller );
      jButton_stopGenerating.removeActionListener ( m_controller );
      
      m_controller = null;
    }
  }
  
  /** Overridden so we can exit when window is closed */
  @Override
  protected void processWindowEvent (WindowEvent e)
  {
    // if view is disabled ignore event
    if (! m_enabled) return;
    
    // pass this event along to the controller for processing
    if (m_controller != null)
    {
      m_controller.processWindowEvent (e);
      super.processWindowEvent (e);
    }
    else
    {
      super.processWindowEvent (e);
      if (e.getID () == WindowEvent.WINDOW_CLOSING)
      {
        if (m_controller != null)
          ((UdpRptrCoMainFrame)m_controller).exit_task (0);
        else
          System.exit (0);  // how'd that happen?
      }
    }
    
    // if this is the first time through here set the scrolling message
    // box size and grab its initial size along with the main frame size
    if (e.getID () == WindowEvent.WINDOW_OPENED)
    {
//      // set the initial size to allow for 3 messages on the display
//      jTextArea_msgWindow.setRows (4);
//      jPanel_msgPanel.invalidate ();
//      jPanel_msgPanel.validate ();
      
      synchronized ( m_sync_lock )
      {
        jPanel_mainPanel.invalidate ();
        jPanel_mainPanel.validate ();
      }
      
//      // grab initial message box and main window frame sizes
//      m_msgboxSize = new Dimension (jPanel_msgPanel.getSize ());
//      m_msgboxMinimumSize = new Dimension (jPanel_msgPanel.getSize ());
//      updateFrameSize ();
    }
    
  }
  
  // Adjsut the size of the form to show as many servers as possible
  protected void resizeForm ()
  {
  }

  public String getInputIpAddress()
  {
    return jTextField_inputIpAddress.getText();
  }

  public String getInputPort()
  {
    return jTextField_inputPort.getText();
  }
  
  public String getOutputIpAddress()
  {
    return jTextField_outputIpAddress.getText();
  }
  
  public String getTransmitToPort()
  {
    return jTextField_transmitToPort.getText();
  }

  public void toggleStartStop ()
  {
    // Start/Stop Capturing
    m_started = ! m_started;
    
    jCheckBox_addSeqCtr.setEnabled(!m_started);
    jButton_resetSeqCtr1.setEnabled(!m_started);
    jButton_startCaptureOnly.setVisible(!m_started);
    jButton_start.setVisible(!m_started);
    jButton_stop.setVisible(m_started);

    jButton_capturePkt.setVisible(m_started);
    jButton_StartFileCapture.setVisible (m_started);
    jButton_StopFileCapture.setVisible (false);
    jTextField_filename.setVisible (m_started);

    jTextField_payloadLength.setEnabled(!m_started);
    jTextField_genRate.setEnabled(!m_started);
    jTextField_totalPktsToGen.setEnabled(!m_started);
    jButton_startGenerating.setVisible(true);
    jButton_startGenerating.setEnabled(!m_started);
    jButton_stopGenerating.setVisible(false);
    
    jTextField_inputIpAddress.setEnabled(!m_started);
    jTextField_inputPort.setEnabled(!m_started);
    jTextField_outputIpAddress.setEnabled(!m_started);
    jTextField_transmitToPort.setEnabled ( !m_started );
  }

  public void toggleStartStopGenerating ()
  {
    // Start/Stop Capturing
    m_started = ! m_started;
    
    jCheckBox_addSeqCtr.setEnabled(!m_started);
    jButton_resetSeqCtr1.setEnabled(!m_started);
    jButton_startCaptureOnly.setVisible(true);
    jButton_startCaptureOnly.setEnabled(!m_started);
    jButton_start.setVisible(true);
    jButton_start.setEnabled(!m_started);
    jButton_stop.setVisible(false);

    jButton_capturePkt.setVisible (false);
    jButton_StartFileCapture.setVisible (false);
    jButton_StopFileCapture.setVisible (false);
    jTextField_filename.setVisible (false);

    jTextField_payloadLength.setEnabled(!m_started);
    jTextField_genRate.setEnabled(!m_started);
    jTextField_totalPktsToGen.setEnabled(!m_started);
    jButton_startGenerating.setEnabled(!m_started);
    jButton_stopGenerating.setVisible(m_started);
    
    jTextField_inputIpAddress.setEnabled(!m_started);
    jTextField_inputPort.setEnabled(!m_started);
    jTextField_outputIpAddress.setEnabled(!m_started);
    jTextField_transmitToPort.setEnabled ( !m_started );
  }
  
  /** prepend a new message to the message window */
  public void append_msg (String msg)
  {
    m_uicon.swingInvokeLater ( this, "edtAppendMsg", msg );
  }

  public void edtAppendMsg ( String msg )
  {
    m_listModel.add ( 0, msg );
    if ( m_listModel.getSize () > 1000 )
    {
      m_listModel.setSize ( 1000 );
    }
  }

  public void copyToClipboard ()
  {
    Clipboard clip = Toolkit.getDefaultToolkit ().getSystemClipboard ();
    String msgs = getTitle () + " messages: \n";
    if ( 0 == m_listModel.size () )
    {
      msgs += "<None>\n";
    }
    else
    {
      for ( int ix=0; ix<m_listModel.size (); ++ix )
      {
        msgs += m_listModel.get ( ix ) + "\n";
      }
    }

    StringSelection xfer = new StringSelection ( msgs );
    clip.setContents ( xfer, null );
  }

  public void getCapturedPacket()
  {
    jButton_capturePkt.setEnabled ( false );

  }

  public String getFilename()
  {
    return jTextField_filename.getText ();
  }

  public void startFileCapture()
  {
    m_rsrc.putResource ( jTextField_filename );
    jButton_StartFileCapture.setVisible ( false );
    jButton_StopFileCapture.setVisible ( true );
    jTextField_filename.setEnabled ( false );
  }

  public void stopFileCapture()
  {
    jButton_StopFileCapture.setVisible ( false );
    jButton_StartFileCapture.setVisible ( true );
    jTextField_filename.setEnabled ( true );
  }

  private void loadTextField ( JTextField fld )
  {
    String key = fld.getName ();
    if ( m_rsrc.isResourceAvailable ( key ) )
    {
      fld.setText ( m_rsrc.getResource ( key ) );
    }
  }

  private void loadCheckBox ( JCheckBox cb )
  {
    String key = cb.getName ();
    if ( m_rsrc.isResourceAvailable ( key ) )
    {
      cb.setSelected ( m_rsrc.getResource ( key ).equals ("true") );
    }
  }

  private void loadSettings()
  {
    // save text fields to a resource file
    loadTextField ( jTextField_inputIpAddress );
    loadTextField ( jTextField_inputPort );
    loadTextField ( jTextField_outputIpAddress );
    loadTextField ( jTextField_transmitToPort );
    loadTextField ( jTextField_filename );
    
    loadTextField ( jTextField_genRate );
    loadTextField ( jTextField_payloadLength );
    loadTextField ( jTextField_totalPktsToGen );
    
    loadCheckBox ( jCheckBox_addSeqCtr );
    loadCheckBox ( jCheckBox_addSeqCtr );
  }

  public void saveSettings()
  {
    // save text fields to a resource file
    m_rsrc.putResource ( jTextField_inputIpAddress );
    m_rsrc.putResource ( jTextField_inputPort );
    m_rsrc.putResource ( jTextField_outputIpAddress );
    m_rsrc.putResource ( jTextField_transmitToPort );
    m_rsrc.putResource ( jTextField_filename );
    
    m_rsrc.putResource ( jTextField_genRate );
    m_rsrc.putResource ( jTextField_payloadLength );
    m_rsrc.putResource ( jTextField_totalPktsToGen );
    
    m_rsrc.putResource ( jCheckBox_addSeqCtr );
  }

}

