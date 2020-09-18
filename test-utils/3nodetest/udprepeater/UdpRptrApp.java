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

public class UdpRptrApp
{
  
  // public accessible reference to command line arguments
  public static String[] argv = null;
  
  // public accessible reference to the app itself
  public static UdpRptrApp theApp = null;
  
  protected UdpRptrCoMainFrame m_controller = null;
  
  /**Construct the application*/
  public UdpRptrApp ()
  {
    m_controller = new UdpRptrCoMainFrame ();
  }
  
  
  /**get reference to main controller*/
  public UdpRptrCoMainFrame getTopLevelController ()
  {
    return m_controller;
  }
  
  /**Main method*/
  public static void main (String[] args)
  {
    // save easily accessible reference to program command line arguments
    argv = args;
    
//    try
//    {
//      UIManager.setLookAndFeel (new javax.swing.plaf.metal.MetalLookAndFeel ());
//      
//      // replaces the default EventQueue with Esm's
//      EsmKeyManager.Instance ();
//    }
//    catch(Exception e)
//    {
//      e.printStackTrace ();
//    }
    
    // create the Esm application
    theApp = new UdpRptrApp ();
  }
  
}

