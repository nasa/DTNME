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

import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.ScheduledFuture;
import java.util.concurrent.TimeUnit;


public class UdpRptrThreadPools
{
  private static UdpRptrThreadPools m_instance;
  private static final Object m_sync_lock = new Object ();

  private UdpRptrUiConstants m_uicon;

  private static final int SCHED_MAX_THREADS = 6;
  private ScheduledExecutorService m_sched_tasks;
  private ScheduledFuture<?> m_sf_displayRefresh;

  
  /** singleton Instance method */
  public static UdpRptrThreadPools Instance ()
  {
    if (m_instance == null)
    {
      synchronized (m_sync_lock)
      {
        if (m_instance == null)
        {
          m_instance = new UdpRptrThreadPools ();
        }
      }
    }

    return m_instance;
  }


  /** constructor */
  protected UdpRptrThreadPools ()
  {
    m_uicon = UdpRptrUiConstants.Instance();

    m_sched_tasks = Executors.newScheduledThreadPool(SCHED_MAX_THREADS);

  }

  /** schedule a task to run with a fixed interval between executions so that
      if there is a delay you do not get a bunch of executions all at once */
  public ScheduledFuture<?> scheduleWithFixedDelay( Runnable task, long initdelay, long delay )
  {
    return m_sched_tasks.scheduleWithFixedDelay(task, initdelay, delay, TimeUnit.MILLISECONDS);
  }

  /** schedule a task to run at a fixed interval */
  public ScheduledFuture<?> scheduleAtFixedRate( Runnable task, long initdelay, long delay )
  {
    return m_sched_tasks.scheduleAtFixedRate ( task, initdelay, delay, TimeUnit.MILLISECONDS );
  }

  /** schedule a task to run at a fixed interval */
  public ScheduledFuture<?> scheduleAtFixedRateMicroSecs( Runnable task, long initdelay, long delay )
  {
    return m_sched_tasks.scheduleAtFixedRate ( task, initdelay, delay, TimeUnit.MICROSECONDS );
  }

  public ScheduledFuture<?> schedule( Runnable task, long delay )
  {
    return m_sched_tasks.schedule( task, delay, TimeUnit.MILLISECONDS );
  }

  /** restart the display refresh timer */
  public void scheduleDisplayRefresh()
  {
    if ( null != m_sf_displayRefresh ) m_sf_displayRefresh.cancel(false);
    UdpRptrDisplayRefreshTimerTask refresher = new UdpRptrDisplayRefreshTimerTask ();

    // schedule a periodic display refresh
    long millis = m_uicon.GUI_REFRESH_TIME;
    m_sf_displayRefresh =
            m_sched_tasks.scheduleWithFixedDelay(refresher, millis, millis,
                                                 TimeUnit.MILLISECONDS);
  }

  public void shutdown()
  {
    m_sched_tasks.shutdown();
  }
  
}

