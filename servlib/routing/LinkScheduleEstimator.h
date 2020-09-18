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

#include <vector>
#include <oasys/util/IntUtils.h>
#include <oasys/debug/Log.h>

#define CONTACT(s, d) { s, d }
#define absdiff(x,y) ((x<y)?((y)-(x)):((x)-(y)))

#define WARPING_WINDOW .02
#define PERIOD_TOLERANCE .02
#define MAX_DIST 1<<30

namespace dtn {
    
/**
 *    Given a log on the form (start1, duration1), ... ,(startN,
 *    durationN), the LinkScheduleEstimator algorithm figures out a
 *    periodic schedule that this log conforms to.
 *
 *    The schedule computed can then be used to predict future link-up
 *    events, and to inform far-away nodes about the future predicted
 *    availability of the link in question.
 *
 *    Usage:
 *      Log* find_schedule(Log* log);
 *
 *    Returns the best schedule for the given log. If there's no
 *    discernible periodicity in the log, the return value will be
 *    NULL.
 */
class LinkScheduleEstimator : public oasys::Logger {
public:
    typedef struct {
      unsigned int start;
      unsigned int duration;
    } LogEntry;
    
    typedef std::vector<LogEntry> Log;
    
    static Log* find_schedule(Log* log);

    LinkScheduleEstimator();
private:
    unsigned int entry_dist(Log &a, unsigned int a_index, unsigned int a_offset,
                            Log &b, unsigned int b_index, unsigned int b_offset,
                            unsigned int warping_window);

    unsigned int log_dist_r(Log &a, unsigned int a_index, unsigned int a_offset,
                            Log &b, unsigned int b_index, unsigned int b_offset,
                            unsigned int warping_window);
    
       
    unsigned int log_dist(Log &a, unsigned int a_offset,
                 Log &b, unsigned int b_offset,
                 unsigned int warping_window, int print_table);
    
    unsigned int autocorrelation(Log &log, unsigned int phase, int print_table);    
    void print_log(Log &log, int relative_dates);
    
    Log* generate_samples(Log &schedule,
                          unsigned int log_size,
                          unsigned int start_jitter,
                          double duration_jitter);
    
    unsigned int estimate_period(Log &log);       
    unsigned int seek_to_before_date(Log &log, unsigned int date);                
    unsigned int closest_entry_to_date(Log &log, unsigned int date);
    Log* clone_subsequence(Log &log, unsigned int start, unsigned int len);
        
    unsigned int badness_of_match(Log &pattern,
                                  Log &log,
                                  unsigned int warping_window, 
                                  unsigned int period);
    
    Log* extract_schedule(Log &log, unsigned int period_estimate);    
    unsigned int refine_period(Log &log, unsigned int period_estimate);
    Log* find_schedule(Log &log);    
};


}
