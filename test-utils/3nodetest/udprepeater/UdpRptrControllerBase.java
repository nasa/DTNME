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

public class UdpRptrControllerBase implements ActionListener, MouseListener
{
  protected UdpRptrControllerBase m_parent;
  protected UdpRptrViewInterface m_view;
  protected boolean m_stop;
  
  // default contructor
  public UdpRptrControllerBase ()
  {
  }
  
  // alternate contructor allows the child to inform the parent when it
  // is shutting down
  public UdpRptrControllerBase ( UdpRptrControllerBase parent )
  {
    m_parent = parent;
  }
  
  // derived class should override this method to perform any specialized
  // controller exit logic, and then call the super.exit_controller method
  public void exit_controller ()
  {
    m_stop = true;
    
    // close the view
    if (m_view != null) //m_view.closeView();
    {
      m_view.closeView ();
      // dispose must be called at this point so that the view's memory is
      // returned to the OS
      if (m_view instanceof Window) ((Window)m_view).dispose ();
      
      nullifyTheView();
      
      if ( m_parent != null ) 
      {
        // don't get in a loop between parent and child
        UdpRptrControllerBase ecb = m_parent;
        m_parent = null;
        ecb.cleanupChildController ( this );
      }
    }
    
    // if necessary, dispose anything that does not require a view
    dispose ();
  }

  // controllers that use the super.m_view = m_view = new EsmVw___()
  // technique should override this method and null out their
  // m_view and then call super.nullifyTheView() to clear the base
  // controller's m_view
  public void nullifyTheView ()
  {
    m_view = null;
  }

  // allows a controller to notify its parent that it is ready to go away
  public void cleanupChildController ( UdpRptrControllerBase child )
  {
    // this method should be overridden if needed
  }
          
  // force this controllers view to the top of the window stack
  public void toFront ()
  {
    if (m_view != null)
    {
      // let the world see what we have wrought
      m_view.toFront ();
    }
  }
  
  // derived class should override this method to dispose anything that
  // is not inside a view
  public void dispose ()
  { 
    m_parent = null;
  }
  
  // ActionListener interface implementation method
  @Override
  public void actionPerformed (ActionEvent e)
  {
    // note: this base class method is obviously incomplete,
    // and really just to give you some ideas about what you
    // bases you might need to cover in your derived class
    int eventnum = e.getID ();
    
    switch (eventnum)
    {
      case ActionEvent.ACTION_PERFORMED:
        String cmd = e.getActionCommand ();
        System.out.println ("EsmControllerBase.actionPerformed : action command received=" + cmd);
        break;
        
      default:
        //System.out.println ("EsmControllerBase.actionPerformed : received event ID=" + eventnum);
        break;
    }
    
  }
  
  
  // window event processing method
  public void processWindowEvent (WindowEvent e)
  {
    // note: this base class method is obviously incomplete,
    // and really just to give you some ideas about what you
    // bases you might need to cover in your derived class
    int eventnum = e.getID ();
    
    switch (eventnum)
    {
      case WindowEvent.WINDOW_CLOSING:
        exit_controller ();
        break;
        
      default:
        //System.out.println ("EsmControllerBase.processWindowEvent : received event ID=" + eventnum);
        break;
        
    }
    
  }
  
  
  /**MouseListener interface methods*/
  @Override
  public void mouseClicked (java.awt.event.MouseEvent mouseEvent)
  {
  }
  
  @Override
  public void mouseEntered (java.awt.event.MouseEvent mouseEvent)
  {
  }
  
  @Override
  public void mouseExited (java.awt.event.MouseEvent mouseEvent)
  {
  }
  
  @Override
  public void mousePressed (java.awt.event.MouseEvent mouseEvent)
  {
  }
  
  @Override
  public void mouseReleased (java.awt.event.MouseEvent mouseEvent)
  {
  }
  
}

