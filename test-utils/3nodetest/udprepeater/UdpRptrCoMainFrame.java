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
import java.awt.event.*;
import java.io.File;
import javax.swing.*;

public class UdpRptrCoMainFrame extends UdpRptrControllerBase
{
  private UdpRptrVwMainFrame m_view;
  private UdpRptrUiConstants m_uicon;
  private UdpRptrThreadPools m_tpools;

  private UdpRptrPacketCollector m_pktCollector;

  private MouseEvent m_mouse_event;
  
  private long m_num_gui_refreshes;

  /** constructor for the main frame controller */
  public UdpRptrCoMainFrame ()
  {
      parseCmdLineArgs();
  }
  
  private void parseCmdLineArgs()
  {
    internal_init();
  }

  /* perform init that is frowned upon within the constructor */
  private void internal_init()
  {
    // create the main frame view
    super.m_view = m_view = new UdpRptrVwMainFrame ();
    
    m_uicon = UdpRptrUiConstants.Instance ();
      
    // register ourself as the listener
    m_view.registerListener (this);

    // Validate frames that have preset sizes, otherwise,
    // Pack frames that have useful preferred size info, e.g. from their layout
    m_view.validate ();

    // Center the window
    Dimension screenSize = Toolkit.getDefaultToolkit ().getScreenSize ();
    Dimension frameSize = m_view.getSize ();

    if (frameSize.height > screenSize.height)
    {
      frameSize.height = screenSize.height;
    }
    if (frameSize.width > screenSize.width)
    {
      frameSize.width = screenSize.width;
    }

    m_view.setLocation ((screenSize.width - frameSize.width) / 2,
            (screenSize.height - frameSize.height) / 2);

    // update the view
    m_view.updateView ();

    // let the world see what we have wrought
    m_view.showView ();

    m_pktCollector = UdpRptrPacketCollector.Instance();

    // start the periodic display refesh timer
    m_tpools = UdpRptrThreadPools.Instance();
    m_tpools.scheduleDisplayRefresh();
  }

  // overridden EsmControllerBase method:
  // controllers that use the super.m_view = m_view = new EsmVw___()
  // technique should override this method and null out their
  // m_view and then call super.nullifyTheView() to clear the base
  // controller's m_view
  @Override
  public void nullifyTheView ()
  {
    m_view = null;
    super.nullifyTheView ();
  }

  // overridden EsmControllerBase method
  @Override
  public void dispose ()
  {
    nullifyTheView ();

    m_uicon = null;
    m_pktCollector = null;

    m_tpools.shutdown();

    super.dispose ();
  }
  
  // overridden EsmControllerBase method
  @Override
  public void exit_controller ()
  {
    m_stop = true;

    //if ( m_displayRefreshTimer != null ) m_displayRefreshTimer.cancel ();
    if ( m_pktCollector != null ) {
          m_pktCollector.stop ();
    }

    m_tpools.shutdown();

    super.exit_controller ();
  }

  
  /** ActionListener interface implementation method base class override */
  @Override
  public void actionPerformed (ActionEvent e)
  {
    int eventnum = e.getID ();
    
    switch (eventnum)
    {
      case ActionEvent.ACTION_PERFORMED:
        String cmd = e.getActionCommand ();
        
        if (cmd.equalsIgnoreCase ("exit")) {
          actionPerformed_Exit (e);
        }
        else if (cmd.equalsIgnoreCase ("captureonly")) {
          actionPerformed_CaptureOnly (e);
        }
        else if (cmd.equalsIgnoreCase ("start")) {
          actionPerformed_Start (e);
        }
        else if (cmd.equalsIgnoreCase ("stop")) {
          actionPerformed_Stop (e);
        }
        else if (cmd.equalsIgnoreCase ("copytoclipboard")) {
          actionPerformed_CopyToClipBoard (e);
        }
        else if (cmd.equalsIgnoreCase ("capturepkt")) {
          actionPerformed_CapturePacket (e);
        }
        else if (cmd.equalsIgnoreCase ("startfilecapture")) {
          actionPerformed_StartFileCapture (e);
        }
        else if (cmd.equalsIgnoreCase ("stopfilecapture")) {
          actionPerformed_StopFileCapture (e);
        }
        else if (cmd.equalsIgnoreCase ("startGenerating")) {
          actionPerformed_StartGenerating (e);
        }
        else if (cmd.equalsIgnoreCase ("stopGenerating")) {
          actionPerformed_StopGenerating (e);
        }
        else if (cmd.equalsIgnoreCase ("resetSeqCtr")) {
          actionPerformed_ResetSeqCtr (e);
        } else {
          System.out.println ("UdpRptrCoMainFrame.actionPerformed : unknown ActionCommand received=" + cmd);
        }
        break;
        
      default:
        // System.out.println ("EsmCoMainFrame.actionPerformed : received event ID=" + eventnum);
        break;
    }
    
  }
  
  
  /** window event processing method base class override */
  @Override
  public void processWindowEvent (WindowEvent e)
  {
    int eventnum = e.getID ();
    
    switch (eventnum)
    {
      case WindowEvent.WINDOW_CLOSING:
        exit_task (0);
        break;
      case WindowEvent.WINDOW_OPENED:
        break;
        
        
      default:
        // System.out.println ("EsmCoMainFrame.processWindowEvent : received event ID=" + eventnum);
        break;
        
    }
    
  }

  public void resizeForm ()
  {
    m_view.resizeForm ();
  }

  public void actionPerformed_Exit (ActionEvent e)
  {
    m_pktCollector.stop();
    exit_task (0);
  }
  
  public void actionPerformed_Start (ActionEvent e)
  {
    m_view.saveSettings();
    m_view.toggleStartStop();
    
    int input_port = Integer.parseInt(m_view.getInputPort());
    int transmit_to_port = Integer.parseInt(m_view.getTransmitToPort());
    m_pktCollector.init(m_view.getInputIpAddress(), input_port,
                        m_view.getOutputIpAddress(), transmit_to_port,
                        m_view.jCheckBox_addSeqCtr.isSelected() );
    
    int totpkts = Integer.parseInt(m_view.jTextField_totalPktsToGen.getText());
    m_pktCollector.start (this, totpkts);
  }

  public void actionPerformed_CaptureOnly (ActionEvent e)
  {
    m_view.saveSettings();
    m_view.toggleStartStop();

    int input_port = Integer.parseInt(m_view.getInputPort());
    int transmit_to_port = Integer.parseInt(m_view.getTransmitToPort());
    m_pktCollector.init(m_view.getInputIpAddress(), input_port,
                        m_view.getOutputIpAddress(), transmit_to_port,
                        m_view.jCheckBox_addSeqCtr.isSelected() );
    m_pktCollector.setCaptureOnly();

    int totpkts = Integer.parseInt(m_view.jTextField_totalPktsToGen.getText());
    m_pktCollector.start (this, totpkts);
  }

  public void actionPerformed_Stop (ActionEvent e)
  {
    m_pktCollector.stopFileCapture ();
    m_view.stopFileCapture ();
    m_view.toggleStartStop();
    m_pktCollector.stop();
  }

  public void actionPerformed_StartGenerating (ActionEvent e)
  {
    boolean addseqctr = m_view.jCheckBox_addSeqCtr.isSelected();
    int payloadlen = Integer.parseInt(m_view.jTextField_payloadLength.getText());
    
    if ( addseqctr && payloadlen < 10 ) {
        m_uicon.popup_msgbox(0, "Abort - Payload Length must be at least 10 if using sequence counter");
        return;
    }
    
    int rate = Integer.parseInt(m_view.jTextField_genRate.getText());
    int totpkts = Integer.parseInt(m_view.jTextField_totalPktsToGen.getText());

    int input_port = Integer.parseInt(m_view.getInputPort());
    int transmit_to_port = Integer.parseInt(m_view.getTransmitToPort());
    m_pktCollector.init(m_view.getInputIpAddress(), input_port, 
                        m_view.getOutputIpAddress(), transmit_to_port,
                        addseqctr );
    
    m_view.saveSettings();
    m_view.toggleStartStopGenerating();
    m_pktCollector.startGenerating(this, payloadlen, rate, totpkts);
  }

  public void actionPerformed_StopGenerating (ActionEvent e)
  {
    m_pktCollector.stopFileCapture ();
    m_view.stopFileCapture ();
    m_view.toggleStartStopGenerating();
    m_pktCollector.stop();
  }

  public void actionPerformed_ResetSeqCtr (ActionEvent e)
  {
    m_pktCollector.resetSeqCtr();
    m_view.updateView ();
  }
        
  public void actionPerformed_CopyToClipBoard (ActionEvent e)
  {
    m_view.copyToClipboard();
  }

  public void actionPerformed_CapturePacket (ActionEvent e)
  {
    m_pktCollector.capturePacket();
  }

  public void actionPerformed_StartFileCapture (ActionEvent e)
  {
    String fname = m_view.getFilename();
    if ( fname.length () > 0 )
    {
      File file = new File(fname);
      if ( file.exists () )
      {
        // prompt overwrite?
        if ( JOptionPane.YES_OPTION !=
                m_uicon.popup_confirmbox ( m_view, 0,
                                           fname + " exists - overwrite?",
                                           "File Exists Waring",
                                           JOptionPane.YES_NO_CANCEL_OPTION ) )
        {
          return;
        }
      }

      if ( m_pktCollector.startFileCapture(fname) )
      {
        m_view.startFileCapture();
      }
    }
  }

  public void actionPerformed_StopFileCapture (ActionEvent e)
  {
    m_pktCollector.stopFileCapture();
    m_view.stopFileCapture();

  }


  /** get the view for this controller */
  public UdpRptrVwMainFrame getView ()
  {
    return m_view;
  }
  
  /** MouseListener interface overridden methods */
  @Override
  public void mouseClicked (java.awt.event.MouseEvent mouseEvent)
  {
    syncMouseClicked (mouseEvent);
  }
  
  
  
  /** thread-safe mouse event handler */
  public synchronized void syncMouseClicked (java.awt.event.MouseEvent e)
  {
    if (SwingUtilities.isEventDispatchThread ())
    {
    }
    else
    {
      this.m_mouse_event = e;
      SwingUtilities.invokeLater
              ( new Runnable ()
                  {
                    @Override
                    public void run ()
                    {
                    }
                  }
      );
    }
  }
  
  // update all of the panels
  public void updateViews ()
  {
    if ( ! m_stop )
    {
      ++m_num_gui_refreshes;
      if ( null != m_view ) {
            m_view.updateView ();
      }
    }
  }
  
  public long getNumGuiRefreshes()
  {
    return m_num_gui_refreshes;
  }

  /** exit the task */
  void exit_task (int exit_code)
  {
    exit_controller ();
    
    System.exit ( exit_code );
  }
}

