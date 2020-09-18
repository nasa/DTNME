#include "utils.h"




/* ------------------------------------------
 * mega2byte
 *
 * Converts MBytes into Bytes
 * ------------------------------------------ */
long mega2byte(long n)
{
    return (n * 1000000);
} // end mega2byte


/* ------------------------------------------
 * kilo2byte
 *
 * Converts KBytes into Bytes
 * ------------------------------------------ */
long kilo2byte(long n)
{
    return (n * 1000);
} // end kilo2byte


/* ------------------------------------------
 * findDataUnit
 *
 * Extracts the data unit from the given string.
 * If no unit is specified, returns 'Z'.
 * ------------------------------------------ */
char find_data_unit(const char *inarg)
{
    // units are B (Bytes), K (KBytes) and M (MBytes)
    const char unitArray[] =
        {'B', 'K', 'M'
        };
    char * unit = malloc(sizeof(char));

    if ((unit = strpbrk(inarg, unitArray)) == NULL)
    {
        unit = "Z";
    }
    return unit[0];
} // end find_data_unit


/* ------------------------------------------
 * add_time
 * ------------------------------------------ */
void add_time(struct timeval *tot_time, struct timeval part_time)
{
    tot_time->tv_sec += part_time.tv_sec;
    tot_time->tv_usec += part_time.tv_sec;

    if (tot_time->tv_usec >= 1000000)
    {
        tot_time->tv_sec++;
        tot_time->tv_usec -= 1000000;
    }

} // end add_time


/* --------------------------------------------------
 * csv_time_report
 * -------------------------------------------------- */
void csv_time_report(int b_sent, int payload, struct timeval start, struct timeval end, FILE* csv_log)
{
    const char* time_report_hdr = "BUNDLE_SENT,PAYLOAD (byte),TIME (s),DATA_SENT (Mbyte),GOODPUT (Mbit/s)";


    double g_put, data;

    data = (b_sent * payload)/1000000.0;

    fprintf(csv_log, "\n\n%s\n", time_report_hdr);

    g_put = (data * 8 * 1000) / ((double)(end.tv_sec - start.tv_sec) * 1000.0 +
                          (double)(end.tv_usec - start.tv_usec) / 1000.0);
    
    double time_s =  ((double)(end.tv_sec - start.tv_sec) * 1000.0 + (double)(end.tv_usec - start.tv_usec) / 1000.0) / 1000;
    
    fprintf(csv_log, "%d,%d,%.1f,%E,%.3f\n", b_sent, payload, time_s, data, g_put);

} // end csv_time_report




/* --------------------------------------------------
 * csv_data_report
 * -------------------------------------------------- */
void csv_data_report(int b_id, int payload, struct timeval start, struct timeval end, FILE* csv_log)
{
    const char* data_report_hdr = "BUNDLE_ID,PAYLOAD (byte),TIME (s),GOODPUT (Mbit/s)";
    // const char* time_hdr = "BUNDLES_SENT,PAYLOAD,TIME,GOODPUT";
    double g_put;

    fprintf(csv_log, "\n\n%s\n", data_report_hdr);

    g_put = (payload * 8) / ((double)(end.tv_sec - start.tv_sec) * 1000.0 +
                             (double)(end.tv_usec - start.tv_usec) / 1000.0) / 1000.0;
                             
    double time_s =  ((double)(end.tv_sec - start.tv_sec) * 1000.0 + (double)(end.tv_usec - start.tv_usec) / 1000.0) / 1000;

    fprintf(csv_log, "%d,%d,%.1f,%.3f\n", b_id, payload, time_s, g_put);

} // end csv_data_report



/* -------------------------------------------------------------------
 * pattern
 *
 * Initialize the buffer with a pattern of (index mod 10).
 * ------------------------------------------------------------------- */
void pattern(char *outBuf, int inBytes)
{
    assert (outBuf != NULL);
    while (inBytes-- > 0)
    {
        outBuf[inBytes] = (inBytes % 10) + '0';
    }
} // end pattern



/* -------------------------------------------------------------------
 * Set timestamp to the given seconds
 * ------------------------------------------------------------------- */
struct timeval set( double sec )
{
    struct timeval mTime;

    mTime.tv_sec = (long) sec;
    mTime.tv_usec = (long) ((sec - mTime.tv_sec) * 1000000);

    return mTime;
} // end set


/* -------------------------------------------------------------------
 * Add seconds to my timestamp.
 * ------------------------------------------------------------------- */
struct timeval add( double sec )
{
    struct timeval mTime;

    mTime.tv_sec = (long) sec;
    mTime.tv_usec = (long) ((sec - ((long) sec )) * 1000000);

    // watch for overflow
    if ( mTime.tv_usec >= 1000000 )
    {
        mTime.tv_usec -= 1000000;
        mTime.tv_sec++;
    }
    assert( mTime.tv_usec >= 0 && mTime.tv_usec < 1000000 );

    return mTime;
} // end add


/* --------------------------------------------------
 * show_report
 * -------------------------------------------------- */
void show_report (u_int buf_len, char* eid, struct timeval start, struct timeval end, long data, FILE* output)
{
    double g_put;

    double time_s = ((double)(end.tv_sec - start.tv_sec) * 1000.0 + (double)(end.tv_usec - start.tv_usec) / 1000.0) / 1000.0;

    double data_MB = data / 1000000.0;
    
    if (output == NULL)
        printf("got %d byte report from [%s]: time=%.1f s - %E Mbytes sent", buf_len, eid, time_s, data_MB);
    else
        fprintf(output, "\n total time=%.1f s - %E Mbytes sent", time_s, data_MB);
  
    // report goodput (bits transmitted / time)
    g_put = (data_MB * 8) / time_s;
    if (output == NULL)
        printf(" (goodput = %.3f Mbit/s)\n", g_put);
    else
        fprintf(output, " (goodput = %.3f Mbit/s)\n", g_put);

    // report start - end time
    if (output == NULL)
        printf(" started at %u sec - ended at %u sec\n", (u_int)start.tv_sec, (u_int)end.tv_sec);
    else
        fprintf(output, " started at %u sec - ended at %u sec\n", (u_int)start.tv_sec, (u_int)end.tv_sec);

} // end show_report


char* get_filename(char* s)
{
    int i = 0, k;
    char* temp;
    char c = 'a';
    k = strlen(s) - 1;
    temp = malloc(strlen(s));
    strcpy(temp, s);
    while ((c != '/') && (k >= 0))
    {
        c = temp[k];
        k--;
    }

    if (c == '/')
        k += 2;

    else
        return temp;

    while (k != (int)strlen(temp))
    {
        temp[i] = temp[k];
        i++;
        k++;
    }
    temp[i] = '\0';

    return temp;
} // end get_filename



