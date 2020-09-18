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

public class UdpRptrDisplayRefreshTimerTask implements Runnable
{
  
  public UdpRptrDisplayRefreshTimerTask ()
  {
  }
  
  @Override
  public void run ()
  {
    // force the main view to be refreshed with
    // the latest values from the model
    if (UdpRptrApp.theApp != null)
    {
      UdpRptrCoMainFrame controller = UdpRptrApp.theApp.getTopLevelController ();

      if (controller != null)
      {
        controller.updateViews ();
      }
    }
  }
  
}

