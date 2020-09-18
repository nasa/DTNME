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

import java.util.regex.*;

public class UdpRptrStringTokenizer
{
  private static Pattern m_escaper = Pattern.compile ( "([^a-zA-Z0-9])" );

  private StringBuilder m_sb = new StringBuilder();

  protected boolean m_count_nulls = false;

  private String[] m_tokens = null;

  private String m_str = null;
  private String m_delim = "";
  private String m_delim_regx = "";

  private int m_next_token = 0;
  private int m_num_tokens = 0;
  private int m_num_remaining = 0;


  /** constructors */
  public UdpRptrStringTokenizer ()
  {
    m_delim = " \t\n\r\f";  // default delimiters (white space)
  }


  public UdpRptrStringTokenizer ( String str )
  {
    m_str = str;
    setDelim ( " \t\n\r\f" );  // default delimiters (white space)
  }

  public UdpRptrStringTokenizer ( String str, String delim )
  {
    m_str = str;
    setDelim ( delim );
  }

  /** we're NOT gonna implement this one (see above)
   * public EsmStringTokenizer ( String str, String delim, boolean returnDelims )
   * {
   * }
   */


  /** standard java StringTokenizer method */
  public int countTokens ()
  {
    // initialize tokenizer if necessary
    if ( m_tokens == null )
    {
      initialize_tokenizer ();
      if ( m_tokens == null )
      {
        return 0;
      }
    }

    // return the token count
    return m_num_tokens;

  }


  /** standard java StringTokenizer method */
  public boolean hasMoreElements ()
  {
    return hasMoreTokens ();
  }


  /** standard java StringTokenizer method */
  public boolean hasMoreTokens ()
  {
    // initialize tokenizer if necessary
    if ( m_tokens == null )
    {
      initialize_tokenizer ();
      if ( m_tokens == null )
      {
        return false;
      }
    }

    // are there any more tokens available?
    return ( m_num_remaining > 0 );

  }


  /** standard java StringTokenizer method */
  public Object nextElement ()
  {
    return (Object)nextToken ();
  }


  /** standard java StringTokenizer method */
  public String nextToken ()
  {
    // initialize tokenizer if necessary
    if ( m_tokens == null )
    {
      initialize_tokenizer ();
      if ( m_tokens == null )
      {
        return null;
      }
    }

    // grab the next token off the list and return it
    if ( m_num_remaining < 1 )
    {
      return null;
    }

    // get the index to the next token
    int ix = m_next_token++;

    // if we're not returning null fields (the default) then skip over nulls
    if ( ! m_count_nulls )
    {
      while ( m_tokens[ix].length () < 1 )
      {
        ix = m_next_token++;
      }
    }

    --m_num_remaining;

    try
    {
      String result = m_tokens[ix];
    }
    catch ( Exception ex )
    {
      System.out.println("error");
    }

    return m_tokens[ix];

  }


  /** not implemented (see above)
   * public String nextToken ( String delim )
   * {
   * }
   */


  /** new methods to allow user to re-initialize this object */
  public void setString ( String str )
  {
    m_str = str;
    initialize_tokenizer ();
  }

  public void setString ( String str, String delim )
  {
    m_str = str;
    setDelim ( delim );
  }

  public final void setDelim ( String delim )
  {
    if ( ! m_delim.equals(delim) )
    {
      m_delim = delim;
      m_sb.setLength(0);
      m_sb.append('[');
      m_sb.append(escapeRE ( m_delim ));
      m_sb.append(']');
      m_delim_regx = m_sb.toString();
    }

    if ( null != m_str )
    {
      initialize_tokenizer ();
    }
  }


  /** fix string to escape regular expression characters properly */
  public String escapeRE ( String str )
  {
    return m_escaper.matcher (str).replaceAll ( "\\\\$1" );
  }


  /** new internal method to initialize the tokenizer */
  void initialize_tokenizer ()
  {
    // make sure we have something to work with
    if ( m_str == null  ||  m_delim == null )
    {
      m_tokens = null;
      return;
    }

    // parse the string
    m_tokens = m_str.split ( m_delim_regx, -1 );

    // count the tokens
    m_next_token = 0;
    m_num_tokens = 0;
    m_num_remaining = 0;

    // if we're counting null tokens it's easy
    if ( m_count_nulls )
    {
      m_num_tokens = m_tokens.length;

      // we don't want to count an extraneous trailing
      // delimiter as a null token, so if the very last
      // character of our input string is a delimiter,
      // subtract one from the number of tokens.
      //
      // for example, the string "1,2,,4,5,,," should
      // result in 7 tokens, not 8 as split returns
      //
      if ( m_num_tokens > 0  &&  m_tokens[m_num_tokens-1].length () < 1 )
      {
        try
        {
          int len = m_str.length ();
          char c = m_str.charAt (len-1);

          if ( m_delim.indexOf (c) >= 0 )
          {
            --m_num_tokens;
          }
        }
        catch ( Exception ex )
        { }
      }

      m_num_remaining = m_num_tokens;
    }

    // okey, not counting null tokens so it's a little bit trickier
    else
    {
      int len = m_tokens.length;

      for ( int j=0; j < len; ++j )
      {
        if ( m_tokens[j].length () > 0 )
        {
          ++m_num_tokens;
        }
      }

      m_num_remaining = m_num_tokens;
    }

  }

}

