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
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import javax.swing.*;
import javax.swing.text.*;
import java.lang.reflect.Method;
import java.net.InetAddress;

public class UdpRptrUiConstants
{
  protected static UdpRptrUiConstants m_instance = null;

  public static String m_host_name;
  public static String m_ip_address;

  // creates a default Document to replace UdpRptrDocuments on JTextFields
  public static final DefaultEditorKit m_editorKit = new DefaultEditorKit();

  // replaces UdpRptrDocuments on JTextFields so that the UdpRptrDocuments will be
  // garbage collected
  public static final Document m_defaultDoc = 
    m_editorKit.createDefaultDocument();

  // replaces AbstractListModels on JLists so that the AbstractListModels will
  // be garbage collected
  public static final ListModel m_defaultListModel = new DefaultListModel();

  private final StringBuilder mb_sb_tkval = new StringBuilder(64);
  private final static StringBuilder m_sb_brksen = new StringBuilder(1024);

  // dimensions of the screen used by fitToScreen
  public static final Dimension m_screenDimActual =
        Toolkit.getDefaultToolkit().getScreenSize();
  public static Dimension m_screenDim;

  // timer constants (in millisecs)
  public static long GUI_HEARTBEAT_TIME = 30000;        // 30 seconds
  public static long GUI_REFRESH_TIME = 1000;           //  3
  public static long SERVER_ALIVE_CHECK_TIME = 10000;   // 10
  public static long LOGIN_RESPONSE_TIME = 15000;       // 15
  public static long SERVER_DATA_RESPONSE_TIME = 30000; // 30
  
  public static final int MISSION_NAME_LEN = 4;

  public static final int MISSION_ID_MAX = 255;

  // ip address constraints
  public static final int NUM_IP_ADDR_OCTETS = 4;
  public static final int IP_ADDR_OCTET_MAX = 255;
  public static final int MULTICAST_MIN = 224;
  public static final int MULTICAST_MAX = 239;

  // port range
  public static final int MIN_PORT = 1024;
  public static final int MAX_PORT = 65535;

  // VCID range
  public static final int MIN_VCID = 0;
  public static final int MAX_VCID = 63;
  
  // TODO : Make this configurable???
  // status ports
  public static int ADVERTISEMENT_PORT = 11400;
  public static int DAEMON_GUI_PORT = 11401;
  public static int SERVER_GUI_PORT = 11402;

  public static int GUI_RESPONSE_PORT = 11412;
  
  public static String ADVERTISEMENT_MULTICAST_ADDRESS = "225.1.1.1";

  public static boolean DISABLE_ENCRYPTION = false;
  public static boolean IS_TRAINING_SIMULATION = false;
  public static String TRAINING_SIMULATION_TITLE = "** TRAINING SIMULATION **";
  
  // delimiter used for command and status strings
  public static final String delimiter = "`";
  public static final String sc_delimiter = String.valueOf ( '\u001f' );

  // component colors used throughout UdpRptr
  public static final Color LIGHT_GRAY = new Color(238,238,238);
  public static final Color MEDIUM_YELLOW = new Color(225,225,0);
  public static final Color FOREST_GREEN = new Color(0,128,0);
  public static final Color BURGUNDY_RED = new Color(190,0,90);
  public static final Color BRIGHT_RED = new Color(255,0,0);
  public static final Color STEEL_BLUE = new Color(70,130,180);
  public static final Color BURNT_ORANGE = new Color(205,133,0);
  public static final Color PALE_YELLOW = new Color(255,255,200);
  public static final Color BRIGHT_YELLOW = new Color(255,255,0);
  public static final Color WHEAT = new Color(205,186,150);
  public static final Color MAGENTA = new Color(255,0,255);
  
  public static final Color bgcolor_Gray = LIGHT_GRAY;
  
  // background colors for the FEP Uplink and SLE Downlink forms
  public static final Color bgcolor_OFF = new Color ( 153, 153, 153 ); // dark gray
  public static final Color bgcolor_FEP = new Color ( 204, 255, 255 ); // pale blue
  public static final Color bgcolor_FEPHeading = new Color ( 135, 205, 241 ); // darker blue
  public static final Color bgcolor_Playback = new Color ( 230, 201, 249 ); // pale purple
  public static final Color bgcolor_PlaybackHeading = new Color ( 167, 116, 200 ); // 175, 87, 232 ); // darker purple
  public static final Color bgcolor_SLE = new Color ( 204, 255, 204 ); // pale green
  public static final Color bgcolor_SLEHeading = new Color ( 132, 195, 164 ); // darker green
  public static final Color bgcolor_SLEUplinkLoopback = new Color ( 159, 222, 159 );
  public static final Color bgcolor_SLEUplinkLoopbackHeading = bgcolor_SLEHeading;
  public static final Color bgcolor_Config = new Color ( 254, 254, 197 ); // pale yellow
  public static final Color bgcolor_ConfigHeading = new Color ( 186, 186, 89 ); // brownish yellow

  public static final Color bgcolor_EngineeringSupport = new Color ( 147,190,221 ); // light blue/gray
  public static final Color bgcolor_EngineeringSupportHeading = new Color ( 94,151,190 ); // darker blue/gray
  public static final Color bgcolor_EngineeringSupportMode = new Color ( 0, 0, 153 ); // navy blue
  

  public static final Color fgcolor_MsgWindow = Color.black;
  // public static final Color bgcolor_MsgWindow = new Color(238,213,183);  // pumpkin??
  public static final Color bgcolor_MsgWindow = new Color(255,204,102);  // orangier different from psm

  public static final Color fgcolor_aos = Color.white;
  public static final Color bgcolor_aos = FOREST_GREEN;
  public static final Color fgcolor_los = Color.white;
  public static final Color bgcolor_los = BURGUNDY_RED;

  public static final Color fgcolor_error = Color.white;
  public static final Color bgcolor_error = BRIGHT_RED;
  public static final Color bgcolor_input = Color.white;
  public static final Color fgcolor_input = Color.black;

  public static final Color fgcolor_noStatus = Color.black;
  public static final Color bgcolor_noStatus = BRIGHT_YELLOW;

  public static final Color fgcolor_search = Color.white;
  public static final Color bgcolor_search = BURGUNDY_RED;
  public static final Color fgcolor_lock = Color.white;
  public static final Color bgcolor_lock = FOREST_GREEN;

  protected static final int default_width = 80;

  static
  {
    // fudge the screen dimension to have some space around it
    m_screenDim = new Dimension ( m_screenDimActual.height * 9 / 10,
                                  m_screenDimActual.width * 9 / 10 );
  }

  

  /**singleton Instance method*/
  static public UdpRptrUiConstants Instance()
  {
    if (m_instance == null) m_instance = new UdpRptrUiConstants();
    return m_instance;
  }
  
  /**constructor*/
  private UdpRptrUiConstants()
  {
    m_host_name = "unknown";
    m_ip_address = "0.0.0.0";

    try
    {
      InetAddress myhost = InetAddress.getLocalHost ();
      m_host_name = myhost.getHostName ();
      m_ip_address = myhost.getHostAddress ();
    }
    catch (Exception ex)
    { }
  }
  
  /**trim a double string to the desired number of decimal places*/
  public static String trimDouble ( double d, int numDecimalPlaces )
  {
    String s = Double.toString(d);
    return trimDouble (s, numDecimalPlaces);
  }


  /**trim a double string to the desired number of decimal places*/
  public static String trimDouble ( String s, int numDecimalPlaces )
  {
    int ix = s.indexOf('.');
    if (ix == -1  ||  numDecimalPlaces < 1) return s;
    ix += 1 + numDecimalPlaces;
    String t = null;

    try { t = s.substring(0,ix); }
    catch (Exception ex) { t = s; }

    return t;
  }


  /**pad a string with blanks on the right*/
  public static String blankPadRight ( String s, int fieldWidth )
  {
    int n = s.length();
    String t = s;
    if (n >= fieldWidth) return t;

    for (int j=0; j < (fieldWidth - n); ++j) t += " ";
    return t;
  }


  /**pad a string with blanks on the left*/
  public static String blankPadLeft ( String s, int fieldWidth )
  {
    int n = s.length();
    if (n >= fieldWidth) return s;

    String blanks = "";
    for (int j=0; j < (fieldWidth - n); ++j) blanks += " ";
    return blanks + s;
  }


  /** a simple method for bringing up a dialog box*/
  public static void popup_dialog (JDialog dlg, JFrame view, boolean ismodal)
  {
    Dimension dlgSize = dlg.getPreferredSize();
    Dimension frmSize = view.getSize();
    Point loc = view.getLocation();
    dlg.setLocation((frmSize.width - dlgSize.width) / 2 + loc.x, 
                    (frmSize.height - dlgSize.height) / 2 + loc.y);
    dlg.setModal(ismodal);
    dlg.setVisible(true);
  }

  /**interpret string of such values as true/false, yes/no, 0/1, as a boolean*/
  public static boolean parseBoolean ( String bool )
  {
    // default return value is false unless otherwise specified
    return parseBoolean ( bool, false );
  }

  public static boolean parseBoolean ( String bool, boolean theDefault )
  {
    if ( bool != null )
    {
      if (bool.equalsIgnoreCase("true"))
        return true;
      else if (bool.equalsIgnoreCase("false"))
        return false;
      else if (bool.equalsIgnoreCase("t"))
        return true;
      else if (bool.equalsIgnoreCase("f"))
        return false;
      else if (bool.equalsIgnoreCase("on"))
        return true;
      else if (bool.equalsIgnoreCase("off"))
        return false;
      else if (bool.equalsIgnoreCase("yes"))
        return true;
      else if (bool.equalsIgnoreCase("no"))
        return false;
      else if (bool.equalsIgnoreCase("y"))
        return true;
      else if (bool.equalsIgnoreCase("n"))
        return false;
      else if (bool.equalsIgnoreCase("1"))
        return true;
      else if (bool.equalsIgnoreCase("0"))
        return false;
      else if (bool.equalsIgnoreCase("aos"))
        return true;
      else if (bool.equalsIgnoreCase("los"))
        return false;
      else if (bool.equalsIgnoreCase("lock"))
        return true;
      else if (bool.equalsIgnoreCase("search"))
        return false;
      else if (bool.equalsIgnoreCase("prime"))
        return true;
      else if (bool.equalsIgnoreCase("enabled"))
        return true;
      else if (bool.equalsIgnoreCase("enabled/bound"))
        return true;
      else if (bool.equalsIgnoreCase("disabled"))
        return false;
      else if (bool.equalsIgnoreCase("bound"))
        return true;
      else if (bool.equalsIgnoreCase("unbound"))
        return false;
    }

    return theDefault;
  }


  /** this method takes a sentence and breaks it up by inserting
   *  new line characters in the appropriate places.
   */
  public static String breakSentence ( String s, int width )
  {
    synchronized ( m_sb_brksen )
    {
      m_sb_brksen.setLength(0);
      m_sb_brksen.append(s);
      if (width < 1) width = default_width;

      for ( int ix=0; ix < m_sb_brksen.length(); ++ix )
      {
        int last_blank = -1, linelen = 0, start_line = ix;

        for ( ;
              ( ix < m_sb_brksen.length() ) && ( linelen < width );
              ++linelen, ++ix )
        {
          // keep track of the location of the last blank we found
          if (m_sb_brksen.charAt(ix) == ' ') last_blank = ix;

          // start over again with this line if we find a new line character
          if (m_sb_brksen.charAt(ix) == '\n')
          {
            linelen = 0;
            last_blank = -1;
            start_line = ix + 1;
          }
        }

        // if we reached the end of the line with no where
        // to break, then we are, by definition, finished
        if ( last_blank == -1 ) last_blank = ix;

        // point to the next character after the blank
        ix = last_blank + 1;
        if ( ix >= m_sb_brksen.length() ) break;

        // look for the next non-blank character
        while (ix < m_sb_brksen.length() && m_sb_brksen.charAt(ix) == ' ') ++ix;

        // check again to make sure we're not at the end
        linelen = ix - start_line;
        int remaining = m_sb_brksen.length() - ix;
        if ( (linelen + remaining) < width ) break;

        // insert the new line character just before the next
        // non-blank character.  this will prevent blanks
        // from appearing at the beginning of the next line,
        // which just looks wierd.
        m_sb_brksen.insert (ix, "\n");

        // if there isn't enough left to bother with then quit
        if (remaining <= width) break;

      }

      return m_sb_brksen.toString();
    }
  }

  /** these methods generate a JOptionPane type of box with wrapped text,
   *  which is useful when you want to display a message, but the message
   *  string is just too darn long to fit nicely on one line.
   */

  public static void popup_msgbox ( Component parent, int width, String msg )
  {
    popup_msgbox ( parent, width, msg, "", JOptionPane.PLAIN_MESSAGE );
  }

  public static void popup_msgbox ( Component parent, int width, String msg, 
          String title, int msgtype )
  {
    String wrapped_msg = breakSentence ( msg, width );

    try
    {
      JOptionPane.showMessageDialog ( parent, wrapped_msg, title, msgtype );
    } catch (Exception ex) { }
  }

  public static void popup_msgbox ( int width, String msg )
  {
    popup_msgbox ( null, width, msg, "", JOptionPane.PLAIN_MESSAGE );
  }


  // popup a confirm box with specified values and default value
  public static int popup_confirmbox ( 
    Component parent, int width, String msg, String title, int opttype )
  {
    String wrapped_msg = breakSentence ( msg, width );

    try
    {
      return JOptionPane.showConfirmDialog (parent, wrapped_msg, title, opttype);
    }
    catch (Exception ex)
    {
      if (opttype == JOptionPane.YES_NO_CANCEL_OPTION)
        return JOptionPane.CANCEL_OPTION;
      else
        return JOptionPane.NO_OPTION;
    }
  }

  public static int popup_confirmbox ( 
    Component parent, int width, String msg, String title, int opttype, 
          Object[] options, Object initialValue )
  {
    String wrapped_msg = breakSentence ( msg, width );

    try
    {
      return JOptionPane.showOptionDialog (parent, wrapped_msg, title, opttype,
              JOptionPane.QUESTION_MESSAGE, null, options, initialValue );
    }
    catch (Exception ex)
    {
      if (opttype == JOptionPane.YES_NO_CANCEL_OPTION)
        return JOptionPane.CANCEL_OPTION;
      else
        return JOptionPane.NO_OPTION;
    }
  }

/** these methods generate a non-modal message box using JDialog with wrapped text,
   *  which is useful when you want to display a message, but the message
   *  string is just too darn long to fit nicely on one line.
   */
  public static void popup_nonmodal_msgbox ( Frame parent, int width, String msg )
  {
    popup_nonmodal_msgbox ( parent, width, msg, "", JOptionPane.PLAIN_MESSAGE );
  }

  public static void popup_nonmodal_msgbox ( Frame parent, int width, String msg, 
          String title, int msgtype )
  {
    String wrapped_msg = breakSentence ( msg, width );

    try
    {
      // this code based on DialogDemoProject by Sun Microsystems, Inc. 
      final JDialog dialog = new JDialog (parent, "" );
      
      JLabel label = new JLabel(wrapped_msg);
      label.setHorizontalAlignment ( JLabel.CENTER );
      
      JButton okButton = new JButton("OK");
      okButton.addActionListener ( 
              new ActionListener ()
              {
                @Override
                public void actionPerformed (ActionEvent e)
                {
                  dialog.setVisible ( false );
                  dialog.dispose ();
                }
              } 
      );
      JPanel buttonPanel = new JPanel ();
      buttonPanel.setLayout ( new FlowLayout(FlowLayout.CENTER, 4, 4) );
      buttonPanel.add ( okButton );
      buttonPanel.setBorder ( BorderFactory.createEmptyBorder ( 0, 0, 5,5 ) );
      
      JPanel contentPane = new JPanel ( new BorderLayout() );
      contentPane.add ( label, BorderLayout.CENTER );
      contentPane.add ( buttonPanel, BorderLayout.SOUTH );
      contentPane.setOpaque ( true );
      dialog.setContentPane ( contentPane );
      
      // Show it
      dialog.pack ();
      Dimension dim = new Dimension ( label.getPreferredSize () );
      dim.height += 10;
      dim.width += 20;
      label.setMinimumSize ( dim );
      label.setMaximumSize ( dim );
      label.setPreferredSize ( dim );
      
      dim = new Dimension ( buttonPanel.getPreferredSize () );
      dim.height += 10;
      dim.width += 20;
      buttonPanel.setMinimumSize ( dim );
      buttonPanel.setMaximumSize ( dim );
      buttonPanel.setPreferredSize ( dim );
      
      dialog.pack ();
      dim = new Dimension ( dialog.getPreferredSize () );
      dim.height += 30;
      dim.width += 20;
      dialog.setSize ( dim );
      dialog.setLocationRelativeTo ( parent );
      dialog.setVisible ( true );
    } catch (Exception ex) { }
  }

  /** sizes the indicated frame with the indicated size so that it will fit
   *  on the screen
   */
  public static void fitToScreen(JFrame frame, Dimension frameDim)
  {
    // make sure the frame fits on the screen
    if (frameDim.width+10 >= m_screenDim.width) 
    {
      if (frameDim.height+10 < m_screenDim.height) 
        frame.setSize(m_screenDim.width, frameDim.height+10);
      else
        frame.setSize(m_screenDim.width, m_screenDim.height);
    }
    else if (frameDim.height+10 >= m_screenDim.height) 
    {
      frame.setSize(frameDim.width+10, m_screenDim.height);
    }
    else
    {
      // fudge the dimensions to account for the frame's border
      frame.setSize(frameDim.width+10, frameDim.height+10);
    }
  }

  // Expects a string of a rate in kilobits per second and then converts it to
  // megabits per second if appropriate and then appends the corresponding
  // units (either "Kb/s" or "Mb/s")
  public String fmtDataRate ( long rate )
  {
    String result;
    try
    {
      double val = rate;
      String uom = "Mb/s";

      if ( val < 400.0 ) val = val * 1000000;

      if ( val < 1000000.0 )
      {
        val = val / 1000.0;
        uom = "Kb/s";
      }
      else
      {
        val = val / 1000000.0;
      }

      if ( val == 0.0 )
      {
        result = "0.0 Mb/s";
      }
      else if ( val >= 1000.0 )
      {
        result = String.format ( "%.0f %s", val, uom );
      }
      else if ( val >= 100.0 )
      {
        result = String.format ( "%.1f %s", val, uom );
      }
      else if ( val >= 10.0 )
      {
        result = String.format ( "%.2f %s", val, uom );
      }
      else
      {
        result = String.format ( "%.3f %s", val, uom );
      }
    }
    catch ( Exception ex)
    {
      result = String.valueOf ( rate ) + " b/s";
    }

    return result;
  }

  private class SwingTask implements Runnable
  {
    Object m_object;
    Method m_method;
    Object[] m_params;

    public SwingTask ( Object obj, Method mth )
    {
      m_object = obj;
      m_method = mth;
      m_params = null;
    }

    public SwingTask ( Object obj, Method mth, Object[] params )
    {
      m_object = obj;
      m_method = mth;
      m_params = params;
    }

    @Override
    public void run()
    {
      try
      {
        m_method.invoke ( m_object, m_params );
      }
      catch ( Exception ex )
      {
        String msg = " SwingTask - Error invoking method: " +
                m_method.getName () + " on class: " + m_object.toString () +
                "\nError: " + ex.getMessage ();
        System.out.println ( msg );
        popup_msgbox ( 0, msg);
      }
    }
  }

  public void swingInvokeLater ( Object obj, String methodname )
  {
    try
    {
      Class<?> cls = obj.getClass ();
      Method mth = cls.getMethod ( methodname, (Class[]) null );
      SwingUtilities.invokeLater ( new SwingTask ( obj, mth ) );
    }
    catch ( Exception ex )
    {
      String msg = "swingInvokeLater - error accessing method: " +
              methodname + " on class: " + obj.getClass ().getSimpleName () +
              "\nError: " + ex.getMessage ();
      System.out.println( msg );
      popup_msgbox ( 0, msg);
    }
  }

  public void swingInvokeLater ( Object obj, String methodname, String var1 )
  {
    try
    {
      Class<?> cls = obj.getClass ();
      Class partypes[] = new Class[1];
      partypes[0] = String.class;
      Method mth = cls.getMethod ( methodname, partypes );

      Object vars[] = new Object[1];
      vars[0] = var1;
      SwingUtilities.invokeLater ( new SwingTask ( obj, mth, vars ) );
    }
    catch ( Exception ex )
    {
      String msg = "swingInvokeLater - error accessing method: " +
              methodname + " on class: " + obj.getClass ().getSimpleName () +
              "\nError: " + ex.getMessage ();
      System.out.println( msg );
      popup_msgbox ( 0, msg);
    }
  }


}

