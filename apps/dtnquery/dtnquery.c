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

// todo: move these out to a header file
#define DTN_BPQ_LITERAL 1
#define DTN_BPQ_BASE64 2
#define DTN_BPQ_FILE 3

#define DTN_BPQ_SEND 1
#define DTN_BPQ_RECV 2
#define DTN_BPQ_SEND_RECV 3


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

int  api_IP_set = 0;
char api_IP[40] = "127.0.0.1";
int  api_port = 5010;


/*******************************************************************************
* usage:
* display cmd line options to user.
*******************************************************************************/
int
usage()
{
    fprintf(stderr, "usage: %s -s < src endpoint > -d < dest endpoint > "
                    "[opts]\n", progname);
    fprintf(stderr, "options:\n");
    fprintf(stderr, " -r  < eid > reply to endpoint\n");
    fprintf(stderr, " -f  < filename > response filename\n");
    fprintf(stderr, " -q  < query > query or matching file\n");
    fprintf(stderr, " -t  < literal | base64 | file > query type\n");
    fprintf(stderr, " -u  < exact > matching rule\n");
    fprintf(stderr, " -m  < send | receive | both > mode\n");
    fprintf(stderr, " -n  < count > number of bundles to recv\n");
    fprintf(stderr, " -o  < seconds > receiver timeout\n");
    fprintf(stderr, " -e  < seconds > bundle expiry time\n");
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
    fprintf(stderr, " -1  assert destination endpoint is a singleton\n");
    fprintf(stderr, " -N  assert destination endpoint is not a singleton\n");
    fprintf(stderr, " -W  set the do not fragment option\n");
    fprintf(stderr, " -A  set the option API IP addess (default localhost)\n");
    fprintf(stderr, " -B  set the option API port (default 5010)\n");
    fprintf(stderr, " -h  help\n");
    fprintf(stderr, " -v  verbose\n");

    return DTN_SUCCESS;
}

/*******************************************************************************
* parse matching file
* if matching file is passed in rather than cml line literal query,
* extract relevant information by reading the csv file and splitting
* it into tokens.
*
* Matching File Format:
* [ matching_rule, encoding, query, response_path, expiry ]
*******************************************************************************/
int
parse_matching_file(const char * filename,
    int * matching_rule,
    int * query_type,
    char * query,
    dtn_timeval_t * bundle_expiry,
    int verbose)
{
    FILE * file;
    char * token;
    char line[PATH_MAX];

    memset(line, 0, sizeof(char) * PATH_MAX);
    
    if (verbose) fprintf(stdout, "openning matching file...\n");
    file = fopen(filename, "r");
    if (file == NULL) {
        fprintf(stderr, "Error opening file: %s", filename);
        return 1;    
    }

    fgets(line, PATH_MAX, file);
    if (verbose) fprintf(stdout, "matching file %s contains [ %s ]\n", filename, line);

    fclose(file);
    if (verbose) fprintf(stdout, "closed matching file...\n");

    //matching rule
    token = strtok (line, ",");
    if (token == NULL)  return 1;
    *matching_rule = atoi(token);        

    //encoding
    token = strtok (NULL, ",");
    if (token == NULL)  return 1;
    *query_type = atoi(token);

    //query
    token = strtok (NULL, ",");
    if (token == NULL)  return 1;
    strncpy(query, token, PATH_MAX);

    //response path - to be ignored
    token = strtok (NULL, ",");
    if (token == NULL)  return 1;

    //expiry
    token = strtok (NULL, ",");
    if (token == NULL)  return 1;
    *bundle_expiry = atoi(token);

    //ensure there are no more tokens
    token = strtok (NULL, ",");
    if (token != NULL)  return 1;

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
    char * src_eid_name,                // s
    char * dest_eid_name,               // d
    char * reply_eid_name,              // r
    char * filename,                    // f
    char * query,                       // q
    int * query_type,                   // t
    int * matching_rule,                // u
    int * mode,                         // m
    int * count,                        // n
    dtn_timeval_t * timeout,            // o
    dtn_timeval_t * bundle_expiry,      // e
    dtn_reg_id_t  * regid,              // i
    dtn_timeval_t * reg_expiry,         // E
    int  * reg_fail_action,             // A
    char * reg_fail_script,             // S
    dtn_bundle_priority_t * priority,   // P
    int * delivery_options,             // D X F R c C 1 N W
    int * verbose)                      // v
{
    int c, done = 0;
    char matching_file[PATH_MAX];

    progname = argv[0];

    //initialize strings
    memset(src_eid_name,    0, sizeof(char) * PATH_MAX);
    memset(dest_eid_name,   0, sizeof(char) * PATH_MAX);
    memset(reply_eid_name,  0, sizeof(char) * PATH_MAX);
    memset(filename,        0, sizeof(char) * PATH_MAX);
    memset(query,           0, sizeof(char) * PATH_MAX);
    memset(matching_file,   0, sizeof(char) * PATH_MAX);
    memset(reg_fail_script, 0, sizeof(char) * PATH_MAX);

    while( !done )
    {
        c = getopt(argc, argv, "s:d:f:q:t:r:u:m:n:o:e:i:E:a:S:P:A:B:DXFcC1NWvhH");
        switch(c)
        {
        case 's':            
            strncpy(src_eid_name, optarg, PATH_MAX);
            break;
        case 'd':    
            strncpy(dest_eid_name, optarg, PATH_MAX);
            break;
        case 'r':
            strncpy(reply_eid_name, optarg, PATH_MAX);
            break;
        case 'f':    
            strncpy(filename, optarg, PATH_MAX);
            break;
        case 'q':            
            strncpy(query, optarg, PATH_MAX);
            break;
        case 't':
            if (!strcasecmp(optarg, "literal")) {
                *query_type = DTN_BPQ_LITERAL;
                break;
            } else if (!strcasecmp(optarg, "base64")) {
                *query_type = DTN_BPQ_BASE64;
                break;
            } else if (!strcasecmp(optarg, "file")) {
                *query_type = DTN_BPQ_FILE;
                break;
            } else {
                fprintf(stderr, "invalid query type '%s'\n", optarg);
                usage();
                exit(1);
            }
        case 'u':
            if (!strcasecmp(optarg, "exact")) {
                *matching_rule = BPQ_MATCHING_RULE_EXACT;
                break;
            } else {
                fprintf(stderr, "invalid query type '%s'\n", optarg);
                usage();
                exit(1);
            }
        case 'm':
            if (!strcasecmp(optarg, "send")) {
                *mode = DTN_BPQ_SEND;
                break;
            } else if (!strcasecmp(optarg, "receive")) {
                *mode = DTN_BPQ_RECV;
                break;
            } else if (!strcasecmp(optarg, "both")) {
                *mode = DTN_BPQ_SEND_RECV;
                break;
            } else {
                fprintf(stderr, "invalid mode '%s'\n", optarg);
                usage();
                exit(1);
            }
        case 'n':
            *count = atoi(optarg);
            break;
        case 'o':
            *timeout = atoi(optarg);
            break;
        case 'e':
            *bundle_expiry = atoi(optarg);  
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
            api_port = atoi(optarg);
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
    // now set matching file if required
    if (*query_type == DTN_BPQ_FILE) {
        strncpy(matching_file, query, PATH_MAX);
        memset(query, 0, sizeof(char) * PATH_MAX);

        int ret = parse_matching_file(matching_file, matching_rule,
                                      query_type, query, bundle_expiry, *verbose);
        if (ret != DTN_SUCCESS) {
            fprintf(stderr, "error parsing matching file, "
                            "see man page dtnmatch(1)\n");
            usage();
            exit(1);
        }
    }

    // if no reply-to eid set, use the src eid
    if (*reply_eid_name == 0)
        strncpy(reply_eid_name, src_eid_name, PATH_MAX);
    
    return DTN_SUCCESS;
}

/*******************************************************************************
* validate options:
* as there are different requirements depending on the mode,
* the validation will differ accordingly.
* returns success or exits on failure
*******************************************************************************/
int
validate_options(const char * src_eid_name,
    const char * dest_eid_name,
    const char * reply_eid_name,
    const char * query,
    int query_type,
    int matching_rule,
    int mode,
    dtn_timeval_t bundle_expiry)
{
//todo: add new options
#define REQUIRE(test, err_msg) \
    if(!(test)) { \
        fprintf(stderr, err_msg); \
        usage(); \
        exit(1); \
    }

    switch (mode)
    {
    case DTN_BPQ_SEND:  //requires src, dest, query
        REQUIRE(strlen(src_eid_name) > 0, "-s <src eid> required\n");
        REQUIRE(strlen(dest_eid_name) > 0, "-d <dest eid> required\n");
        REQUIRE(strlen(reply_eid_name) > 0, "-r <reply eid> required\n");
        REQUIRE(strlen(query) > 0, "-q <query> required\n");
        break;

    case DTN_BPQ_RECV:  //requires src
        REQUIRE(strlen(src_eid_name) > 0, "-s <src eid> required\n");
        REQUIRE(strlen(reply_eid_name) > 0, "-r <reply eid> required\n");
        break;

    case DTN_BPQ_SEND_RECV: //requires src, dest, query
        REQUIRE(strlen(src_eid_name) > 0, "-s <src eid> required\n");
        REQUIRE(strlen(dest_eid_name) > 0, "-d <dest eid> required\n");
        REQUIRE(strlen(reply_eid_name) > 0, "-r <reply eid> required\n");
        REQUIRE(strlen(query) > 0, "-q <query> required\n");
        break;
    default:  
        REQUIRE(mode == DTN_BPQ_SEND ||
                mode == DTN_BPQ_RECV ||
                mode == DTN_BPQ_SEND_RECV, "-m <mode> invalid mode\n")
    }
    REQUIRE(query_type == DTN_BPQ_LITERAL ||
            query_type == DTN_BPQ_BASE64  ||
            query_type == DTN_BPQ_FILE, "-t <query type> invalid type\n");

    REQUIRE(matching_rule == BPQ_MATCHING_RULE_EXACT,
         "-u <matching rule> invalid rule\n");
    REQUIRE(bundle_expiry > 0, "-e <expiry> must be a positive integer\n");

    return DTN_SUCCESS;
#undef REQUIRE
}

/*******************************************************************************
* register with dtn:
* 
*******************************************************************************/
int
register_with_dtn(dtn_handle_t handle,
    dtn_endpoint_id_t * src_eid,
    dtn_reg_id_t * regid,
    dtn_timeval_t reg_expiration,
    int reg_fail_action,
    char * reg_fail_script)
{
    int call_bind = 0;
    dtn_reg_info_t reginfo;

    // if no regid has been given we need to create a new registration
    if (*regid == DTN_REGID_NONE) {
        memset(&reginfo, 0, sizeof(dtn_reg_info_t));

        // create a new registration based on this eid
        dtn_copy_eid(&reginfo.endpoint, src_eid);
        reginfo.regid             = *regid;
        reginfo.expiration        = reg_expiration;
        reginfo.flags             = reg_fail_action;
        reginfo.script.script_val = reg_fail_script;
        reginfo.script.script_len = strlen(reg_fail_script) + 1;
    }

    // try to see if there is an existing registration that matches
    // the given endpoint, in which case we'll use that one.
    if (*regid == DTN_REGID_NONE) {
        fprintf(stdout, "### src_eid: %s, regid: %d  ###\n", src_eid->uri, *regid);
        if (dtn_find_registration(handle, src_eid, regid) != DTN_SUCCESS &&
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
* handle file transfer:
* copy the received payload file to the destination file
* @return 0 on success or -1 on error
*******************************************************************************/
int
handle_file_transfer(dtn_bundle_spec_t bundle_spec,
    dtn_bundle_payload_t payload,
    const char * destination,
    int verbose)
{

    int src_fd;
    int dest_fd;
    char block[BLOCKSIZE];
    ssize_t bytes_read;
    struct stat fileinfo;

    if ( (src_fd = open(payload.filename.filename_val, O_RDONLY)) < 0) {
        fprintf(stderr, "error opening payload file for reading '%s': %s\n",
                payload.filename.filename_val, strerror(errno));
        return -1;
    }

    if ( (dest_fd = creat(destination, 0644)) < 0) {
        fprintf(stderr, "error opening output file for writing '%s': %s\n",
                destination, strerror(errno));
        close(src_fd);
        return -1;
    }


    // Duplicate the file
    while ( (bytes_read = read(src_fd, block, BLOCKSIZE)) > 0 ) 
        write(dest_fd, block, bytes_read);

    close(src_fd);
    close(dest_fd);

    unlink(payload.filename.filename_val);

    if ( stat(destination, &fileinfo) == -1 ) {
        fprintf(stderr, "Unable to stat destination file '%s': %s\n",
                destination, strerror(errno));
        return -1;
    }

    if (verbose) printf("%d byte file from [%s]: transit time=%d ms, written to '%s'\n",
           (int)fileinfo.st_size, bundle_spec.source.uri, 0, destination);

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
    size_t buf_len,
    int verbose)
{
    int j=0, q_encoding_len, f_encoding_len, encoding_len;
    u_int i=0, k=0;
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
	 if ( (i + bpq->original_id.source_len) < buf_len ) {
		memcpy(&(buf[i]), bpq->original_id.source.uri, bpq->original_id.source_len);
		i += bpq->original_id.source_len;
	} else {
		fprintf (stderr, "Error encoding query value\n");
		return -1;
	}


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
    if ( (i + bpq->query.query_len) < buf_len ) {
    	memcpy(&(buf[i]), bpq->query.query_val, bpq->query.query_len);
    	i += bpq->query.query_len;
    } else {
    	fprintf (stderr, "Error encoding query value\n");
    	return -1;
    }


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
        fprintf (stdout, "\nbpq_to_char_array (buf_len:%zu, i:%d):\n",buf_len,i);
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
    size_t buf_len,
    dtn_bpq_extension_block_data_t * bpq,
    int verbose)
{
    u_int i=0, j=0;
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
    if (q_decoding_len == -1) {
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
    if (q_decoding_len == -1) {
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
    if (f_decoding_len == -1) {
        fprintf (stderr, "Error decoding number of fragments\n");
        return -1;
    }
    bpq->fragments.num_frag_returned = (u_int) uval64;
    i += f_decoding_len;

    for (j=0; i<buf_len && j<bpq->fragments.num_frag_returned; ++j) {

        // fragment offsets     SDNV
        decoding_len = sdnv_decode (&(buf[i]), buf_len - i, &uval64);
        if (decoding_len == -1) {
            fprintf (stderr, "Error decoding fragment[%d] offset\n", j);
            return -1;
        }
        bpq->fragments.frag_offsets[j] = (u_int) uval64;
        i += decoding_len;

        // fragment lengths     SDNV
        decoding_len = sdnv_decode (&(buf[i]), buf_len - i, &uval64);
        if (decoding_len == -1) {
            fprintf (stderr, "Error decoding fragment[%d] length\n", j);
            return -1;
        }
        bpq->fragments.frag_lenghts[j] = (u_int) uval64;
        i += decoding_len;
    }

    if (i != buf_len)
        return -1;

    if (verbose) {
        fprintf (stdout, "\nchar_array_to_bpq (buf_len:%zu, i:%d):\n",buf_len, i);
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
* send bpq:
* given a registration handle, build a bundle with
* a BPQ extension block and an empty payload and 
* send it to the destination.
*******************************************************************************/
int
send_bpq(dtn_handle_t handle,
    dtn_reg_id_t regid,
    const dtn_endpoint_id_t * src_eid,
    const dtn_endpoint_id_t * dest_eid,
    const dtn_endpoint_id_t * reply_eid,
    char * query,
    int matching_rule,
    int bundle_expiry,
    dtn_bundle_priority_t  priority,
    int delivery_options,
    int verbose)
{
    int  ret = 0;
    char buf [PATH_MAX];
    size_t buf_len = 0;
    dtn_bundle_id_t                 bundle_id;
    dtn_bundle_spec_t               bundle_spec;
    dtn_extension_block_t           bpq_block;
    dtn_bpq_extension_block_data_t  bpq_block_data;
    dtn_bundle_payload_t            payload;

    memset(buf,             0, PATH_MAX);
    memset(&bundle_spec,    0, sizeof(dtn_bundle_spec_t));
    memset(&bpq_block,      0, sizeof(dtn_extension_block_t));
    memset(&bpq_block_data, 0, sizeof(dtn_bpq_extension_block_data_t));
    memset(&payload,        0, sizeof(dtn_bundle_payload_t));
    
    // set the bpq block data
    bpq_block_data.kind = BPQ_BLOCK_KIND_QUERY;
    bpq_block_data.matching_rule = matching_rule;

    bpq_block_data.original_id.creation_ts.secs = 0;
    bpq_block_data.original_id.creation_ts.seqno = 0;
    bpq_block_data.original_id.source = *src_eid;
    bpq_block_data.original_id.source_len = strlen(src_eid->uri);

    bpq_block_data.query.query_len = strlen(query) + 1;     // include the null char at the end
    bpq_block_data.query.query_val = query; 
    bpq_block_data.fragments.num_frag_returned = 0;
    bpq_block_data.fragments.frag_offsets = NULL;
    bpq_block_data.fragments.frag_lenghts = NULL;

    buf_len = bpq_to_char_array(&bpq_block_data, buf, PATH_MAX, verbose);

    // set the bpq block
    bpq_block.type = DTN_BPQ_BLOCK_TYPE;
    bpq_block.flags = BLOCK_FLAG_REPLICATE;
    bpq_block.data.data_len = buf_len;
    bpq_block.data.data_val = buf;
    
    // set the payload (empty)
    dtn_set_payload(&payload, DTN_PAYLOAD_MEM, NULL, 0);    

    // set the bundle spec eids
    if (verbose) fprintf(stdout, "Source: %s\n", src_eid->uri);
    if (verbose) fprintf(stdout, "Destination: %s\n", dest_eid->uri);
    if (verbose) fprintf(stdout, "Reply-To: %s\n", reply_eid->uri);
    bundle_spec.source = *src_eid;
    bundle_spec.dest = *dest_eid;
    bundle_spec.replyto = *reply_eid;

    // set the bundle spec dtn options
    bundle_spec.expiration = bundle_expiry;
    bundle_spec.dopts = delivery_options;
    bundle_spec.priority = priority;

    // set the bundle extension
    bundle_spec.blocks.blocks_len = 1;
    bundle_spec.blocks.blocks_val = &bpq_block;

    // send the bundle, bpq extension and empty payload
    if (verbose) fprintf(stdout, "Sending bundle to: %s\n", dest_eid->uri);
    ret = dtn_send(handle, regid, &bundle_spec, &payload, &bundle_id);
    if (ret != DTN_SUCCESS) {
        fprintf(stderr, "error sending bundle: %d (%s)\n",
                    ret, dtn_strerror(dtn_errno(handle)));
    } else if (verbose) {
        fprintf(stdout, "bundle sent successfully: id %s,%"PRIu64".%"PRIu64"\n",
                    bundle_id.source.uri,
                    bundle_id.creation_ts.secs,
                    bundle_id.creation_ts.seqno); 
    }
    return ret;   
}

/*******************************************************************************
* recv bpq:
* given a registration handle, listen for count bundles.
* if count is 0 - listen forever
* as new bundles arrive save the payload as filename
* if filename is NULL, use the query value as the filename
*******************************************************************************/
int
recv_bpq(dtn_handle_t handle,
    dtn_timeval_t timeout,
    char * filename,
    int count,
    int verbose)
{
    int ret = 0, err = 0, num_blocks, i, j;
    int has_bpq_block = 0;
    dtn_bundle_spec_t               bundle_spec;
    dtn_extension_block_t*          bpq_blocks;
    dtn_bpq_extension_block_data_t  bpq_block_data;
    dtn_bundle_payload_t            payload;

    for(j = 0; (count == 0) || (j < count); ++j) {
        memset(&bundle_spec, 0, sizeof(bundle_spec));
        memset(&bpq_block_data, 0, sizeof(dtn_bpq_extension_block_data_t));
        memset(&payload, 0, sizeof(payload));
        err = 0;

        // recv the bpq bundle
        if (verbose) fprintf(stdout, "blocking waiting for dtn_recv\n");
        ret = dtn_recv(handle, &bundle_spec, DTN_PAYLOAD_FILE, &payload, timeout);
        if (ret != DTN_SUCCESS) {
            fprintf(stderr, "error receiving bundle: %d (%s)\n",
                             ret, dtn_strerror(dtn_errno(handle)));
            err = 1;
            continue;
        } else if (verbose) {
            fprintf(stdout, "bundle num %d received successfully: id %s,%"PRIu64".%"PRIu64"\n",
                             j+1,
                             bundle_spec.source.uri,
                             bundle_spec.creation_ts.secs,
                             bundle_spec.creation_ts.seqno);

            fprintf(stdout, "Source: %s\n", bundle_spec.source.uri);
            fprintf(stdout, "Destination: %s\n", bundle_spec.dest.uri);
            fprintf(stdout, "Reply-To: %s\n", bundle_spec.replyto.uri);
        }

        // extract the bpq
        num_blocks = bundle_spec.blocks.blocks_len;
        bpq_blocks = bundle_spec.blocks.blocks_val;

        for (i = 0; i < num_blocks; ++i) {
            if (bpq_blocks[i].type == DTN_BPQ_BLOCK_TYPE) {
                has_bpq_block = 1;

                if (verbose) fprintf(stdout, "bundle contains a "
                                             "BPQ extension block\n");

                if ( bpq_blocks[i].data.data_len <= 0 || 
                     bpq_blocks[i].data.data_val == NULL) {
                    fprintf(stderr, "error decoding query bundle: %d\n", ret);
                    err = 1;
                    break;
                }
                
                ret = char_array_to_bpq((u_char*)bpq_blocks[i].data.data_val,
                                        bpq_blocks[i].data.data_len,
                                        &bpq_block_data,
                                        verbose);
                if (ret != DTN_SUCCESS) {
                    fprintf(stderr, "error decoding query bundle: %d\n", ret);
                    err = 1;
                    break;
                }

                if (verbose) fprintf(stdout, "BPQ query(%s)\n", bpq_block_data.query.query_val);
                if (filename == NULL)
                    strncpy(filename, bpq_block_data.query.query_val, PATH_MAX);
    
                break;
            }
        }

        if(!has_bpq_block) {
            fprintf(stderr, "no bpq block found in bundle:\n");
            for (i = 0; i < num_blocks; ++i)
                fprintf(stderr, "\tblock[%d] : type = %d\n", i, bpq_blocks[i].type);

            continue;
        }

        if(err)
            continue;

        // handle the payload file
        ret = handle_file_transfer(bundle_spec, payload, filename, verbose);
        if (ret != DTN_SUCCESS) {
            fprintf(stderr, "error handling file transfer: %d\n", ret);
        } else if (verbose) {
            fprintf(stdout, "sucessfully handled file transfer\n");        
        }
    
        dtn_free_payload(&payload);  
    }
    return ret;
}

/*******************************************************************************
* main:
*******************************************************************************/
int
main(int argc, char** argv)
{
    dtn_endpoint_id_t reg_eid;
    dtn_endpoint_id_t src_eid;
    dtn_endpoint_id_t dest_eid;
    dtn_endpoint_id_t reply_eid;
    char reg_eid_name[PATH_MAX];
    char src_eid_name[PATH_MAX];
    char dest_eid_name[PATH_MAX];
    char reply_eid_name[PATH_MAX];
    char filename[PATH_MAX];
    char query[PATH_MAX];
    int query_type = DTN_BPQ_LITERAL;
    int matching_rule = BPQ_MATCHING_RULE_EXACT;
    int mode = DTN_BPQ_SEND_RECV;
    int count = 1;
    dtn_timeval_t timeout = DTN_TIMEOUT_INF;    //forever
    dtn_timeval_t bundle_expiry = 3600;         //one hour
    dtn_reg_id_t regid = DTN_REGID_NONE;
    dtn_timeval_t reg_expiry = 30;
    int reg_fail_action = DTN_REG_DEFER;
    char reg_fail_script[PATH_MAX];
    dtn_bundle_priority_t priority = COS_NORMAL; 
    int delivery_options = 0;               
    int verbose = 0;
    int err = 0;
    dtn_handle_t handle;

    memset( reg_eid_name,   0, sizeof(char) * PATH_MAX );
    memset( src_eid_name,   0, sizeof(char) * PATH_MAX );
    memset( dest_eid_name,  0, sizeof(char) * PATH_MAX );
    memset( reply_eid_name,  0, sizeof(char) * PATH_MAX );
    memset( filename,       0, sizeof(char) * PATH_MAX );
    memset( query,          0, sizeof(char) * PATH_MAX );

    parse_options(argc, argv,
        src_eid_name,
        dest_eid_name,
        reply_eid_name,
        filename,
        query,
        &query_type,
        &matching_rule,
        &mode,
        &count,
        &timeout,
        &bundle_expiry,
        &regid,
        &reg_expiry,
        &reg_fail_action,
        reg_fail_script,
        &priority,
        &delivery_options,
        &verbose);

    if (mode == DTN_BPQ_SEND) {
        snprintf(&(reg_eid_name[0]), PATH_MAX, "%s-send", src_eid_name);
    } else {
        snprintf(&(reg_eid_name[0]), PATH_MAX, "%s", src_eid_name);
    }

    validate_options(src_eid_name,
        dest_eid_name,
        reply_eid_name,
        query,
        query_type,
        matching_rule,
        mode,
        bundle_expiry);

    // open the ipc handle
    if (verbose) printf("opening connection to dtn router...\n");
    if (api_IP_set || api_port != 5010)
    {
        if ((err = dtn_open_with_IP(api_IP, api_port, &handle)) != DTN_SUCCESS) {
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

    if (mode != DTN_BPQ_SEND) {
        if (count == 0)
            fprintf(stdout, "dtnquery will loop forever receiving bundles\n");
        else 
            fprintf(stdout, "dtnquery will exit after receiving %d bundle(s)\n", count);
    }

    // parse eids
    parse_eid(handle, &reg_eid, reg_eid_name, verbose);
    parse_eid(handle, &src_eid, src_eid_name, verbose);
    parse_eid(handle, &dest_eid, dest_eid_name, verbose);
    parse_eid(handle, &reply_eid, reply_eid_name, verbose);
    if (verbose) fprintf(stdout, "parsed reg_eid: %s\n", reg_eid.uri);
    if (verbose) fprintf(stdout, "parsed src_eid: %s\n", src_eid.uri);
    if (verbose) fprintf(stdout, "parsed dest_eid: %s\n", dest_eid.uri);
    if (verbose) fprintf(stdout, "parsed reply_eid: %s\n", reply_eid.uri);
    if (verbose) fprintf(stdout, "regid: %d\n", regid);

    // get dtn registration
    if (verbose) fprintf(stdout, "registering with dtn...\n");
    register_with_dtn(handle,
        &reg_eid,
        &regid,
        reg_expiry,
        reg_fail_action,
        reg_fail_script);
    if (verbose) fprintf(stdout, "registered with dtn, "
                                 "regid: %d local eid: %s\n",
                                 regid, reg_eid.uri);
    
    //get to work
    switch (mode)
    {
#define TRY(fn, err_msg) \
    if (fn != DTN_SUCCESS) { \
        fprintf(stderr, err_msg); \
        dtn_close(handle); \
        exit(1); \
    }
    case DTN_BPQ_SEND:
        TRY( send_bpq(handle, regid, &src_eid, &dest_eid, &reply_eid, query,
                      matching_rule, bundle_expiry, priority,
                      delivery_options, verbose), "error sending query\n" );
        break;

    case DTN_BPQ_RECV:
        TRY( recv_bpq(handle, timeout, filename, count, verbose), 
             "error receiving query\n" );
        break;

    case DTN_BPQ_SEND_RECV:
        TRY( send_bpq(handle, regid, &src_eid, &dest_eid, &reply_eid, query,
                      matching_rule, bundle_expiry, priority,
                      delivery_options, verbose), "error sending query\n" );

        TRY( recv_bpq(handle, timeout, filename, count, verbose), 
             "error receiving query\n" );
        break;

    default:
        fprintf(stderr, "invalid mode '%d'\n", mode);
        dtn_close(handle);
        exit(1);
#undef TRY
    }

    // close the ipc handle
    if (verbose) fprintf(stdout, "closing connection to dtn router...\n");
    dtn_close(handle);
    if (verbose) fprintf(stdout, "closed connection to dtn router...\n");

    return DTN_SUCCESS;
}

