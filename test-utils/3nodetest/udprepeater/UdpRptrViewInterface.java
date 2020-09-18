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

public interface UdpRptrViewInterface
{
  // change the enabled and visiblilty properties
  void showView();
  void hideView();
  void toFront ();
  
  // closing the view permenantly so clean up to improve garbage collection
  void closeView ();

  // change/check the m_enabled property
  void enableView ();
  void disableView ();
  boolean isViewEnabled ();

  // time to update all the fields that are displayed
  void updateView ();

  // register the listener for the form
  void registerListener (UdpRptrControllerBase listener);
  void unregisterListener ();
}

