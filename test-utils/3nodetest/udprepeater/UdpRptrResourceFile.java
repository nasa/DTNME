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

import java.io.*;
import java.util.*;
import javax.swing.JCheckBox;
import javax.swing.JTextField;

public class UdpRptrResourceFile
{
  // singleton instance variable
  private static UdpRptrResourceFile m_instance;

  private static UdpRptrUiConstants m_uicon = UdpRptrUiConstants.Instance ();
  // actual file name of UdpRptr resource file
  private static String m_resource_filename;

  // field separator of key/value pairs in the resource file
  protected static final String m_field_separator = "=";

  // sorted list of resources as read from the resource file
  protected HashMap<String, String> m_resource_list = new HashMap<String, String>();

  // this object provides synchronized access to the resource list
  public final Object m_thread_lock = new Object ();


  /** singleton Instance method */
  static public UdpRptrResourceFile Instance ()
  {
    if (m_instance == null) m_instance = new UdpRptrResourceFile ();
    return m_instance;
  }


  /** constructor */
  protected UdpRptrResourceFile ()
  {
    m_uicon = UdpRptrUiConstants.Instance ();
    // actual file name of UdpRptr resource file
    m_resource_filename = System.getProperty ( "user.home" ) +
                          "/.UdpRptr_rc_" + m_uicon.m_host_name;


    File UdpRptrrc = new File ( m_resource_filename );

    // make sure we can access this file first
    if ( ! UdpRptrrc.canRead () )
    {
      // System.err.println ( "UdpRptrResourceFile.getResource : \"" + m_UdpRptrrc_filename + "\" is unreadable" );
      // System.err.flush();
      return;
    }

    try
    {
      FileInputStream fis = new FileInputStream ( UdpRptrrc );
      InputStreamReader isr = new InputStreamReader ( fis );
      BufferedReader br = new BufferedReader ( isr );

      while (true)
      {
        try
        {
          // get a line of input from the .UdpRptrrc file
          String line = br.readLine ();
          if ( null == line ) break;

          // add new element to sorted map
          UdpRptrStringTokenizer tokenizer = new UdpRptrStringTokenizer ( line, m_field_separator );

          String key = tokenizer.nextToken ();
          String value = tokenizer.nextToken ();

          if ( key.length () < 1 ) continue;
          if ( value.length () < 1 ) continue;

          synchronized ( m_thread_lock )
          {
            m_resource_list.put ( key, value );
          }
        }

        // if we caught some sort of i/o error
        // then break out of the loop
        catch ( Exception ex )
        {
          // System.err.println ( "UdpRptrResourceFile.getResource : \"" +
          //   m_UdpRptrrc_filename + "\" i/o error - " + ex.toString() );
          // System.err.flush();
          break;
        }
      }

      // do a little cleanup
      br.close ();
    }
    catch ( Exception ex )
    {
      System.err.println ( "UdpRptrResourceFile.getResource : \"" +
              m_resource_filename + "\" read exception - " + ex.toString () );
      System.err.flush ();
    }

  }

  public void putResource ( JTextField fld )
  {
    putResource ( fld.getName (), fld.getText () );
  }

  public void putResource ( JCheckBox cb )
  {
    putResource ( cb.getName (), cb.isSelected () ? "true" : "false" );
  }

  /** put a new/modified resource value */
  public void putResource ( String key, String value )
  {
    // save the new key=value pair in our sorted list
    synchronized ( m_thread_lock )
    {
      if ( value.equals ( m_resource_list.get ( key ) ) )
      {
        return; // key=value already exists
      }
      m_resource_list.put ( key, value );
    }

    try
    {
      // re-write the resource file
      File UdpRptrrc = new File ( m_resource_filename );
      UdpRptrrc.createNewFile ();

      FileOutputStream fos = new FileOutputStream ( UdpRptrrc );
      OutputStreamWriter osw = new OutputStreamWriter ( fos );
      BufferedWriter bw = new BufferedWriter ( osw );

      synchronized ( m_thread_lock )
      {
        String nextkey;
        String nextvalue;
        Set<String> keyset = m_resource_list.keySet ();
        Iterator<String> ix = keyset.iterator ();

        while ( ix.hasNext () )
        {
          nextkey = ix.next();
          nextvalue = m_resource_list.get(nextkey);
          bw.write ( nextkey + m_field_separator + nextvalue );
          bw.newLine ();
          bw.flush ();
        }
      }

      // do a little cleanup
      bw.close ();
    }
    catch ( Exception ex )
    {
      System.err.println ( "UdpRptrResourceFile.putResource : \"" +
              m_resource_filename + "\" write exception - " + ex.toString () );
      System.err.flush ();
    }

  }


  /** check to see if .UdpRptrrc file was readable */
  public boolean isAccessible ()
  {
    synchronized ( m_thread_lock )
    {
      return ( m_resource_list.size () > 0 );
    }
  }


  /** check to see if a specific resource is available */
  public boolean isResourceAvailable ( String key )
  {
    synchronized ( m_thread_lock )
    {
      return m_resource_list.containsKey (key);
    }
  }


  /** get the value of a specific resource */
  public String getResource ( String key )
  {
    String value = "";

    synchronized ( m_thread_lock )
    {
      if ( m_resource_list.containsKey (key) )
      {
        value = m_resource_list.get(key);
      }
    }

    return value;

  }

}

