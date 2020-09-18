#include "bundle_tools.h"

/* ----------------------------
 * parse_eid
 * ---------------------------- */
dtn_endpoint_id_t* parse_eid(dtn_handle_t handle, dtn_endpoint_id_t *eid, char *str)
{
    // try the string as an actual dtn tuple
    if (!dtn_parse_eid_string(eid, str))
    {
        return eid;
    }

    // build a local tuple based on the configuration of our dtn
    // router plus the str as demux string
    else if (!dtn_build_local_eid(handle, eid, str))
    {
        return eid;
    }
    else
    {
        return NULL;
    }
} // end parse_eid


/* ----------------------------------------------
 * bundles_needed
 * ---------------------------------------------- */
long bundles_needed (long data, long pl)
{
    long res = 0;
    ldiv_t r;

    r = ldiv(data, pl);
    res = r.quot;
    if (r.rem > 0)
        res += 1;

    return res;
} // end bundles_needed



/* ----------------------------
 * print_eid
 * ---------------------------- */
void print_eid(char * label, dtn_endpoint_id_t * eid)
{
    printf("%s [%s]\n", label, eid->uri);
} // end print_eid




void init_info(send_information_t *send_info, int window)
{
    int i;

    for (i = 0; i < window; i++)
    {
        send_info[i].bundle_id.creation_ts.secs = 0;
        send_info[i].bundle_id.creation_ts.seqno = 0;
    }
} // end init_info



long add_info(send_information_t* send_info, dtn_bundle_id_t bundle_id, struct timeval p_start, int window)
{
    int i;

    static u_int id = 0;
    static int last_inserted = -1;
    for (i = (last_inserted + 1); i < window ; i++)
    {
        if ((send_info[i].bundle_id.creation_ts.secs == 0) && (send_info[i].bundle_id.creation_ts.seqno == 0))
        {
            send_info[i].bundle_id.creation_ts.secs = bundle_id.creation_ts.secs;
            send_info[i].bundle_id.creation_ts.seqno = bundle_id.creation_ts.seqno;
            send_info[i].send_time.tv_sec = p_start.tv_sec;
            send_info[i].send_time.tv_usec = p_start.tv_usec;
            send_info[i].relative_id = id;
            last_inserted = i;
            return id++;
        }
    }
    for (i = 0; i <= last_inserted ; i++)
    {
        if ((send_info[i].bundle_id.creation_ts.secs == 0) && (send_info[i].bundle_id.creation_ts.seqno == 0))
        {
            send_info[i].bundle_id.creation_ts.secs = bundle_id.creation_ts.secs;
            send_info[i].bundle_id.creation_ts.seqno = bundle_id.creation_ts.seqno;
            send_info[i].send_time.tv_sec = p_start.tv_sec;
            send_info[i].send_time.tv_usec = p_start.tv_usec;
            send_info[i].relative_id = id;
            last_inserted = i;
            return id++;
        }
    }
    return -1;
} // end add_info


int is_in_info(send_information_t* send_info, dtn_bundle_id_t bundle_id, int window)
{
    int i;

    static int last_collected = -1;
    for (i = (last_collected + 1); i < window; i++)
    {
        if ((send_info[i].bundle_id.creation_ts.secs == bundle_id.creation_ts.secs) && (send_info[i].bundle_id.creation_ts.seqno == bundle_id.creation_ts.seqno))
        {
            last_collected = i;
            return i;
        }
    }
    for (i = 0; i <= last_collected; i++)
    {
        if ((send_info[i].bundle_id.creation_ts.secs == bundle_id.creation_ts.secs) && (send_info[i].bundle_id.creation_ts.seqno == bundle_id.creation_ts.seqno))
        {
            last_collected = i;
            return i;
        }

    }
    return -1;
} // end is_in_info


void remove_from_info(send_information_t* send_info, int position)
{
    send_info[position].bundle_id.creation_ts.secs = 0;
    send_info[position].bundle_id.creation_ts.seqno = 0;
} // end remove_from_info



