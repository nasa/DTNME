#ifndef BUNDLE_TOOLS_H_
#define BUNDLE_TOOLS_H_

#include "includes.h"


typedef struct
{
    dtn_bundle_id_t bundle_id;
    struct timeval send_time;
    u_int relative_id;
}
send_information_t;


dtn_endpoint_id_t* parse_eid(dtn_handle_t handle, dtn_endpoint_id_t *eid, char *str);
long bundles_needed (long data, long pl);
void print_eid(char * label, dtn_endpoint_id_t *eid);


void init_info(send_information_t *send_info, int window);
long add_info(send_information_t* send_info, dtn_bundle_id_t bundle_id, struct timeval p_start, int window);
int is_in_info(send_information_t* send_info, dtn_bundle_id_t bundle_id, int window);
void remove_from_info(send_information_t* send_info, int position);


#endif /*BUNDLE_TOOLS_H_*/
