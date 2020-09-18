/*
 *    Copyright 2005-2006 Intel Corporation
 * 
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 * 
 *        http://www.apache.org/licenses/LICENSE-2.0
 * 
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<assert.h>

#include "LinkScheduleEstimator.h"

namespace dtn {

// dist holds a temporary distance array for the dynamic programming algorithm
unsigned int **dist;

LinkScheduleEstimator::LinkScheduleEstimator() 
    : Logger("LinkScheduleEstimator", "/dtn/route/linkstate/estimator")
{}  

/*
 *  compute the distance between two log entries.
 *  a_offset, b_offset is used for comparing subsequences
 *  at different places in the log.
 *
 *  Distances larger than the window size are given the cost of the
 *  minimum duration smaller distances just use the actual distance.
 */
unsigned int 
LinkScheduleEstimator::entry_dist(Log &a, unsigned int a_index, unsigned int a_offset,
                                  Log &b, unsigned int b_index, unsigned int b_offset,
                                  unsigned int warping_window)
{
    unsigned int diff=absdiff(a[a_index].start-a_offset,b[b_index].start-b_offset);
    unsigned int minduration=oasys::min(a[a_index].duration, b[b_index].duration);
    if(diff > warping_window)
        return minduration;
    else return oasys::min(diff, minduration);
}

/*
 *  Recursive internal function of log_dist.
 */
unsigned int 
LinkScheduleEstimator::log_dist_r(Log &a, unsigned int a_index, unsigned int a_offset,
                                  Log &b, unsigned int b_index, unsigned int b_offset,
                                  unsigned int warping_window)
{
    unsigned int mindist;

    // can't compute values outside the boundaries.
    //XXX/jakob - why did we get here anyway?
    // XXX/demmer this is a bogus compariosn
    //if(a_index<0 || b_index<0) return MAX_DIST;

    // if we've already computed the dist, return it
    if(dist[a_index][b_index]<MAX_DIST)
        return dist[a_index][b_index];

    // no use going outside boundaries
    else if(a_index==0)
        mindist=log_dist_r(a,a_index,a_offset,
                           b,b_index-1,b_offset,
                           warping_window);
    // no use going outside boundaries
    else if(b_index==0)
        mindist=log_dist_r(a,a_index-1,a_offset,
                           b,b_index,b_offset,
                           warping_window);
    // find the minimum cost neighboring cell
    else {
        mindist = oasys::min(oasys::min(log_dist_r(a,a_index-1,a_offset,
                                                   b,b_index-1,b_offset,warping_window),
                                        log_dist_r(a,a_index-1,a_offset,
                                                   b,b_index,b_offset,warping_window)),
                             log_dist_r(a,a_index,a_offset,
                                        b,b_index-1,b_offset,warping_window));
    }

    unsigned int entrydist=entry_dist(a,a_index,a_offset,
                             b,b_index,b_offset,
                             warping_window);

    dist[a_index][b_index]=entrydist+mindist;

    return dist[a_index][b_index];
}

/*
 *  determines the distance between logs a and b using dynamic programming.
 *
 */
unsigned int 
LinkScheduleEstimator::log_dist(Log &a, unsigned int a_offset,
                                Log &b, unsigned int b_offset,
                                unsigned int warping_window, int print_table)
{
    /*
     * Initialize a cost matrix for the DP algorithm
     */
    dist=(unsigned int**)malloc(a.size()*sizeof(unsigned int));
    for(unsigned int i=0;i<a.size();i++)
    {
        dist[i]=(unsigned int*)malloc(b.size()*sizeof(unsigned int));

        for(unsigned int j=0;j<b.size();j++)
            dist[i][j]=MAX_DIST;
    }

    dist[0][0]=entry_dist(a,0,a_offset,b,0,b_offset,warping_window);

    /*
     * Call inner function for progressively larger parts of the array.
     * Goes from 0,0 to n,n along the diagonal.
     */
    unsigned int d;
    for(unsigned int i=0;i<oasys::min(a.size(),b.size());i++)
        d=log_dist_r(a,i,a_offset,b,i,b_offset,warping_window);

    // compute the actual distance with a final call
    d=log_dist_r(a,a.size()-1,a_offset,
                 b,b.size()-1,b_offset,
                 warping_window);

    if(print_table)
    {
        for(unsigned int i=0;i<a.size();i++)
        {
            log_debug("%d\t| ",a[a.size()-i-1].start-a_offset);
            for(unsigned int j=0;j<b.size();j++)
            {
                log_debug("%d\t",dist[a.size()-i-1][j]);
            }
            log_debug("\n");
        }
        log_debug("---------------------------------------------------\n\t ");
        for(unsigned int i=0;i<b.size();i++)
            log_debug("%d\t| ",b[i].start-b_offset);
        log_debug("\n\t ");
        for(unsigned int i=0;i<b.size();i++)
            log_debug("%d\t| ",b[i].duration);
        log_debug("\n\n");
    }

    free(dist);

    return d;
}

/*
 * Computes the distance between l and l shifted phase time
 * steps. This is actually the inverse autocorrelation function...
 */

unsigned int 
LinkScheduleEstimator::autocorrelation(Log &log, unsigned int phase, int print_table)
{
    Log clone(log.size());
    for(unsigned int i=phase;i<log.size();i++) {
        clone[i-phase].start=log[i].start-log[phase].start;
        clone[i-phase].duration=log[i].duration;
    }

    for(unsigned int i=0;i<phase;i++) {
        clone[i+(log.size()-phase)].start=
            log[i].start+log[log.size()-1].start-log[phase-1].start;
        clone[i+(log.size()-phase)].duration=log[i].duration;
    }

    unsigned int d = log_dist(log, log[0].start, // a_offset
                     clone, clone[0].start,
                     (int)((log[log.size()].start+
                            log[log.size()].duration)*WARPING_WINDOW),
                              print_table);


    return d;
}

void 
LinkScheduleEstimator::print_log(Log &log, int relative_dates)
{
    unsigned int offset=0;
    (void) offset;  // prevent unused message if debug logging disabled

    if(relative_dates)
        offset=log[0].start;

    for(unsigned int i=0;i<log.size();i++) {
        log_debug("(%d, %d)",log[i].start-offset,log[i].duration);
        if(i<log.size()-1)
            log_debug(", ");
    }
    log_debug("\n");
}


/*
 *  Generates a randomized periodic log given a schedule and a jitter level.
 *
 */

LinkScheduleEstimator::Log* LinkScheduleEstimator::generate_samples(Log &schedule,
                                        unsigned int log_size,
                                        unsigned int start_jitter,
                                        double duration_jitter)
{
    Log *output = new Log(log_size);
    unsigned int schedule_index = 0;
    unsigned int start_time_offset = 0;
    for(unsigned int i=0;i<log_size;i++)
    {
        /*
         * The last schedule entry is assumed to be identical to the
         * first entry
         */

        if(schedule_index == schedule.size() - 1) {
            start_time_offset = start_time_offset +
                                schedule[schedule.size()-1].start +
                                schedule[schedule.size()-1].duration;
            schedule_index = 0;
        }

        (*output)[i].start = (unsigned int) oasys::max((unsigned int)0,(start_time_offset +
                                                          schedule[schedule_index].start -
                                                          start_jitter / 2 +
                                                          (unsigned int)(random() % start_jitter)));

        unsigned int duration_jitter_abs = (int)(duration_jitter*
                                        schedule[schedule_index].duration);

        (*output)[i].duration=schedule[schedule_index].duration -
                              duration_jitter_abs / 2 +
                              random() % duration_jitter_abs;
        schedule_index++;
    }

    return output;
}


/*
 * Calculate the (inverse) autocorrelation function for the log, then
 * look for the deepest valley.  If there second smallest value is
 * 2*period away, we have found our period.
 *
 * This is a pretty simple heuristic, should be possible to do better.
 */
unsigned int 
LinkScheduleEstimator::estimate_period(Log &log)
{
    int* autoc=(int*)malloc(log.size()*sizeof(int));
    for(unsigned int i=1;i<log.size();i++) {
        autoc[i]=autocorrelation(log,i,0);
    }

    // first find the best autocorrelation period
    unsigned int candidate=1;
    for(unsigned int i=1;i<log.size();i++)
        if(autoc[i]<autoc[candidate])
            candidate=i;

    unsigned int candidate2=1;
    for(unsigned int i=1;i<log.size();i++)
        if(i!=candidate && autoc[i]<autoc[candidate2])
            candidate2=i;


    double should_be_2=log[candidate2].start/(double)log[candidate].start;

    if(absdiff(should_be_2,2)<2*PERIOD_TOLERANCE)
        return (int)(log[candidate2].start+log[candidate].start)/3;
    else
        return 0;
}

/**
 * return the index of the closest log entry at or before the given date.
 */
unsigned int 
LinkScheduleEstimator::seek_to_before_date(Log &log, unsigned int date)
{
    for( unsigned int i=0 ; i < log.size() ; i++ )
        if( log[i].start > date )
            return (unsigned int)oasys::max(0,(int)i-1);
    return 0;
}

/**
 * return the index of the closest log entry to the given date
 */
unsigned int 
LinkScheduleEstimator::closest_entry_to_date(Log &log, unsigned int date)
{
    unsigned int i=seek_to_before_date(log, date);

    if(absdiff(log[i].start,date)<absdiff(log[i+1].start,date))
        return i;
    else
        return i+1;

    return 0;
}


LinkScheduleEstimator::Log* 
LinkScheduleEstimator::clone_subsequence(Log &log, unsigned int start, unsigned int len)
{
    Log *out = new Log(len);
    for(unsigned int i=0;i<len;i++) {
        (*out)[i].start=log[i+start].start;
        (*out)[i].duration=log[i+start].duration;
    }

    return out;
}

/*
 * Match the pattern against the log, one pattern_len at a time.
 * returns the sum of the distances.
 */
unsigned int 
LinkScheduleEstimator::badness_of_match(Log &pattern,
                                        Log &log,
                                        unsigned int warping_window, 
                                        unsigned int period)
{
    unsigned int badness=0;
    for(unsigned int date=0; date<log[log.size()-pattern.size()].start; date+=period)
    {
        unsigned int start = closest_entry_to_date(log,date);
        Log* subsequence = clone_subsequence(log, start,
                                             closest_entry_to_date(log,date) -
                                             start+1);

        unsigned int change=log_dist(pattern,
                            pattern[0].start,
                            *subsequence,
                            date,
                            warping_window,
                            0);

        delete subsequence;
        badness+=change;
    }
    return badness;
}


/*
 *  Given a period estimate, determine the schedule that best fits the
 *  provided log.
 */
LinkScheduleEstimator::Log* 
LinkScheduleEstimator::extract_schedule(Log &log, unsigned int period_estimate)
{
    Log* best_pattern=0;
    unsigned int best_pattern_badness=100000000;

    /*
       First find a canonical schedule approximation - the period that
       best matches all the other periods in this log. Once we have
       that, refine the period length and the schedule using all the
       other periods that match exactly.
    */

    // find the period that best matches the rest of the log
    for(unsigned int date=0;
        date<=(log[log.size()-1].start-period_estimate);
        date+=period_estimate)
    {
        unsigned int pattern_index=closest_entry_to_date(log,date);
        unsigned int pattern_len = closest_entry_to_date(log,date+period_estimate) -
                          pattern_index+1;
        Log* pattern = clone_subsequence(log, pattern_index, pattern_len);

        unsigned int badness=badness_of_match(*pattern,
                                     log,
                                     (int)(period_estimate*WARPING_WINDOW),
                                     period_estimate);

        if(badness < best_pattern_badness)
        {
            if(best_pattern)
                delete best_pattern;

            best_pattern = pattern;
            best_pattern_badness = badness;
        }
        else delete pattern;
    }

    log_debug("And the best pattern is (%d): \n",best_pattern_badness);
    print_log(*best_pattern, 1);

    return new Log(*best_pattern);

/*
      // This part computes an average schedule based on all the
      // subsequences matching the pattern.  apparently, it isn't
      // working as well as the "best pattern" thing is right now, so
      // it's commented until we can improve it.

    unsigned int dist;
    unsigned int count=0;
    for ( unsigned int date=0 ;
          date<=(log[log.size()-1].start-period_estimate);
          date+=period_estimate )
    {
        unsigned int pattern_index=closest_entry_to_date(l,log.size(),date);
        unsigned int pattern_len =
            closest_entry_to_date(l,log.size(),date+period_estimate) -
            pattern_index;

        if((pattern_len == best_pattern_len) &&
           !(dist=log_dist(best_pattern,,best_pattern_len,best_pattern[0].start,
                           &(log[pattern_index]), pattern_len, date,
                           (int)(period_estimate*WARPING_WINDOW),
                           0)))
        {
            log_debug("log at %d is good\n", date);
            count++;

            for(unsigned int i=0;i<pattern_len;i++) {
                schedule[i].start+=log[pattern_index+i].start-date;
                schedule[i].duration+=log[pattern_index+i].duration;
            }
        }
        else
            log_debug("tried date %d (%d-%d): %d\n",
                   date, pattern_index,pattern_len, dist);

    }

    for(unsigned int i=0;i<best_pattern_len;i++) {
        schedule[i].start/=count;
        schedule[i].duration/=count;
    }

    log_debug("Extracted schedule is\n");
    print_log(schedule,best_pattern_len,1);
*/
}

/*
 *  Given a period estimate, improve the estimate by fitting it
 *  against the entire log data.
 *
 *  XXX/jakob - if the last scheduled event doesn't occur in any given
 *  period, the refined period will be way off!  should check if the
 *  data makes sense before using it.
 */

unsigned int 
LinkScheduleEstimator::refine_period(LinkScheduleEstimator::Log &log, 
                                     unsigned int period_estimate)
{
    unsigned int sum=0;
    int count=-1;
    int count2=0;


    for(unsigned int date=0 ; 
        date<=(log[log.size()-1].start-period_estimate) ; 
        date+=period_estimate)
    {
        unsigned int pattern_index=closest_entry_to_date(log,date);
        sum+=log[pattern_index].start;
        count++;
        count2+=count;
    }

    return sum/count2;
}

/**
 *  This is the function to be called from the outside.
 **/
LinkScheduleEstimator::Log* 
LinkScheduleEstimator::find_schedule(LinkScheduleEstimator::Log &log)
{
    // find a first estimate of the period. If there is a period, this
    // will return a non-zero value.

    unsigned int period = estimate_period(log);

    if(period) {
        // now try to fit this period to the full log as closely as possible
        period = refine_period(log, period);
        // and then compute the best schedule for the given log and period
        return extract_schedule(log, period);
    }
    else return 0;
}


/*
int main(int argc, char** argv)
{
    Log f2(5);
    f2[0].start=0;
    f2[0].duration=1000;

    f2[1].start=4000;
    f2[1].duration=500;

    f2[2].start=9000;
    f2[2].duration=1000;

    f2[3].start=14000;
    f2[3].duration=2000;

    f2[4].start=19000;
    f2[4].duration=1000;

//    log f1[]={CONTACT(0,1023),
//              CONTACT(5006,1040),
//             CONTACT(10150,1002),
//              CONTACT(20040, 956),
//              CONTACT(25200, 896),
//              CONTACT(26150, 350),
//              CONTACT(29682, 1098),
//              CONTACT(39780, 1035),
//              CONTACT(45000, 987),
//              CONTACT(50045, 907),
//              CONTACT(60340, 1055),
//              CONTACT(65000, 987),
//              CONTACT(70045, 907),
//              CONTACT(80340, 1055)};


    Log* best_schedule=0;
    int  best_schedule_len;
    Log* schedule;
    int schedule_len;

    int log_len=100;
    Log *full_log=generate_samples(f2,log_len,1000,.1);

    int best_period=0;
    int best_cost=1000000;

    for(int current_event=1;current_event<log_len;current_event++)
    {
        Log *log=clone_subsequence(*full_log, 0, current_event+1);
        int period=estimate_period((*log));
        if(period)
        {
            log_debug("At time %d (%d), estimated period is %d.\n",
                   current_event,
                   (*log)[current_event-1].start,
                   period);


            period = refine_period((*log), period);
            log_debug("Period after refinement is %d\n",period);

            schedule=extract_schedule((*log), period);

            // XXX/jakob - this isn't fair. we're comparing the cost
            // of matching to a short interval to the cost of matching
            // to a long interval
            int cost=badness_of_match(*schedule,
                                      *log,
                                      (int)(period*WARPING_WINDOW),
                                      period);

            if(cost<best_cost) {
                log_debug("Found a new best period %d and schedule at time %d, cost is %d.\n",
                       period,
                       (*log)[current_event].start,
                       cost);
                       if(best_schedule)
                       delete best_schedule;

                best_schedule=schedule;
                best_schedule_len=schedule_len;
                best_period=period;
                best_cost=cost;
            }
            else
                delete schedule;
        }

        delete log;
    }

    log_debug("Best period %d, cost %d, schedule: ",best_period,best_cost);
    print_log(*best_schedule, 1);

    delete full_log;
}

*/

}
