/*
 *    Copyright 2010-2011 Trinity College Dublin
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

/*
 *    Modifications made to this file by the patch file dtnme_mfs-33289-1.patch
 *    are Copyright 2015 United States Government as represented by NASA
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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <inttypes.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <strings.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "dtn_api.h"
#include "sdnv-c.h"

#define BUFSIZE 16
#define BLOCKSIZE 8192
#define COUNTER_MAX_DIGITS 9

// Find the maximum commandline length
#ifdef __FreeBSD__
/* Needed for PATH_MAX, Linux doesn't need it */
#include <sys/syslimits.h>
#endif

#ifndef PATH_MAX
/* A conservative fallback */
#define PATH_MAX 1024
#endif

//global variables
const char* progname;

int  api_IP_set =  0;
char api_IP[40] = "127.0.0.1";
int  api_port   = 5010;

/*******************************************************************************
* usage:
* display cmd line options to user.
*******************************************************************************/
int
usage()
{
    fprintf(stderr, "usage: %s -l < local endpoint > -f < matching filename > "
                    "[opts]\n", progname);
    fprintf(stderr, "options:\n");
    fprintf(stderr, " -n  < count > exit after count bundles received\n");
    fprintf(stderr, " -r  < eid > reply to endpoint\n");
    fprintf(stderr, " -i  < regid > existing registration id\n");
    fprintf(stderr, " -E  < seconds > registration expiry time\n");
    fprintf(stderr, " -a  < defer | drop | exec > failure action\n");
    fprintf(stderr, " -S  < script > failure script for exec action\n");
    fprintf(stderr, " -P  < bulk | normal | expedited | reserved > priority\n");
    fprintf(stderr, " -D  request end-to-end delivery receipt\n");
    fprintf(stderr, " -X  request deletion receipt\n");
    fprintf(stderr, " -F  request bundle forwarding receipts\n");
    fprintf(stderr, " -R  request bundle reception receipts\n");
    fprintf(stderr, " -c  request custody transfer\n");
    fprintf(stderr, " -C  request custody transfer receipts\n");
    fprintf(stderr, " -A  api IP address optional (default is localhost)\n");
    fprintf(stderr, " -B  api port optional (default is 5010)\n");
    fprintf(stderr, " -1  assert destination endpoint is a singleton\n");
    fprintf(stderr, " -N  assert destination endpoint is not a singleton\n");
    fprintf(stderr, " -W  set the do not fragment option\n");
    fprintf(stderr, " -h  help\n");
    fprintf(stderr, " -v  verbose\n");

    return DTN_SUCCESS;
}

/*******************************************************************************
* parse options:
* set internal variables based on cmd line args.
* calls parse matching file if required.
* returns success or exits on failure.
*******************************************************************************/
int
parse_options(int argc, char** argv,
    char * local_eid_name,              // l
    char * reply_eid_name,              // r
    char * matching_filename,           // f
    int * count,                        // n
    dtn_reg_id_t * regid,               // i
    dtn_timeval_t * reg_expiry,         // E
    int * reg_fail_action,              // A
    char * reg_fail_script,             // S
    dtn_bundle_priority_t * priority,   // P
    int * delivery_options,             // D X F R c C 1 N W
    int * verbose)                      // v
{
    int c, done = 0;

    progname = argv[0];

    //initialize strings
    memset(local_eid_name,    0, sizeof(char) * PATH_MAX);
    memset(reply_eid_name,    0, sizeof(char) * PATH_MAX);
    memset(matching_filename, 0, sizeof(char) * PATH_MAX);
    memset(reg_fail_script,   0, sizeof(char) * PATH_MAX);

    while( !done )
    {
        c = getopt(argc, argv, "A:B:l:f:n:r:i:E:a:S:P:DXFRcC1NWvhH");
        switch(c)
        {
        case 'l':
            strncpy(local_eid_name, optarg, PATH_MAX);
            break;
        case 'r':
            strncpy(reply_eid_name, optarg, PATH_MAX);
            break;
        case 'f':
            strncpy(matching_filename, optarg, PATH_MAX);
            break;
        case 'n':
            *count = atoi(optarg);
            break;
        case 'i':
            *regid = atoi(optarg);
            break;
        case 'E':
            *reg_expiry = atoi(optarg);
            break;
        case 'A':
            api_IP_set = 1;
            strcpy(api_IP,optarg);
            break;
        case 'B':
            api_IP_set = 1;
            api_port  = atoi(optarg);
            break;
        case 'a':
            if (!strcasecmp(optarg, "defer")) {
                *reg_fail_action = DTN_REG_DEFER;

            } else if (!strcasecmp(optarg, "drop")) {
                *reg_fail_action = DTN_REG_DROP;

            } else if (!strcasecmp(optarg, "exec")) {
                *reg_fail_action = DTN_REG_EXEC;

            } else {
                fprintf(stderr, "invalid failure action '%s'\n", optarg);
                usage();
                exit(1);
            }
            break;
        case 'S':
            strncpy(reg_fail_script, optarg, PATH_MAX);
            break;
        case 'P':
            if (!strcasecmp(optarg, "bulk"))   {
                *priority = COS_BULK;
            } else if (!strcasecmp(optarg, "normal")) {
                *priority = COS_NORMAL;
            } else if (!strcasecmp(optarg, "expedited")) {
                *priority = COS_EXPEDITED;
            } else if (!strcasecmp(optarg, "reserved")) {
                *priority = COS_RESERVED;
            } else {
                fprintf(stderr, "invalid priority value %s\n", optarg);
                usage();
                exit(1);
            }
            break;
        case 'D':
            *delivery_options |= DOPTS_DELIVERY_RCPT;
            break;
        case 'X':
            *delivery_options |= DOPTS_DELETE_RCPT;
            break;
        case 'F':
            *delivery_options |= DOPTS_FORWARD_RCPT;
            break;
        case 'R':
            *delivery_options |= DOPTS_RECEIVE_RCPT;
            break;
        case 'c':
            *delivery_options |= DOPTS_CUSTODY;
            break;
        case 'C':
            *delivery_options |= DOPTS_CUSTODY_RCPT;
            break;
        case '1':
            *delivery_options |= DOPTS_SINGLETON_DEST;
            break;
        case 'N':
            *delivery_options |= DOPTS_MULTINODE_DEST;
            break;
        case 'W':
            *delivery_options |= DOPTS_DO_NOT_FRAGMENT;
            break;
        case 'v':
            *verbose = 1;
            break;
        case 'h':
        case 'H':
            usage();
            exit(0);
        case -1:
            done = 1;
            break;
        default:
            // getopt already prints error message for unknown option characters
            usage();
            exit(1);
        }             
    }

    // if no reply-to eid set, use the local eid
    if (*reply_eid_name == 0)
        strncpy(reply_eid_name, local_eid_name, PATH_MAX);

    return DTN_SUCCESS;
}

/*******************************************************************************
* validate options:
* returns success or exits on failure 
*******************************************************************************/
int
validate_options(const char * local_eid_name,
    const char * reply_eid_name,
    const char * matching_filename)
{
#define REQUIRE(test, err_msg) \
    if(!(test)) { \
        fprintf(stderr, err_msg); \
        usage(); \
        exit(1); \
    }

    FILE* file;

    REQUIRE(strlen(local_eid_name) > 0, "-l <local eid> required\n");
    REQUIRE(strlen(reply_eid_name) > 0, "-r <reply eid> required\n");
    REQUIRE(strlen(matching_filename) > 0, "-f <matching filename> required\n");
    REQUIRE((file = fopen(matching_filename, "r")) != 0, "matching file not found");
    
	fclose (file);
	return DTN_SUCCESS;

#undef REQUIRE
}

/*******************************************************************************
* register with dtn:
* 
*******************************************************************************/
int
register_with_dtn(dtn_handle_t handle,
    dtn_endpoint_id_t * local_eid,
    const char * local_eid_name,
    dtn_reg_id_t * regid,
    dtn_timeval_t reg_expiration,
    int reg_fail_action,
    char * reg_fail_script)
{
    int call_bind = 0;
    dtn_reg_info_t reginfo;

    memset(local_eid, 0, sizeof(dtn_endpoint_id_t));

    // if no regid has been given we need to create a new registration
    if (*regid == DTN_REGID_NONE) {
        if (local_eid_name[0] == '/') {
            if (dtn_build_local_eid(handle, local_eid, local_eid_name) != DTN_SUCCESS) {
                fprintf(stderr, "error building local eid: %s\n",
                        dtn_strerror(dtn_errno(handle)));
                dtn_close(handle); 
                exit(1);
            }
        } else {
            if (dtn_parse_eid_string(local_eid, local_eid_name) != DTN_SUCCESS) {
                fprintf(stderr, "error parsing eid string: %s\n",
                        dtn_strerror(dtn_errno(handle)));
                dtn_close(handle);
                exit(1);
            }
        }

        memset(&reginfo, 0, sizeof(dtn_reg_info_t));

        // create a new registration based on this eid
        dtn_copy_eid(&reginfo.endpoint, local_eid);
        reginfo.regid             = *regid;
        reginfo.expiration        = reg_expiration;
        reginfo.flags             = reg_fail_action;
        reginfo.script.script_val = reg_fail_script;
        reginfo.script.script_len = strlen(reg_fail_script) + 1;
    }

    // try to see if there is an existing registration that matches
    // the given endpoint, in which case we'll use that one.
    if (*regid == DTN_REGID_NONE) {
        if (dtn_find_registration(handle, local_eid, regid) != DTN_SUCCESS &&
            dtn_errno(handle) != DTN_ENOTFOUND) {
            fprintf(stderr, "error finding registration: %s\n",
                    dtn_strerror(dtn_errno(handle)));
            dtn_close(handle);
            exit(1);
        }
        call_bind = 1;
    }

    // if the user didn't give us a registration to use, get a new one
    if (*regid == DTN_REGID_NONE) {
        if (dtn_register(handle, &reginfo, regid) != DTN_SUCCESS) {
            fprintf(stderr, "error registering: %s\n",
                    dtn_strerror(dtn_errno(handle)));
            dtn_close(handle);
            exit(1); 
        }
        call_bind = 0;
    } else {
        call_bind = 1;
    }

    if (call_bind) {
        // bind the current handle to the found registration
        if (dtn_bind(handle, *regid) != DTN_SUCCESS) {
            fprintf(stderr, "error binding to registration: %s\n",
                    dtn_strerror(dtn_errno(handle)));
            dtn_close(handle);
            exit(1);
        }
    }

    return DTN_SUCCESS;
}

/*******************************************************************************
* parse eid:
*  
* code lifted from dtnsend
*******************************************************************************/
dtn_endpoint_id_t *
parse_eid(dtn_handle_t handle,
    dtn_endpoint_id_t * eid,
    const char * str,
    int verbose)
{
    // try the string as an actual dtn eid
    if (dtn_parse_eid_string(eid, str) == DTN_SUCCESS) {
        if (verbose) fprintf(stdout, "%s (literal)\n", str);
        return eid;
    }

    // build a local eid based on the configuration of our dtn
    // router plus the str as demux string
    else if (dtn_build_local_eid(handle, eid, str) == DTN_SUCCESS) {
        if (verbose) fprintf(stdout, "%s (local)\n", str);
        return eid;
    }
    else {
        fprintf(stderr, "invalid eid string '%s'\n", str);
        exit(1);
    }
}

/*******************************************************************************
* trim whitespace:
* first move past any leading whitespace 
* after the first non-whitespace char begin copying to output
* finish by setting any trailing whitespace to 0
* @return 0 on success or -1 if input not completely read
*******************************************************************************/
int
trim_whitespace(const char * in, char * out, int out_len)
{
    int i=0, j=0;
    int in_len;
    char whitespace[] = " \t\n\r";

    memset(out, 0, out_len);

    // move past any leading whitespace
    // by testing if the current char is in the whitespace string
    in_len = strlen(in);
    while ( i<in_len && strchr(whitespace, in[i]) != NULL )
    	++i;

    in_len = strlen(&(in[i]));
    if (in_len > out_len) {
    	fprintf(stderr, "Error trimming whitespace, input string [%d] is longer"
    			" than output string [%d]\n", in_len, out_len);
    	return -1;
    }
    
    // copy the body
    strncpy(out, &(in[i]), in_len);

    // remove any trailing whitespace
    // by testing if the current char is in the whitespace string
    for (j = strlen(out)-1; strchr(whitespace, in[i]) != NULL; --j)
    	out[j--] = 0;

    return DTN_SUCCESS;
}   

/*******************************************************************************
* escape spaces:
* first move past any leading whitespace 
* after the first non-whitespace char escape any whitespace
* @return 0 on success or -1 if input not completely read
*******************************************************************************/
int
escape_spaces(const char * in, char * out, size_t out_len)
{
    u_int i=0, j=0;
    char escape = '\\';
    char space = ' ';

    memset(out, 0, out_len);

    // move past any leading whitespace
    while (i<strlen(in) && in[i] == space)
        ++i;
    
    // body case
    for ( ; i<strlen(in) && j<out_len-2; ++i){
        if (in[i] == space && in[i-1] != escape) {
            out[j++] = escape;
            out[j++] = space;
        } else {
            out[j++] = in[i];
        }
    }

    if (i <  strlen(in))
        return -1;
    else
        return 0;
}   

/*******************************************************************************
* response_path_exists
* first escape any spaces in the path name
* then try to open the file for reading
* @return 0 if exists or -1 if not found
*******************************************************************************/
int
response_path_exists(const char * path)
{
    FILE *file;
    char esc_path[PATH_MAX];

    // trim & escape spaces in dir before calling ls
    if (escape_spaces(path, esc_path, PATH_MAX) != DTN_SUCCESS)
        return -1;
    
    if ((file = fopen(esc_path, "r")) == 0) {
    	fprintf(stderr, "Error: the path %s specified in the matching file"
    			" could not be found\n", path);
		return -1;
    } else {
    	fclose (file);
		return DTN_SUCCESS;
    }
}


/*******************************************************************************
* match bpq:
* read in paths to search for query in from 'matching file'
* for each record in the matching file, extract the response path
* matching file format: [matching_rule, query, response_path, response_type, expiry]
*******************************************************************************/
int
match_bpq(const dtn_bpq_extension_block_data_t * bpq,
    const char * matching_filename,
    u_int * response_kind,
    char  * response_path,
    int     response_path_len,
    dtn_timeval_t * response_expiry,
    int   * found,
    int     verbose)
{
    char line[PATH_MAX];
    char trim_path[PATH_MAX];
    char * matching_rule;
    char * query;
    char * path;
    char * kind;
    char * expiry;
    FILE * file;

    if ((file = fopen(matching_filename, "r")) == 0) {
    	fprintf(stderr, "Error opening matching file: %s\n",
    			matching_filename);
        return -1;
    }
    
    if (verbose) fprintf(stdout, "Matching query: %s\n",
                                 bpq->query.query_val);

    while (1) {
    	memset(line, 0 , PATH_MAX);
    	*found = 0;

    	// read line from matching file
    	// TODO: handle malformed input from matching file
    	if (fgets(line, PATH_MAX, file) == NULL) {
    		break;
    	}

    	if (strncmp(line, "#", 1) == 0)
    		continue;

    	// if a null pointer is returned, loop again to get to the next line
    	if ((matching_rule 	= strtok(line, ",")) == NULL) continue;
    	if ((query 			= strtok(NULL, ",")) == NULL) continue;
    	if ((path 	        = strtok(NULL, ",")) == NULL) continue;
    	if ((kind  			= strtok(NULL, ",")) == NULL) continue;
    	if ((expiry			= strtok(NULL, ",")) == NULL) continue;


    	// match query
        if (atoi(matching_rule) != (int)bpq->matching_rule		||
			BPQ_MATCHING_RULE_EXACT != bpq->matching_rule		||
			strlen(query) != strlen(bpq->query.query_val)		||
        	strncmp(query, bpq->query.query_val, strlen(query)) != 0) {

        	continue;
        }


        // trim whitespace from response path
        trim_whitespace(path, trim_path, PATH_MAX);

        // make sure the file exists
        if (response_path_exists(trim_path) == DTN_SUCCESS) {
        	*found = 1;
        	*response_kind = (u_int) atoi(kind);
        	*response_expiry = (dtn_timeval_t) atoi(expiry);
        	strncpy(response_path, trim_path, response_path_len);

			break;
        } else {
        	continue;
        }
    }
    fclose (file);

    return DTN_SUCCESS;
}

/*******************************************************************************
* bpq to char array
* encode the following information:
*
*   BPQ-kind             		1-byte
*   Matching rule 				1-byte
*
*   Creation time-stamp	sec		SDNV
*   Creation time-stamp	seq		SDNV
*   Source EID length			SDNV
*   Source EID 					n-bytes
*
*   Query value length     		SDNV
*   Query value            		n-bytes
*
*   Number of fragments  		SDNV
*   Fragment offsets    		SDNV
*   Fragment lengths     		SDNV
*
* @return The number of bytes or -1 on error
*******************************************************************************/
int
bpq_to_char_array(const dtn_bpq_extension_block_data_t * bpq,
    char* buf,
    int buf_len,
    int verbose)
{
    int i=0, j=0;
	u_int k=0;
    int q_encoding_len, f_encoding_len, encoding_len;
    u_char encoding[PATH_MAX];

    memset(buf, 0, buf_len);

    // BPQ-kind             1-byte
    if (i < buf_len)    buf[i++] = (char) bpq->kind;

    // matching rule type   1-byte
    if (i < buf_len)    buf[i++] = (char) bpq->matching_rule;

    // Timestamp secs (SDNV)
	if ( (q_encoding_len = sdnv_encode (bpq->original_id.creation_ts.secs,
										encoding, PATH_MAX)) == -1 ) {
		fprintf (stderr, "Error encoding creation timestamp secs\n");
		return -1;
	} else {
		for (j=0; i<buf_len && j<q_encoding_len; ++j)
	        buf[i++] = encoding[j];
	}

    // Timestamp seqno (SDNV)
	if ( (q_encoding_len = sdnv_encode (bpq->original_id.creation_ts.seqno,
										encoding, PATH_MAX)) == -1 ) {
		fprintf (stderr, "Error encoding creation timestamp seqno\n");
		return -1;
	} else {
		for (j=0; i<buf_len && j<q_encoding_len; ++j)
	        buf[i++] = encoding[j];
	}

    // Source EID len (SDNV)
	if ( (q_encoding_len = sdnv_encode (bpq->original_id.source_len,
										encoding, PATH_MAX)) == -1 ) {
		fprintf (stderr, "Error encoding source EID len\n");
		return -1;
	} else {
		for (j=0; i<buf_len && j<q_encoding_len; ++j)
	        buf[i++] = encoding[j];
	}

    // Source EID n-bytes
    for (j=0; i<buf_len && j<(int)bpq->original_id.source_len; ++j)
        buf[i++] = bpq->original_id.source.uri[j];



    // Query length (SDNV)
    if ( (q_encoding_len = sdnv_encode (bpq->query.query_len,
    									encoding, PATH_MAX)) == -1 ) {
    	fprintf (stderr, "Error encoding query len\n");
        return -1;
    } else {
    	for (j=0; i<buf_len && j<q_encoding_len; ++j)
    		buf[i++] = encoding[j];
    }

    // Query value n-bytes
    for (j=0; i<buf_len && j<(int)bpq->query.query_len; ++j)
        buf[i++] = bpq->query.query_val[j];



    // number of fragments  SDNV
    if ( (f_encoding_len = sdnv_encode (bpq->fragments.num_frag_returned,
    									encoding, PATH_MAX)) == -1 ){
    	fprintf (stderr, "Error encoding number of fragments\n");
        return -1;
    } else {
    	for (j=0; i<buf_len && j<f_encoding_len; ++j)
    		buf[i++] = encoding[j];
    }

    for (k=0; k<bpq->fragments.num_frag_returned; ++k) {

        // fragment offsets     SDNV
        if ( (encoding_len = sdnv_encode (bpq->fragments.frag_offsets[k],
        								  encoding, PATH_MAX)) == -1 ) {
        	fprintf (stderr, "Error encoding fragment offset[%d]\n", k);
        	return -1;
        } else {
        	for (j=0; i<buf_len && j<encoding_len; ++j)
        		buf[i++] = encoding[j];
        }

        // fragment lengths     SDNV
        if ( (encoding_len = sdnv_encode (bpq->fragments.frag_lenghts[k],
        								  encoding, PATH_MAX)) == -1 ) {
        	fprintf (stderr, "Error encoding fragment length[%d]\n", k);
        	return -1;
        } else {
        	for (j=0; i<buf_len && j<encoding_len; ++j)
        		buf[i++] = encoding[j];
        }
    }

    if (verbose) {
        fprintf (stdout, "\nbpq_to_char_array (buf_len:%d, i:%d):\n",buf_len,i);
        fprintf (stdout, "             kind: %d\n", (int) buf[0]);
        fprintf (stdout, "    matching rule: %d\n", (int) buf[1]);

        fprintf (stdout, "  creation ts sec: %d\n",
        		 (int) bpq->original_id.creation_ts.secs);
        fprintf (stdout, "  creation ts seq: %d\n",
        		 (int) bpq->original_id.creation_ts.seqno);
        fprintf (stdout, "   source eid len: %d\n",
        		 (int) bpq->original_id.source_len);
        fprintf (stdout, "       source eid: %s\n",
        		 bpq->original_id.source.uri);

        fprintf (stdout, "        query len: %d\n", bpq->query.query_len);
        fprintf (stdout, "   q_encoding_len: %d\n", q_encoding_len);
        fprintf (stdout, "        query val: %s\n", bpq->query.query_val);

        fprintf (stdout, "     fragment len: %d\n", bpq->fragments.num_frag_returned);
        fprintf (stdout, "   f_encoding_len: %d\n\n", f_encoding_len);
    }

    return i;
}

/*******************************************************************************
* char array to bpq
* decode the following information:
*
*   BPQ-kind             		1-byte
*   Matching rule 				1-byte
*
*   Creation time-stamp	sec		SDNV
*   Creation time-stamp	seq		SDNV
*   Source EID length			SDNV
*   Source EID 					n-bytes
*
*   Query value length     		SDNV
*   Query value            		n-bytes
*
*   Number of fragments  		SDNV
*   Fragment offsets    		SDNV
*   Fragment lengths     		SDNV
*
* @return The number of bytes or -1 on error
*******************************************************************************/
int
char_array_to_bpq(const u_char* buf,
    int buf_len,
    dtn_bpq_extension_block_data_t * bpq,
    int verbose)
{
    int i=0, j=0;
    u_int64_t uval64;
    int q_decoding_len, f_decoding_len, decoding_len;

    // BPQ-kind             1-byte
    if (i<buf_len) bpq->kind = (u_int) buf[i++];

    // matching rule type   1-byte
    if (i<buf_len) bpq->matching_rule = (u_int) buf[i++];



    // Creation time-stamp sec     SDNV
    if ( (q_decoding_len = sdnv_decode (&(buf[i]),
    									buf_len - i,
    									&(bpq->original_id.creation_ts.secs))) == -1 ) {
        fprintf (stderr, "Error decoding creation time-stamp sec\n");
        return -1;
    }
    i += q_decoding_len;

    // Creation time-stamp seq     SDNV
    if ( (q_decoding_len = sdnv_decode (&(buf[i]),
    									buf_len - i,
    									&(bpq->original_id.creation_ts.seqno))) == -1 ) {
        fprintf (stderr, "Error decoding creation time-stamp seq\n");
        return -1;
    }
    i += q_decoding_len;

    // Source EID length     SDNV
    q_decoding_len = sdnv_decode (&(buf[i]), buf_len - i, &uval64);
    if ( q_decoding_len == -1 ) {
        fprintf (stderr, "Error decoding source EID length\n");
        return -1;
    }
    bpq->original_id.source_len = (u_int) uval64;
    i += q_decoding_len;

    // Source EID            n-bytes
    if (i<buf_len && bpq->original_id.source_len <= DTN_MAX_ENDPOINT_ID) {
    	strncpy(bpq->original_id.source.uri, (char*)&(buf[i]), bpq->original_id.source_len);
    	i += bpq->original_id.source_len;
    } else {
    	fprintf (stderr, "Error copying source EID\n");
		return -1;
    }



    // BPQ-value-length     SDNV
    q_decoding_len = sdnv_decode (&(buf[i]), buf_len - i, &uval64);
    if ( q_decoding_len == -1 ) {
        fprintf (stderr, "Error decoding BPQ-value-length\n");
        return -1;
    }
    bpq->query.query_len = (u_int) uval64;
    i += q_decoding_len;

    // BPQ-value            n-bytes
    if (i<buf_len) bpq->query.query_val = (char*)&(buf[i]);
    	i += bpq->query.query_len;


    // number of fragments  SDNV
    f_decoding_len = sdnv_decode (&(buf[i]), buf_len - i, &uval64);
    if ( f_decoding_len == -1 ) {
        fprintf (stderr, "Error decoding number of fragments\n");
        return -1;
    }
    bpq->fragments.num_frag_returned = (u_int) uval64;
    i += f_decoding_len;

    if ( bpq->fragments.num_frag_returned > 0 ) {
    	bpq->fragments.frag_offsets = (u_int*) malloc ( sizeof(u_int) * bpq->fragments.num_frag_returned );
    	bpq->fragments.frag_lenghts = (u_int*) malloc ( sizeof(u_int) * bpq->fragments.num_frag_returned );
    } else {
    	bpq->fragments.frag_offsets = NULL;
    	bpq->fragments.frag_lenghts = NULL;
    }

    for (j=0; i<buf_len && j<(int)bpq->fragments.num_frag_returned; ++j) {

        // fragment offsets     SDNV
        decoding_len = sdnv_decode (&(buf[i]), buf_len - i, &uval64);
        if ( decoding_len == -1 ) {
            fprintf (stderr, "Error decoding fragment[%d] offset\n", j);
            return -1;
        }
        bpq->fragments.frag_offsets[j] = (u_int) uval64;
        i += decoding_len;

        // fragment lengths     SDNV
        decoding_len = sdnv_decode (&(buf[i]), buf_len - i, &uval64);
        if ( decoding_len == -1 ) {
            fprintf (stderr, "Error decoding fragment[%d] length\n", j);
            return -1;
        }
        bpq->fragments.frag_lenghts[j] = (u_int) uval64;
        i += decoding_len;
    }

    if (i != buf_len)
        return -1;

    if (verbose) {
        fprintf (stdout, "\nchar_array_to_bpq (buf_len:%d, i:%d):\n",buf_len, i);
        fprintf (stdout, "             kind: %d\n", (int) buf[0]);
        fprintf (stdout, "    matching rule: %d\n", (int) buf[1]);

        fprintf (stdout, "  creation ts sec: %d\n",
        		 (int) bpq->original_id.creation_ts.secs);
        fprintf (stdout, "  creation ts seq: %d\n",
        		 (int) bpq->original_id.creation_ts.seqno);
        fprintf (stdout, "   source eid len: %d\n",
        		 (int) bpq->original_id.source_len);
        fprintf (stdout, "       source eid: %s\n",
        		 bpq->original_id.source.uri);

        fprintf (stdout, "        query len: %d\n", bpq->query.query_len);
        fprintf (stdout, "   q_decoding_len: %d\n", q_decoding_len);
        fprintf (stdout, "        query val: %s\n", bpq->query.query_val);

        fprintf (stdout, "     fragment len: %d\n", bpq->fragments.num_frag_returned);
        fprintf (stdout, "   f_decoding_len: %d\n\n", f_decoding_len);
    }

    return DTN_SUCCESS;
}

/*******************************************************************************
* send response bpq:
* build a response bundle containing the queried object file as the payload
* attach a BPQ extension describing the query & object
* and send to the source of the query
* @return 0 if successful or -1 if error
*******************************************************************************/
int
send_response_bpq(dtn_handle_t * handle,
    dtn_reg_id_t regid,
    u_int response_kind,
    dtn_bundle_spec_t * query_bundle_spec,
    dtn_endpoint_id_t * reply_eid,
    dtn_bpq_extension_block_data_t * query_bpq_block_data,
    char * pathname,
    dtn_timeval_t bundle_expiry,
    dtn_bundle_priority_t  priority,
    int delivery_options,
    int verbose)
{
    int  ret = 0;
    char buf [PATH_MAX];
    int buf_len = 0;
    dtn_bundle_id_t                 response_bundle_id;
    dtn_bundle_spec_t               response_bundle_spec;
    dtn_extension_block_t           response_bpq_block;
    dtn_bpq_extension_block_data_t  response_bpq_block_data;
    dtn_bundle_payload_t            response_payload;

    memset(buf,                      0, PATH_MAX);
    memset(&response_bundle_spec,    0, sizeof(dtn_bundle_spec_t));
    memset(&response_bpq_block,      0, sizeof(dtn_extension_block_t));
    memset(&response_bpq_block_data, 0, sizeof(dtn_bpq_extension_block_data_t));
    memset(&response_payload,        0, sizeof(dtn_bundle_payload_t));

    // set the payload
    dtn_set_payload(&response_payload, DTN_PAYLOAD_FILE, pathname, strlen(pathname)); 

    // set the bpq block data
    response_bpq_block_data.kind = response_kind;
    response_bpq_block_data.matching_rule = query_bpq_block_data->matching_rule;
    response_bpq_block_data.query.query_len = query_bpq_block_data->query.query_len;
    response_bpq_block_data.query.query_val = query_bpq_block_data->query.query_val;

    if ( (buf_len = bpq_to_char_array(&response_bpq_block_data, buf, PATH_MAX, verbose)) == -1 ) {
        fprintf (stderr, "error encoding bpq: %d", buf_len);
        return -1;
    }

    // set the bpq block
    response_bpq_block.type = DTN_BPQ_BLOCK_TYPE;
    response_bpq_block.flags = BLOCK_FLAG_REPLICATE;
    response_bpq_block.data.data_len = buf_len;
    response_bpq_block.data.data_val = buf;

    // copy dest src
    dtn_copy_eid(&response_bundle_spec.dest,    &(query_bundle_spec->replyto));
    dtn_copy_eid(&response_bundle_spec.source,  &(query_bundle_spec->dest));
    dtn_copy_eid(&response_bundle_spec.replyto, reply_eid);

    // set the bundle spec dtn options
    response_bundle_spec.expiration = bundle_expiry;
    response_bundle_spec.dopts = delivery_options;
    response_bundle_spec.priority = priority;

    // set the bundle extension
    response_bundle_spec.blocks.blocks_len = 1;
    response_bundle_spec.blocks.blocks_val = &response_bpq_block;


    // send the bundle, bpq extension and empty payload
    ret = dtn_send(*handle, regid, &response_bundle_spec, &response_payload, &response_bundle_id);
    if (ret != DTN_SUCCESS) {
        fprintf(stderr, "error sending response bundle: %d (%s)\n",
                    ret, dtn_strerror(dtn_errno(*handle)));
    } else if (verbose) {
        fprintf(stdout, "bundle sent successfully: id %s,%"PRIu64".%"PRIu64"\n",
                    response_bundle_id.source.uri,
                    response_bundle_id.creation_ts.secs,
                    response_bundle_id.creation_ts.seqno);
    }
    return ret;
}

/*******************************************************************************
* receive bpq:
* listen for incoming bundles, 
* upon receipt of a new bundle ckeck for bpq expension block. 
* Attempt to match the query in the bpq.
* If a match is found, send a response containing the queied object to the 
* source of the query.
*******************************************************************************/
int
receive_bpq(dtn_handle_t * handle,
    dtn_reg_id_t regid,
    dtn_endpoint_id_t * reply_eid,
    const char * matching_filename,
    int  count,
    dtn_bundle_priority_t  priority,
    int delivery_options,
    int verbose)
{
    int i, j, num_blocks, found, ret = 0;
    dtn_timeval_t bundle_expiry = 3600; // default one hour
    u_int response_kind;
    char pathname[PATH_MAX];
    dtn_bundle_spec_t              bundle_spec;
    dtn_extension_block_t          * bpq_blocks;
    dtn_bpq_extension_block_data_t bpq_block_data;
    dtn_bundle_payload_t           payload;

    // start listening for bpq bundles
    for (i = 0; count == 0 || i < count; ++i) {
        found = 0;
        memset(&bundle_spec,    0, sizeof(dtn_bundle_spec_t));
        memset(&bpq_block_data, 0, sizeof(dtn_bpq_extension_block_data_t));
        memset(&payload,        0, sizeof(dtn_bundle_payload_t));
        memset(pathname,        0, PATH_MAX);
        pathname[0] = '\0';

        if (verbose) fprintf(stdout, "blocking waiting for dtn_recv\n");
        ret = dtn_recv(*handle,
                       &bundle_spec,
                       DTN_PAYLOAD_FILE,
                       &payload,
                       DTN_TIMEOUT_INF);
        if (ret != DTN_SUCCESS) {
            fprintf(stderr, "error receiving bundle: %d (%s)\n",
                    ret, dtn_strerror(dtn_errno(*handle)));
            return ret;
        } else if (verbose) {
            fprintf(stdout, "bundle %d received successfully: id %s,%"PRIu64".%"PRIu64"\n",
                             i,
                             bundle_spec.source.uri,
                             bundle_spec.creation_ts.secs,
                             bundle_spec.creation_ts.seqno);
        }

        // extract the bpq
        num_blocks = bundle_spec.blocks.blocks_len;
        bpq_blocks = bundle_spec.blocks.blocks_val;    

        for (j = 0; j < num_blocks; ++j) {
            if (bpq_blocks[j].type == DTN_BPQ_BLOCK_TYPE) {

                if (verbose) fprintf(stdout, "bundle %d contains a "
                                             "BPQ extension block\n", i);

                ret = char_array_to_bpq((u_char*)bpq_blocks[j].data.data_val,
                                        bpq_blocks[j].data.data_len, 
                                        &bpq_block_data,
                                        verbose);
                if (ret != DTN_SUCCESS) {
                    fprintf(stderr, "error decoding query bundle: %d\n", ret);
                    return ret;
                }

                match_bpq( 	&bpq_block_data,
                			matching_filename,
                			&response_kind,
                			pathname,
                			PATH_MAX,
                			&bundle_expiry,
                			&found,
                			verbose);

                break;
            }                
        }

        // if found respond and continue listening
        if (found) {
            if (verbose) fprintf(stdout, "BPQ match found for query: %s\n",
                                         bpq_block_data.query.query_val);

            ret = send_response_bpq(handle,
                                    regid,
                                    response_kind,
                                    &bundle_spec,
                                    reply_eid,
                                    &bpq_block_data,
                                    pathname,
                                    bundle_expiry,
                                    priority,
                                    delivery_options,
                                    verbose);
            if (ret != DTN_SUCCESS) {
                fprintf(stderr, "error sending response bundle: %d (%s)\n",
                        ret, dtn_strerror(dtn_errno(*handle)));
                return ret;
            }
        }
        dtn_free_payload(&payload);
    }

    // if fragment memory was allocated
    // free it now
    if ( bpq_block_data.fragments.frag_offsets != NULL ){
    	free(bpq_block_data.fragments.frag_offsets);
    	bpq_block_data.fragments.frag_offsets = NULL;
    }

    if ( bpq_block_data.fragments.frag_lenghts != NULL ){
    	free(bpq_block_data.fragments.frag_lenghts);
    	bpq_block_data.fragments.frag_lenghts = NULL;
    }

    return ret;
}

/*******************************************************************************
* main:
*
*******************************************************************************/
int
main(int argc, char** argv)
{
    dtn_endpoint_id_t local_eid;
    dtn_endpoint_id_t reply_eid;
    char local_eid_name[PATH_MAX];
    char reply_eid_name[PATH_MAX];
    char matching_filename[PATH_MAX];
    int count = 0;                                      //forever
    dtn_reg_id_t regid = DTN_REGID_NONE;
    dtn_timeval_t reg_expiry = 30;
    int reg_fail_action = DTN_REG_DEFER;
    char reg_fail_script[PATH_MAX]; 
    dtn_bundle_priority_t priority = COS_NORMAL;
    int delivery_options = 0;
    int verbose = 0;
    int err = 0;
    dtn_handle_t handle;

    parse_options(argc, argv,
        local_eid_name,
        reply_eid_name,
        matching_filename,
        &count,
        &regid,
        &reg_expiry,
        &reg_fail_action,
        reg_fail_script,
        &priority,
        &delivery_options,
        &verbose);

    validate_options(local_eid_name, reply_eid_name, matching_filename);

    // open the ipc handle
    if (verbose) fprintf(stdout, "opening connection to dtn router...\n");
    if (api_IP_set || api_port != 5010)
    {
        if((err = dtn_open_with_IP(api_IP,api_port,&handle)) != DTN_SUCCESS) 
        {
            fprintf(stderr, "fatal error opening dtn handle (%s:%d) : %s\n",
                    api_IP, api_port, dtn_strerror(err));
            exit(1);
        }
    } else if ((err = dtn_open(&handle)) != DTN_SUCCESS) {
        fprintf(stderr, "fatal error opening dtn handle: %s\n",
                dtn_strerror(err));
        exit(1);
    }
    if (verbose) fprintf(stdout, "opened connection to dtn router...\n");

    parse_eid(handle, &reply_eid, reply_eid_name, verbose);
    if (verbose) fprintf(stdout, "parsed reply_eid: %s\n", reply_eid.uri);

    // get dtn registration
    if (verbose) fprintf(stdout, "registering with dtn...\n");
    register_with_dtn(handle,
        &local_eid,
        local_eid_name,
        &regid,
        reg_expiry,
        reg_fail_action,
        reg_fail_script);
    if (verbose) fprintf(stdout, "registered with dtn, "
                                 "regid: %d local eid: %s\n",
                                 regid, local_eid.uri);

    // get to work
    // this fn will likely never exit so the handle won't be closed...
    receive_bpq(&handle,
        regid,
        &reply_eid,
        matching_filename,
        count,
        priority,
        delivery_options,
        verbose);

// UNREACHABLE CODE if count = 0 //////////////////////////////////////////////

    // close the ipc handle
    if (verbose) fprintf(stdout, "closing connection to dtn router...\n");
    dtn_close(handle);
    if (verbose) fprintf(stdout, "closed connection to dtn router...\n");

    return DTN_SUCCESS;

// UNREACHABLE CODE if count = 0 //////////////////////////////////////////////
}

