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



/* ----------------------------------------
 *         DTNperf 2.8 - CLIENT
 *
 *             developed by
 * 
 * Piero Cornice - piero.cornice(at)gmail.com
 * Marco Livini - marco.livini(at)gmail.com
 * Leo Iannacone - liannacone(at)arces.unibo.it
 *
 * DEIS - Dipartimento di Elettronica, Informatica e Sistemistica
 * Universita' di Bologna
 * Italy
 * ----------------------------------------
 */



#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

/* pthread_yield() is not standard, 
   so use sched_yield if necessary */
#ifndef HAVE_PTHREAD_YIELD
#   ifdef HAVE_SCHED_YIELD
#       include <sched.h>
#       define pthread_yield    sched_yield
#   else
#       define pthread_yield()  usleep(1);
#   endif
#else
#   ifndef __USE_GNU
#      define __USE_GNU
#      include <pthread.h>
#      undef __USE_GNU
#   else
#      include <pthread.h>
#   endif
#endif


#include "includes.h"
#include "utils.h"
#include "bundle_tools.h"
#include <signal.h>



// max payload (in bytes) if bundles are stored into memory
#define MAX_MEM_PAYLOAD 50000

// illegal number of bytes for the bundle payload
#define ILLEGAL_PAYLOAD 0

// default value (in bytes) for bundle payload
#define DEFAULT_PAYLOAD 50000



/* ---------------------------------------------
 * Values inside [square brackets] are defaults
 * --------------------------------------------- */

// Daemon connection
int api_IP_set = 0;
char * api_IP = "127.0.0.1";
short api_port = 5010;


// global options

int verbose = 0;    // if set to 1, execution becomes verbose (-v option) [0]
int debug = 0;    // if set to 1, many debug messages are shown [0]
int debug_level = 0;
int csv_out = 0;    // if set to 1, a Comma-Separated-Values output is shown [0]
char* csv_log_filename = NULL;
FILE* csv_log_file = NULL;
int create_log = 0;
char* log_filename = NULL; // name of log destination file;
FILE* log_file = NULL;



typedef struct
{
	int expiration;				// expiration time (sec) [3600]
	int delivery_receipts;    	// request delivery receipts [1]
	int forwarding_receipts;	// request per hop departure [0]
	int custody_transfer;		// request custody transfer [0]
	int custody_receipts;		// request per custodian receipts [0]
	int receive_receipts;		// request per hop arrival receipt [0]
	int wait_for_report;		// wait for bundle status reports [1]
	int disable_fragmentation;	// disable bundle fragmentation [0]
	dtn_bundle_priority_t priority; // bundle priority [COS_NORMAL]
}
dtn_options_t;


typedef struct
{
	char op_mode ;    		// operative mode (t = time_mode, d = data_mode) [d]
	long data_qty;			// data to be transmitted (bytes) [0]
	char * n_arg;			// arguments of -n option
	char * p_arg;			// arguments of -p option
	int use_file;			// if set to 1, a file is used instead of memory [1]
	int transfer_file;		// if set to 1, the transfer involved a real file [0]
	char data_unit;			// B = bytes, K = kilobytes, M = megabytes [M]
	int transmission_time;	// seconds of transmission [0]
	int window;				// trasmission window [1]
	int wait_before_exit;
	int slide_on_custody;	// flag sliding window on custody receipts [0] 
	dtn_reg_id_t regid;   	// registration ID (-i option) [DTN_REGID_NONE]
	long bundle_payload;  	// quantity of data (in bytes) to send (-p option) [DEFAULT_PAYLOAD]
	dtn_bundle_payload_location_t payload_type;	// the type of data source for the bundle [DTN_PAYLOAD_FILE]
}
dtnperf_options_t;


typedef struct
{
	dtnperf_options_t *dtnperf_opt;
	dtn_options_t *dtn_opt;
}
global_options_t;



// specified options for bundle tuples
char * arg_replyto = NULL; // replyto_tuple
char * arg_source = NULL; // source_tuple
char * arg_dest = NULL; // destination_tuple



// Data-Mode variables
int fd ;    // file descriptor, used with -f option
int data_written = 0;    // data written on the file
int data_read = 0;    // data read from the file
char * file_name_src = "/var/dtn/dtnperf/dtnbuffer.snd";    // name of the SOURCE file to use
char * filename = NULL; // name of the file to transfer
char * real_filename = NULL; // absolute path of the file to transfer
int real_filename_fd;	// file decriptor of the file to tranfer



/* -------------------------------
 *       function interfaces
 * ------------------------------- */

void parse_options(int, char**, dtnperf_options_t *, dtn_options_t *);
void print_usage(char* progname);

void check_options(dtnperf_options_t *perf_opt, dtn_options_t *dtn_opt);
void show_options(dtnperf_options_t *perf_opt, dtn_options_t *dtn_opt);

void init_dtnperf_options(dtnperf_options_t *);
void init_dtn_options(dtn_options_t*);
void set_dtn_options(dtn_bundle_spec_t *, dtn_options_t*);

// Thread functions
void* send_bundle(void *opt);
void* receive_ack(void *opt);

//CTRL+C handling
void sigint(int sig);

// Utility
int file_exists(const char * filename);
long double calc_exec_time ( long long unsigned sec, long long unsigned sec_no );
long long unsigned calc_epoch_time ( long long unsigned dtn_time );
long double calc_timestamp ( long long unsigned sec ) ;

/* -----------------------
 *  variables declaration
 * ----------------------- */
int ret;                        // result of DTN-registration
struct timeval start, end,
			p_start, p_end, now; // time-calculation variables

send_information_t* send_info;

int i, j;                       // loop-control variables

int n_bundles = 0;              // number of bundles needed (Data-Mode)

int first_bundle_ever = 0;      // Check if the first bundle is sent

// DTN variables
dtn_handle_t handle;
dtn_reg_info_t reginfo;
dtn_bundle_spec_t bundle_spec;
dtn_bundle_spec_t reply_spec;
dtn_bundle_id_t bundle_id;
dtn_bundle_payload_t send_payload;
dtn_bundle_payload_t reply_payload;
char demux[64];


// buffer settings
char* buffer = NULL;            // buffer containing data to be transmitted
int bufferLen;                  // lenght of buffer
int sent_bundles;               // number of bundles sent in Time-Mode

int bundles_ready;
int orphan_acks = 0;
int close_ack_receiver = 0;

pthread_t sender;
pthread_t ack_receiver;
pthread_mutex_t mutexdata;
pthread_cond_t cond_sender;
pthread_cond_t cond_ackreceiver;



/* -----------------------
 *        M A I N
 * ----------------------- */
int main(int argc, char *argv[])
{
	intptr_t pthread_status;

	dtnperf_options_t dtnperf_options;
	dtn_options_t dtn_options;


	// Init options
	init_dtnperf_options(&dtnperf_options);
	init_dtn_options(&dtn_options);


	// Parse and check command line options
	parse_options(argc, argv, &dtnperf_options, &dtn_options);

	if ((debug) && (debug_level > 0))
		printf("[debug] parsed command-line options\n");

	if ((debug) && (debug_level > 0))
		printf("[debug] checking command-line option...");

	check_options(&dtnperf_options, &dtn_options);

	if ((debug) && (debug_level > 0))
		printf(" done\n");

	if (debug)
		show_options(&dtnperf_options, &dtn_options);



	// Create a new log file
	if (create_log)
	{
		if ((log_file = fopen(log_filename, "w")) == NULL)
		{
			fprintf(stderr, "fatal error opening log file\n");
			exit(1);
		}
	}

	if (csv_out)
	{
		if ((csv_log_file = fopen(csv_log_filename, "w")) == NULL)
		{
			fprintf(stderr, "fatal error opening log file\n");
			exit(1);
		}
	}

	if (dtnperf_options.transfer_file)
	{
		if ((real_filename_fd = open(real_filename, O_RDONLY)) < 0)
		{
			fprintf(stderr, "fatal error opening file %s\n", filename);
			exit(1);
		}
	}


	// Connect to DTN Daemon
	if ((debug) && (debug_level > 0))
		printf("[debug] opening connection to local DTN daemon...");

    int err = 0;
    if (api_IP_set) err = dtn_open_with_IP(api_IP,api_port,&handle);
    else err = dtn_open(&handle);


	if (err != DTN_SUCCESS)
	{
		fprintf(stderr, "fatal error opening dtn handle: %s\n", dtn_strerror(err));
		if (create_log)
			fprintf(log_file, "fatal error opening dtn handle: %s\n", dtn_strerror(err));
		exit(1);
	}

	if ((debug) && (debug_level > 0))
		printf("done\n");



	/* -----------------------------------------------------
	 *   initialize and parse bundle src/dest/replyto EIDs
	 * ----------------------------------------------------- */

	memset(&bundle_spec, 0, sizeof(bundle_spec));


	// SOURCE is local EID + demux string (with optional file path)
	sprintf(demux, "/dtnperf:/src_%d",getpid());
	dtn_build_local_eid(handle, &bundle_spec.source, demux);

	if (debug)
		printf("\nSource     : %s\n", bundle_spec.source.uri);

	if (create_log)
		fprintf(log_file, "\nSource     : %s\n", bundle_spec.source.uri);


	// DEST host is specified at runtime, demux is hardcoded
	sprintf(demux, "/dtnperf:/dest");
	strcat(arg_dest, demux);

	if (verbose)
		fprintf(stdout, "%s (local)\n", arg_dest);

	if (parse_eid(handle, &bundle_spec.dest, arg_dest) == NULL)
	{
		fprintf(stderr, "fatal error parsing dtn EID: invalid eid string '%s'\n", arg_dest);
		exit(1);
	}

	if (debug)
		printf("Destination: %s\n", bundle_spec.dest.uri);

	if (create_log)
		fprintf(log_file, "Destination: %s\n", bundle_spec.dest.uri);


	// REPLY-TO (if none specified, same as the source)
	if (arg_replyto == NULL)
	{
		if ((debug) && (debug_level > 0))
			printf("[debug] setting replyto = source...");

		dtn_copy_eid(&bundle_spec.replyto, &bundle_spec.source);

		if ((debug) && (debug_level > 0))
			printf(" done\n");
	}
	else
	{
		sprintf(demux, "/dtnperf:/src_%d", getpid());
		strcat(arg_replyto, demux);
		parse_eid(handle, &bundle_spec.dest, arg_replyto);
	}

	if (debug)
		printf("Reply-to   : %s\n\n", bundle_spec.replyto.uri);

	if (create_log)
		fprintf(log_file, "Reply-to   : %s\n\n", bundle_spec.replyto.uri);



	/* ------------------------
	 * set DTN options
	 * ------------------------ */

	if ((debug) && (debug_level > 0))
		printf("[debug] setting the DTN options: ");

	if (create_log)
		fprintf(log_file, " DTN options: ");

	set_dtn_options(&bundle_spec, &dtn_options);

	if ((debug) && (debug_level > 0))
		printf("option(s) set\n");


	/* ----------------------------------------------
	 * create a new registration based on the source
	 * ---------------------------------------------- */

	memset(&reginfo, 0, sizeof(reginfo));


	if ((debug) && (debug_level > 0))
		printf("[debug] copying bundle_spec.replyto to reginfo.endpoint...");

	dtn_copy_eid(&reginfo.endpoint, &bundle_spec.replyto);

	if ((debug) && (debug_level > 0))
		printf(" done\n");

	if ((debug) && (debug_level > 0))
		printf("[debug] setting up reginfo...");

	reginfo.flags = DTN_REG_DEFER;
	reginfo.regid = dtnperf_options.regid;
	reginfo.expiration = 30;

	if ((debug) && (debug_level > 0))
		printf(" done\n");

	if ((debug) && (debug_level > 0))
		printf("[debug] registering to local daemon...");

	if ((ret = dtn_register(handle, &reginfo, &dtnperf_options.regid)) != 0)
	{
		fprintf(stderr, "error creating registration: %d (%s)\n", ret, dtn_strerror(dtn_errno(handle)));
		if (create_log)
			fprintf(log_file, "error creating registration: %d (%s)\n", ret, dtn_strerror(dtn_errno(handle)));
		exit(1);
	}

	if ((debug) && (debug_level > 0))
		printf(" done: regid 0x%x\n", dtnperf_options.regid);

	if (create_log)
		fprintf(log_file, " regid 0x%x\n", dtnperf_options.regid);


	// if bundle_payload > MAX_MEM_PAYLOAD, then transfer a file
	if (dtnperf_options.bundle_payload > MAX_MEM_PAYLOAD)
		dtnperf_options.use_file = 1;
	else
		dtnperf_options.use_file = 0;

	if (csv_out)
		fprintf(csv_log_file, "Rx_TIME,Tx_TIMESTAMP,SEQ_NO,STATUS,SENDER,ID,IS FRAGMENT,FRAGMENT OFFSET,RECEIVER\n");


	/* ------------------------------------------------------------------------------
	 * select the operative-mode (between Time_Mode and Data_Mode)
	 * ------------------------------------------------------------------------------ */
	
	
	if (dtnperf_options.op_mode == 't')	// Time mode
	{

		if (debug)
			printf("Working in Time_Mode\n");

		if (create_log)
			fprintf(log_file, "Working in Time_Mode\n");

		if (debug)
			printf("requested %d second(s) of transmission\n", dtnperf_options.transmission_time);

		if (create_log)
			fprintf(log_file, "requested %d second(s) of transmission\n", dtnperf_options.transmission_time);

		if ((debug) && (debug_level > 0))
			printf("[debug] bundle_payload %s %d bytes\n", dtnperf_options.use_file ? ">=" : "<", MAX_MEM_PAYLOAD);

		if (create_log)
			fprintf(log_file, " bundle_payload %s %d bytes\n", dtnperf_options.use_file ? ">=" : "<", MAX_MEM_PAYLOAD);

		if (debug)
			printf(" transmitting data %s\n", dtnperf_options.use_file ? "using a file" : "using memory");

		if (create_log)
			fprintf(log_file, " transmitting data %s\n", dtnperf_options.use_file ? "using a file" : "using memory");


		dtnperf_options.data_qty = 0;
		sent_bundles = 0;

		// Init buffer
		buffer = malloc(dtnperf_options.bundle_payload * sizeof(char));

		if ((debug) && (debug_level > 0))
			printf("[debug] initialize the buffer with a pattern... ");

		pattern(buffer, dtnperf_options.bundle_payload);
		bufferLen = strlen(buffer);

		if ((debug) && (debug_level > 0))
			printf("done\n[debug] bufferLen = %d\n", bufferLen);


		if (dtnperf_options.use_file)
		{
			// create the file
			if ((debug) && (debug_level > 0))
				printf("[debug] creating file %s...", file_name_src);

			fd = open(file_name_src, O_CREAT | O_TRUNC | O_WRONLY | O_APPEND, 0666);

			if (fd < 0)
			{
				fprintf(stderr, "ERROR: couldn't create file %s [fd = %d].\n \b Maybe you don't have permissions\n", file_name_src, fd);

				if (create_log)
					fprintf(log_file, "ERROR: couldn't create file %s [fd = %d].\n \b Maybe you don't have permissions\n", file_name_src, fd);

				exit(2);
			}

			if ((debug) && (debug_level > 0))
				printf(" done\n");


			// Fill in the file with a pattern
			if ((debug) && (debug_level > 0))
				printf("[debug] filling the file (%s) with the pattern...", file_name_src);

			data_written += write(fd, buffer, bufferLen);

			if ((debug) && (debug_level > 0))
				printf(" done. Written %d bytes\n", data_written);

			// Close the file
			if ((debug) && (debug_level > 0))
				printf("[debug] closing file (%s)...", file_name_src);

			close(fd);

			if ((debug) && (debug_level > 0))
				printf(" done\n");
		}

		bundles_ready = dtnperf_options.window;

		// Create the array for the bundle send info
		if ((debug) && (debug_level > 0))
			printf("[debug] creating structure for sending information...");

		if (dtnperf_options.slide_on_custody==1)
		{
			send_info = (send_information_t*) malloc((dtnperf_options.window+1000) * sizeof(send_information_t));
			init_info(send_info, dtnperf_options.window+1000);
		}
		else
		{
			send_info = (send_information_t*) malloc(dtnperf_options.window * sizeof(send_information_t));
			init_info(send_info, dtnperf_options.window);
		}

		if ((debug) && (debug_level > 0))
			printf(" done\n");


		// Fill the payload
		memset(&send_payload, 0, sizeof(send_payload));

		if ((debug) && (debug_level > 0))
			printf("[debug] filling payload...");

		if (dtnperf_options.use_file)
			dtn_set_payload(&send_payload, DTN_PAYLOAD_FILE, file_name_src, strlen(file_name_src));
		else
			dtn_set_payload(&send_payload, DTN_PAYLOAD_MEM, buffer, bufferLen);

		if ((debug) && (debug_level > 0))
			printf(" done\n");

		//CTRL+C handling
		signal(SIGINT, &sigint);

		// Run threads
		pthread_cond_init(&cond_sender, NULL);
		pthread_cond_init(&cond_ackreceiver, NULL);
		pthread_mutex_init (&mutexdata, NULL);

		global_options_t arg;
		arg.dtnperf_opt = &dtnperf_options;
		arg.dtn_opt = &dtn_options;

		pthread_create(&sender, NULL, send_bundle, (void*)&arg);
		pthread_create(&ack_receiver, NULL, receive_ack, (void*)&arg);

		pthread_join(ack_receiver, (void**)&pthread_status);
		pthread_join(sender, (void**)&pthread_status);

		pthread_mutex_destroy(&mutexdata);

		if ((debug) && (debug_level > 0))
			printf("[debug] out from loop\n");

		free((void*)buffer);

		// Get the TOTAL end time
		if ((debug) && (debug_level > 0))
			printf("[debug] getting total end-time...");

		gettimeofday(&end, NULL);

		if ((debug) && (debug_level > 0))
			printf(" end.tv_sec = %u sec\n", (u_int)end.tv_sec);


		// Show the report
		printf("%d bundles sent, each with a %ld bytes payload\n", sent_bundles, dtnperf_options.bundle_payload);

		show_report(reply_payload.buf.buf_len,
		            reply_spec.source.uri,
		            start,
		            end,
		            dtnperf_options.data_qty,
		            NULL);


		if (create_log)
		{
			fprintf(log_file, "%d bundles sent, each with a %ld bytes payload\n", sent_bundles, dtnperf_options.bundle_payload);

			show_report(reply_payload.buf.buf_len,
			            reply_spec.source.uri,
			            start,
			            end,
			            dtnperf_options.data_qty,
			            log_file);
		}

		if (csv_out)
		{
			csv_time_report(sent_bundles, dtnperf_options.bundle_payload, start, end, csv_log_file);
		}
	}
	// End of Time Mode


	else if (dtnperf_options.op_mode == 'd')	// Data mode
	{
		if (debug)
			printf("Working in Data_Mode\n");

		// Initialize the buffer
		if (!dtnperf_options.transfer_file)
		{

			if ((debug) && (debug_level > 0))
				printf("[debug] initializing buffer...");

			if (!dtnperf_options.use_file)
			{
				buffer = malloc( (dtnperf_options.data_qty < dtnperf_options.bundle_payload) ?
				                 dtnperf_options.data_qty :
				                 dtnperf_options.bundle_payload );

				memset(buffer, 0, (dtnperf_options.data_qty < dtnperf_options.bundle_payload) ?
				       dtnperf_options.data_qty : dtnperf_options.bundle_payload );

				pattern(buffer, (dtnperf_options.data_qty < dtnperf_options.bundle_payload) ?
				        dtnperf_options.data_qty : dtnperf_options.bundle_payload );
			}
			else
			{
				buffer = malloc( (dtnperf_options.data_qty < dtnperf_options.bundle_payload) ?
				                 dtnperf_options.data_qty : dtnperf_options.bundle_payload );

				memset(buffer, 0, (dtnperf_options.data_qty < dtnperf_options.bundle_payload) ?
				       dtnperf_options.data_qty : dtnperf_options.bundle_payload );

				pattern(buffer, (dtnperf_options.data_qty < dtnperf_options.bundle_payload) ?
				        dtnperf_options.data_qty : dtnperf_options.bundle_payload );
			}

			bufferLen = strlen(buffer);

			if ((debug) && (debug_level > 0) && (!dtnperf_options.transfer_file))
				printf(" done. bufferLen = %d (should equal %s)\n",
				       bufferLen, dtnperf_options.use_file ? "data_qty" : "bundle_payload");

			if (dtnperf_options.use_file)
			{
				// Create the file
				if ((debug) && (debug_level > 0))
					printf("[debug] creating file %s...", file_name_src);

				fd = open(file_name_src, O_CREAT | O_TRUNC | O_WRONLY | O_APPEND, 0666);

				if (fd < 0)
				{
					fprintf(stderr, "ERROR: couldn't create file [fd = %d]. Maybe you don't have permissions\n", fd);

					if (create_log)
						fprintf(log_file, "ERROR: couldn't create file [fd = %d]. Maybe you don't have permissions\n", fd);

					exit(2);
				}

				if ((debug) && (debug_level > 0))
					printf(" done\n");

				// Fill in the file with a pattern
				if ((debug) && (debug_level > 0))
					printf("[debug] filling the file (%s) with the pattern...", file_name_src);

				data_written += write(fd, buffer, bufferLen);

				if ((debug) && (debug_level > 0))
					printf(" done. Written %d bytes\n", data_written);


				// Close the file
				if ((debug) && (debug_level > 0))
					printf("[debug] closing file (%s)...", file_name_src);

				close(fd);

				if ((debug) && (debug_level > 0))
					printf(" done\n");
			}
		}

		// 1) If you're using MEMORY (-m option), the maximum data quantity is MAX_MEM_PAYLOAD bytes.
		//    So if someone tries to send more data, you will have to do multiple transmission
		//    in order to avoid daemon failure.
		//    This, however, doesn't affect the goodput measurement, since it is calculated
		//    for each transmission.
		//
		// 2) If you are using FILE, you may want to send an amount of data
		//    using smaller bundles.
		//
		// In both cases we shall calculate how many bundles are needed.

		if (dtnperf_options.transfer_file)
		{
			dtnperf_options.data_qty = lseek(real_filename_fd, 0, SEEK_END);
			lseek(real_filename_fd, 0, SEEK_SET);
		}

		if ((debug) && (debug_level > 0))
			printf("[debug] calculating how many bundles are needed...");

		n_bundles = bundles_needed(dtnperf_options.data_qty, dtnperf_options.bundle_payload);

		if (dtnperf_options.transfer_file)
			n_bundles++;

		if ((debug) && (debug_level > 0))
			printf(" n_bundles = %d\n", n_bundles);

		bundles_ready = dtnperf_options.window;


		// Create the array for the bundle send info
		if ((debug) && (debug_level > 0))
			printf("[debug] creating structure for sending information...");

		if (dtnperf_options.slide_on_custody==1)
		{
			send_info = (send_information_t*) malloc((dtnperf_options.window+1000) * sizeof(send_information_t));
			init_info(send_info, dtnperf_options.window+1000);
		}
		else
		{
			send_info = (send_information_t*) malloc(dtnperf_options.window * sizeof(send_information_t));
			init_info(send_info, dtnperf_options.window);
		}

		if ((debug) && (debug_level > 0))
			printf(" done\n");


		// Run threads
		pthread_cond_init(&cond_sender, NULL);
		pthread_cond_init(&cond_ackreceiver, NULL);
		pthread_mutex_init (&mutexdata, NULL);

		global_options_t arg;
		arg.dtnperf_opt = &dtnperf_options;
		arg.dtn_opt = &dtn_options;

		pthread_create(&sender, NULL, send_bundle, (void*) &arg);
		pthread_create(&ack_receiver, NULL, receive_ack, (void*)&arg);

		pthread_join(ack_receiver, (void**)&pthread_status);
		pthread_join(sender, (void**)&pthread_status);

		pthread_mutex_destroy(&mutexdata);

		// Close source file
		if ((debug) && (debug_level > 0) && (dtnperf_options.transfer_file))
			printf("[debug] deallocating buffer memory...");

		if (dtnperf_options.transfer_file)
			close(real_filename_fd);

		if ((debug) && (debug_level > 0) && (dtnperf_options.transfer_file))
			printf(" done\n");


		free(buffer);
	}
	else
	{
		// This should not be executed (written only for debug purpouse)
		fprintf(stderr, "ERROR: invalid operative mode! Specify -t or -n\n");
		exit(3);
	}


	// Close the DTN handle -- IN DTN_2.1.1 SIMPLY RETURNS -1
	if ((debug) && (debug_level > 0))
		printf("[debug] closing DTN handle...");

	if (dtn_close(handle) != DTN_SUCCESS)
	{
		fprintf(stderr, "fatal error closing dtn handle: %s\n", strerror(errno));
		if (create_log)
			fprintf(log_file, "fatal error closing dtn handle: %s\n", strerror(errno));
		exit(1);
	}

	if ((debug) && (debug_level > 0))
		printf(" done\n");

	if (create_log)
		fclose(log_file);

	if (csv_out)
		fclose(csv_log_file);

	free(send_info);

	pthread_exit(NULL);


	// Final carriage return
	printf("\n");

	return 0;
} // End main




/* ----------------------------------------
 *           UTILITY FUNCTIONS
 * ---------------------------------------- */


/* ----------------------------
 * print_usage
 * ---------------------------- */
void print_usage(char* progname)
{
	fprintf(stderr, "DTNperf ver 2.7\nSYNTAX: %s "
	        "-d <dest_eid> "
	        "[-t <sec> | -n <num>] [options]\n", progname);
	fprintf(stderr, "\n\
 -d, --destination <eid>    Destination eid (required).\n\
 -t, --time <sec>    Time-mode: seconds of transmission.\n\
 -n, --data <num[BKM]>||<file_name> Data-mode: bytes to transmit, data unit default 'M' (Mbytes).\n\
Options common to both Time and Data Mode:\n\
-w, --window <size>  Size of transmission window, i.e. max number of bundles \"in flight\" (not still ACKed by a \"delivered\" status reports); default =1.\n\
-C, --custody [SONC||Slide_on_Custody] Enable both custody transfer and \"custody accepted\" status reports; if SONC||Slide_on_Custody is set, a bundle in the transmission window can be ACKed also by the \"custody accepted\" status report sent by the first custodian but the source.\n\
 -i, --intervalbeforeexit <time> Additional interval before exit.\n\
 -p, --payload <size[BKM]> Size in bytes of bundle payload; data unit default= 'K' (Kbytes).\n\
 -u, --nofragment       Disable bundle fragmentation.\n\
Data-Mode options:\n\
 -m, --memory   Store the bundle into memory instead of file (if payload < 50KB).\n\
Other options:\n\
 -c, --csvout <csv_log_filename> Log all status reports received by the client in a csv (Comma Separated Values) file.\n\
 -D, --debug <level>    Debug messages [0-1-2], if the level is not indicated assume level=0.\n\
 -L, --log <log_filename>       Create a log file.\n\
 -F, --freceipts        Enable \"forwarded\" status reports.\n\
 -R, --rreceipts        Enable \"received\" status reports.\n\
 -h, --help     Help.\n\
 -e, --expiration <time> expiration time in seconds (default: one hour).\n\
 -P, --priority <priority> one of bulk, normal, expedited, or reserved (default normal).\n");
	exit(1);
} // end print_usage



void init_dtnperf_options(dtnperf_options_t *opt)
{
	opt->op_mode = 'd';
	opt->data_qty = 0;
	opt->n_arg = NULL;
	opt->p_arg = NULL;
	opt->use_file = 1;
	opt->transfer_file = 0;
	opt->data_unit = 'M';
	opt->transmission_time = 0;
	opt->window = 1;
	opt->wait_before_exit = 0;
	opt->slide_on_custody = 0;
	opt->regid = DTN_REGID_NONE;
	opt->bundle_payload = DEFAULT_PAYLOAD;
	opt->payload_type = DTN_PAYLOAD_FILE;
}



void init_dtn_options(dtn_options_t* opt)
{
	opt->expiration = 3600; // expiration time (sec) [3600]
	opt->delivery_receipts = 1;    // request delivery receipts [1]
	opt->forwarding_receipts = 0;    // request per hop departure [0]
	opt->custody_transfer = 0;    // request custody transfer [0]
	opt->custody_receipts = 0;    // request per custodian receipts [0]
	opt->receive_receipts = 0;    // request per hop arrival receipt [0]
	opt->wait_for_report = 1;    // wait for bundle status reports [1]
	opt->disable_fragmentation = 0; //disable bundle fragmentation[0]
	opt->priority = COS_NORMAL; // bundle priority [COS_NORMAL]
}


void set_dtn_options(dtn_bundle_spec_t *bundle_spec, dtn_options_t *opt)
{
	// Bundle expiration
	bundle_spec->expiration = opt->expiration;
	
	// Bundle priority
	bundle_spec->priority = opt->priority;

	// Delivery receipt option
	if (opt->delivery_receipts)
	{
		bundle_spec->dopts |= DOPTS_DELIVERY_RCPT;

		if ((debug) && (debug_level > 0))
			printf("DELIVERY_RCPT ");

		if (create_log)
			fprintf(log_file, "DELIVERY_RCPT ");
	}

	// Forward receipt option
	if (opt->forwarding_receipts)
	{
		bundle_spec->dopts |= DOPTS_FORWARD_RCPT;

		if ((debug) && (debug_level > 0))
			printf("FORWARD_RCPT ");

		if (create_log)
			fprintf(log_file, "FORWARD_RCPT ");
	}

	// Custody transfer
	if (opt->custody_transfer)
	{
		bundle_spec->dopts |= DOPTS_CUSTODY;

		if ((debug) && (debug_level > 0))
			printf("CUSTODY ");

		if (create_log)
			fprintf(log_file, "CUSTODY ");
	}


	// Custody receipts
	if (opt->custody_receipts)
	{
		bundle_spec->dopts |= DOPTS_CUSTODY_RCPT;

		if ((debug) && (debug_level > 0))
			printf("CUSTODY_RCPT ");

		if (create_log)
			fprintf(log_file, "CUSTODY_RCPT ");
	}

	// Receive receipt
	if (opt->receive_receipts)
	{
		bundle_spec->dopts |= DOPTS_RECEIVE_RCPT;

		if ((debug) && (debug_level > 0))
			printf("RECEIVE_RCPT ");

		if (create_log)
			fprintf(log_file, "RECEIVE_RCPT ");
	}

	//Disable bundle fragmentation

	if (opt->disable_fragmentation)
	{
		bundle_spec->dopts |= DOPTS_DO_NOT_FRAGMENT;

		if ((debug) && (debug_level > 0))
			printf("DO_NOT_FRAGMENT ");

		if (create_log)
			fprintf(log_file, "DO_NOT_FRAGMENT ");
	}

} // end set_dtn_options



/* ----------------------------
 * parse_options
 * ---------------------------- */
void parse_options(int argc, char**argv, dtnperf_options_t *perf_opt, dtn_options_t *dtn_opt)
{
	char c, done = 0;

	while (!done)
	{
		static struct option long_options[] =
		    {
			    {"destination", required_argument, 0, 'd'},
			    {"time", required_argument, 0, 't'},
			    {"data", required_argument, 0, 'n'},
			    {"file", required_argument, 0, 'f'},
			    {"custody", optional_argument, 0, 'C'},
                {"api_IP", optional_argument, 0, 'A'},
                {"api_port", optional_argument, 0, 'B'},
			    {"window", required_argument, 0, 'w'},
			    {"intervalbeforeexit", required_argument, 0, 'i'},
			    {"payload", required_argument, 0, 'p'},
			    {"memory", no_argument, 0, 'm'},
			    {"csvout", required_argument, 0, 'c'},
			    {"help", no_argument, 0, 'h'},
			    {"debug", required_argument, 0, 'D'},
			    {"log", required_argument, 0, 'L'},
			    {"freceipts", no_argument, 0, 'F'},
			    {"rreceipts", no_argument, 0, 'R'},
			    {"creceipts", no_argument, 0, 'T'},
			    {"nofragment", no_argument, 0, 'u'},
			    {"expiration", no_argument, 0, 'e'},
			    {"priority", no_argument, 0, 'P'}
		    };

		int option_index = 0;
		c = getopt_long(argc, argv, "A:B:hvD::c:mC::w:d:i:t:p:n:FRTuf:L::e:P:", long_options, &option_index);

		switch (c)
		{
        case 'A':
            api_IP_set = 1;
            api_IP = optarg;
            break;
        case 'B':
            api_IP_set = 1;
            api_port = atoi(optarg);
            break;    
		case 'h':
			print_usage(argv[0]);
			exit(0);
			return ;

		case 'c':
			csv_out = 1;
			csv_log_filename = strdup(optarg);
			break;

		case 'C':
			dtn_opt->custody_transfer = 1;
			dtn_opt->custody_receipts = 1;
			if ((optarg!=NULL && (strncmp(optarg, "SONC", 4)==0||strncmp(optarg, "Slide_on_Custody", 16)==0))||((argv[optind]!=NULL)&&(strncmp(argv[optind], "SONC", 4)==0||strncmp(argv[optind], "Slide_on_Custody", 16)==0))){
				perf_opt->slide_on_custody=1;
			}
			break;

		case 'w':
			perf_opt->window = atoi(optarg);
			break;

		case 'i':
			perf_opt->wait_before_exit = atoi(optarg)*1000;
			break;

		case 'd':
			arg_dest = optarg;
			break;

		case 'D':
			debug = 1;
			if (optarg != NULL)
				debug_level = atoi(optarg);
			break;

		case 't':
			perf_opt->op_mode = 't';
			perf_opt->transmission_time = atoi(optarg);
			break;

		case 'n':
			if (file_exists(optarg) == 0)
			{
			  real_filename = strdup(optarg);
				filename = get_filename(optarg); //strdup(optarg);
				perf_opt->transfer_file = 1;
				break;
			}
			perf_opt->n_arg = strdup(optarg);
			perf_opt->data_unit = find_data_unit(perf_opt->n_arg);

			switch (perf_opt->data_unit)
			{
			case 'B':
				perf_opt->data_qty = atol(perf_opt->n_arg);
				break;
			case 'K':
				perf_opt->data_qty = kilo2byte(atol(perf_opt->n_arg));
				break;
			case 'M':
				perf_opt->data_qty = mega2byte(atol(perf_opt->n_arg));
				break;
			default:
				printf("\nWARNING: (-n option) invalid data unit, assuming 'M' (MBytes)\n\n");
				perf_opt->data_qty = mega2byte(atol(perf_opt->n_arg));
				break;
			}
			break;

		case 'p':
			perf_opt->p_arg = optarg;
			perf_opt->data_unit = find_data_unit(perf_opt->p_arg);
			switch (perf_opt->data_unit)
			{
			case 'B':
				perf_opt->bundle_payload = atol(perf_opt->p_arg);
				break;
			case 'K':
				perf_opt->bundle_payload = kilo2byte(atol(perf_opt->p_arg));
				break;
			case 'M':
				perf_opt->bundle_payload = mega2byte(atol(perf_opt->p_arg));

				break;
			default:
				printf("\nWARNING: (-p option) invalid data unit, assuming 'K' (KBytes)\n\n");
				perf_opt->bundle_payload = kilo2byte(atol(perf_opt->p_arg));
				break;
			}
			break;

		case 'f':
			perf_opt->use_file = 1;
			file_name_src = strdup(optarg);
			break;

		case 'm':
			perf_opt->use_file = 0;
			perf_opt->payload_type = DTN_PAYLOAD_MEM;
			break;

		case 'F':
			dtn_opt->forwarding_receipts = 1;
			break;

		case 'R':
			dtn_opt->receive_receipts = 1;
			break;

		case 'T':
			dtn_opt->custody_receipts = 1;
			break;
		
		case 'u':
			dtn_opt->disable_fragmentation = 1;
			break;

		case 'L':
			create_log = 1;
			log_filename = strdup(optarg);
			break;

		case 'e':
			dtn_opt->expiration = atoi(optarg);
			break;

		case 'P':
			if (!strcasecmp(optarg, "bulk"))   {
			    dtn_opt->priority = COS_BULK;
			} else if (!strcasecmp(optarg, "normal")) {
			    dtn_opt->priority = COS_NORMAL;
			} else if (!strcasecmp(optarg, "expedited")) {
			    dtn_opt->priority = COS_EXPEDITED;
			} else if (!strcasecmp(optarg, "reserved")) {
			    dtn_opt->priority = COS_RESERVED;
			} else {
			    fprintf(stderr, "Invalid priority value %s\n", optarg);
			    exit(1);
			}
			break;

		case '?':
			break;

		case (char)(-1):
			done = 1;
			break;

		default:
			// getopt already prints an error message for unknown option characters
			print_usage(argv[0]);
			exit(1);
		} // --switch
	} // -- while


#define CHECK_SET(_arg, _what)                                          	\
    if (_arg == 0) {                                                    	\
        fprintf(stderr, "\nSYNTAX ERROR: %s must be specified\n", _what);   \
        print_usage(argv[0]);                                               \
        exit(1);                                                        	\
    }

	CHECK_SET(arg_dest, "destination tuple");
	CHECK_SET(perf_opt->op_mode, "-t or -n");
} // end parse_options



/* ----------------------------
 * show_options
 * ---------------------------- */
void show_options(dtnperf_options_t *perf_opt, dtn_options_t *dtn_opt)
{
	(void)dtn_opt;
	printf("\nRequested");
	if (perf_opt->op_mode == 't')
		printf(" %d second(s) of transmission\n", perf_opt->transmission_time);
	if (perf_opt->op_mode == 'd')
	{
		if (!perf_opt->transfer_file)
			printf(" %ld byte(s) to be transmitted\n", perf_opt->data_qty);
		else
			printf(" %s file to be transmitted\n", filename);
	}
	printf(" payload of each bundle = %ld byte(s)", perf_opt->bundle_payload);
	printf("\n\n");
} // end show_options



/* ----------------------------
 * check_options
 * ---------------------------- */
void check_options(dtnperf_options_t *perf_opt, dtn_options_t *dtn_opt)
{
	(void)dtn_opt;
	// checks on values
	if ((perf_opt->op_mode == 'd') && (perf_opt->data_qty <= 0) && (filename == NULL))
	{
		fprintf(stderr, "\nSYNTAX ERROR: (-n option) you should send a positive number of MBytes (%ld) or inicate the name of file to transfer\n\n",
		        perf_opt->data_qty);
		exit(2);
	}
	if ((perf_opt->op_mode == 't') && (perf_opt->transmission_time <= 0))
	{
		fprintf(stderr, "\nSYNTAX ERROR: (-t option) you should specify a positive time\n\n");
		exit(2);
	}
	// checks on options combination
	if ((perf_opt->use_file) && (perf_opt->op_mode == 't'))
	{
		if (perf_opt->bundle_payload <= ILLEGAL_PAYLOAD)
		{
			perf_opt->bundle_payload = DEFAULT_PAYLOAD;
			fprintf(stderr, "\nWARNING (a): bundle payload set to %ld bytes\n", perf_opt->bundle_payload);
			fprintf(stderr, "(use_file && op_mode=='t' + payload <= %d)\n\n", ILLEGAL_PAYLOAD);
		}
	}
	if ((perf_opt->use_file) && (perf_opt->op_mode == 'd'))
	{
		if ((perf_opt->bundle_payload <= ILLEGAL_PAYLOAD)
		        || ((perf_opt->bundle_payload > perf_opt->data_qty)	&& (perf_opt->data_qty > 0)))
		{
			perf_opt->bundle_payload = perf_opt->data_qty;
			fprintf(stderr, "\nWARNING (b): bundle payload set to %ld bytes\n", perf_opt->bundle_payload);
			fprintf(stderr, "(use_file && op_mode=='d' + payload <= %d or > %ld)\n\n", ILLEGAL_PAYLOAD, perf_opt->data_qty);
		}
	}
	if ((!perf_opt->use_file)
	        && (perf_opt->bundle_payload <= ILLEGAL_PAYLOAD)
	        && (perf_opt->op_mode == 'd'))
	{
		if (perf_opt->data_qty <= MAX_MEM_PAYLOAD)
		{
			perf_opt->bundle_payload = perf_opt->data_qty;
			fprintf(stderr, "\nWARNING (c1): bundle payload set to %ld bytes\n", perf_opt->bundle_payload);
			fprintf(stderr, "(!use_file + payload <= %d + data_qty <= %d + op_mode=='d')\n\n",
			        ILLEGAL_PAYLOAD, MAX_MEM_PAYLOAD);
		}
		if (perf_opt->data_qty > MAX_MEM_PAYLOAD)
		{
			perf_opt->bundle_payload = MAX_MEM_PAYLOAD;
			fprintf(stderr, "(!use_file + payload <= %d + data_qty > %d + op_mode=='d')\n",
			        ILLEGAL_PAYLOAD, MAX_MEM_PAYLOAD);
			fprintf(stderr, "\nWARNING (c2): bundle payload set to %ld bytes\n\n", perf_opt->bundle_payload);
		}
	}
	if ((!perf_opt->use_file) && (perf_opt->op_mode == 't'))
	{
		if (perf_opt->bundle_payload <= ILLEGAL_PAYLOAD)
		{
			perf_opt->bundle_payload = DEFAULT_PAYLOAD;
			fprintf(stderr, "\nWARNING (d1): bundle payload set to %ld bytes\n\n", perf_opt->bundle_payload);
			fprintf(stderr, "(!use_file + payload <= %d + op_mode=='t')\n\n", ILLEGAL_PAYLOAD);
		}
		if (perf_opt->bundle_payload > MAX_MEM_PAYLOAD)
		{
			fprintf(stderr, "\nWARNING (d2): bundle payload was set to %ld bytes, now set to %ld bytes\n",
			        perf_opt->bundle_payload, (long)DEFAULT_PAYLOAD);
			perf_opt->bundle_payload = DEFAULT_PAYLOAD;
			fprintf(stderr, "(!use_file + payload > %d)\n\n", MAX_MEM_PAYLOAD);
		}
	}

	if (perf_opt->window <= 0)
	{
		fprintf(stderr, "\nSYNTAX ERROR: (-w option) you should specify a positive value of window\n\n");
		exit(2);
	}

	if ((perf_opt->op_mode == 't') && (perf_opt->window == 0))
	{
		fprintf(stderr, "\nSYNTAX ERROR: you cannot use -w option in Time-Mode\n\n");
		exit(2);
	}

	if ((create_log == 1) && (log_filename == NULL))
	{
		fprintf(stderr, "\nSYNTAX ERROR: if you use -L option you should insert a file name for the log file\n\n");
		exit(2);
	}

	if ((csv_out == 1) && (csv_log_filename == NULL))
	{
		fprintf(stderr, "\nSYNTAX ERROR: if you use -L option you should insert a file name for the log file\n\n");
		exit(2);
	}
} // end check_options



void* send_bundle(void *opt)
{
	dtnperf_options_t *perf_opt = ((global_options_t *)(opt))->dtnperf_opt;

	u_int relative_bundleId = 0;

	// Time Mode
	if (perf_opt->op_mode == 't')
	{
		// Initialize timer
		if ((debug) && (debug_level > 0))
			printf("[debug] initializing timer...");

		if (create_log)
			fprintf(log_file, " initializing timer...");

		gettimeofday(&start, NULL);
		
		start.tv_usec = 0;

		if ((debug) && (debug_level > 0))
			printf(" start.tv_sec = %d sec\n", (u_int)start.tv_sec);

		if (create_log)
			fprintf(log_file, " start.tv_sec = %d sec\n", (u_int)start.tv_sec);

		// Calculate end-time
		if ((debug) && (debug_level > 0))
			printf("[debug] calculating end-time...");

		if (create_log)
			fprintf(log_file, " calculating end-time...");

		end = set (0);
		end.tv_sec = start.tv_sec + perf_opt->transmission_time;

		if ((debug) && (debug_level > 0))
			printf(" end.tv_sec = %d sec\n", (u_int)end.tv_sec);

		if (create_log)
			fprintf(log_file, " end.tv_sec = %d sec\n", (u_int)end.tv_sec);

		if ((debug) && (debug_level > 0))
			printf("[debug] entering loop...\n");

		if (create_log)
			fprintf(log_file, " entering loop...\n");


		for (now.tv_sec = start.tv_sec;
		        now.tv_sec <= end.tv_sec;
		        gettimeofday(&now, NULL))
		{
			pthread_mutex_lock(&mutexdata);

			if (bundles_ready == 0)
			{
				pthread_cond_wait(&cond_sender, &mutexdata);
				pthread_mutex_unlock(&mutexdata);
				continue;
			}

			// Send the bundle
			if (debug)
				printf("\t sending the bundle...");

			memset(&bundle_id, 0, sizeof(bundle_id));

			if ((ret = dtn_send(handle, perf_opt->regid, &bundle_spec, &send_payload, &bundle_id)) != 0)
			{
				fprintf(stderr, "error sending bundle: %d (%s)\n", ret, dtn_strerror(dtn_errno(handle)));
				if (create_log)
					fprintf(log_file, "error sending bundle: %d (%s)\n", ret, dtn_strerror(dtn_errno(handle)));
				exit(1);
			}

			gettimeofday(&p_start, NULL);
			--bundles_ready;
			++orphan_acks;
			relative_bundleId = add_info(send_info, bundle_id, p_start, perf_opt->slide_on_custody==1 ? ((perf_opt->window)+1000) : perf_opt->window);

			if (debug)
				printf(" bundle sent\n");
			if ((debug) && (debug_level > 0))
				printf("\t[debug] bundle sent: %llu.%llu\n", (unsigned long long) bundle_id.creation_ts.secs, (unsigned long long) bundle_id.creation_ts.seqno);
			if (create_log)
				fprintf(log_file, "\t bundle sent %llu.%llu\n", (unsigned long long) bundle_id.creation_ts.secs, (unsigned long long) bundle_id.creation_ts.seqno);
			if (csv_out)
			{
				fprintf(csv_log_file, "%Lf,%.0Lf,%llu,STATUS_SENT,%s", 
				        calc_exec_time(p_start.tv_sec, p_start.tv_usec),
				        calc_timestamp(calc_epoch_time(bundle_id.creation_ts.secs)),
				        (unsigned long long) bundle_id.creation_ts.seqno,
				        bundle_spec.source.uri);
				fprintf(csv_log_file, ",%u,No,0,%s\n", relative_bundleId, bundle_spec.dest.uri);
			}


			// Increment sent_bundles
			++sent_bundles;

			if ((debug) && (debug_level > 0))
				printf("\t[debug] now bundles_sent is %d\n", sent_bundles);
			if (create_log)
				fprintf(log_file, "\t now bundles_sent is %d\n", sent_bundles);


			// Increment data_qty
			perf_opt->data_qty += perf_opt->bundle_payload;

/*			if ((debug) && (debug_level > 0))*/
/*				printf("\t[debug] now data_qty is %lu\n", perf_opt->data_qty);*/
/*			if (create_log)*/
/*				fprintf(log_file, "\t now data_qty is %lu\n", perf_opt->data_qty);*/

			pthread_cond_signal(&cond_ackreceiver);
			pthread_mutex_unlock(&mutexdata);
		}
	}
	else	// Data Mode
	{
		long reads = 0;
		long actual_payload = perf_opt->bundle_payload;

		data_written = 0;
		j = 0;

		// Fill the payload
		if ((debug) && (debug_level > 0) && (!perf_opt->transfer_file))
			printf("[debug] filling the bundle payload...");

		memset(&send_payload, 0, sizeof(send_payload));

		if ((perf_opt->use_file) && (!perf_opt->transfer_file))
		{
			dtn_set_payload(&send_payload, DTN_PAYLOAD_FILE, file_name_src, strlen(file_name_src));
		}
		else if ((!perf_opt->use_file) && (!perf_opt->transfer_file))
		{
			dtn_set_payload(&send_payload, DTN_PAYLOAD_MEM, buffer, bufferLen);
		}

		if ((debug) && (debug_level > 0) && (!perf_opt->transfer_file))
			printf(" done\n");

		// Set the file name
		if (perf_opt->transfer_file)
		{
			char temp[1024];
			free(buffer);
			sprintf(temp, "%s/%ld", filename, perf_opt->data_qty);
			bufferLen = strlen(temp) + 1;
			buffer = malloc(bufferLen);
			strcpy(buffer, temp);
			dtn_set_payload(&send_payload, DTN_PAYLOAD_MEM, buffer, bufferLen);
		}


		// Reset data_qty
		if ((debug) && (debug_level > 0))
			printf("[debug] reset data_qty and bundles_sent...");
		perf_opt->data_qty = 0;

		sent_bundles = 0;

		if ((debug) && (debug_level > 0))

			printf(" done\n");

		// Initialize TOTAL start timer
		if ((debug) && (debug_level > 0))
			printf("[debug] initializing TOTAL start timer...");
		if (create_log)
			fprintf(log_file, " initializing TOTAL start timer...");

		gettimeofday(&start, NULL);

		if ((debug) && (debug_level > 0))
			printf(" start.tv_sec = %u sec\n", (u_int)start.tv_sec);
		if (create_log)
			fprintf(log_file, " start.tv_sec = %u sec\n", (u_int)start.tv_sec);


		// Send the name of the file to transfer
		if (perf_opt->transfer_file)
		{
			pthread_mutex_lock(&mutexdata);

			// send the bundle
			if (debug)
				printf("\t sending the name of file...");

			memset(&bundle_id, 0, sizeof(bundle_id));

			if ((ret = dtn_send(handle, perf_opt->regid, &bundle_spec, &send_payload, &bundle_id)) != 0)
			{
				fprintf(stderr, "error sending bundle: %d (%s)\n",
				        ret, dtn_strerror(dtn_errno(handle)));
				if (create_log)
					fprintf(log_file, "error sending bundle: %d (%s)\n", ret, dtn_strerror(dtn_errno(handle)));
				exit(1);
			}

			if (debug)
				printf(" sent\n");

			gettimeofday(&p_start, NULL);
			--bundles_ready;
			++orphan_acks;
			++j;

			relative_bundleId = add_info(send_info, bundle_id, p_start, perf_opt->slide_on_custody==1 ? ((perf_opt->window)+1000) : perf_opt->window);

			if ((debug) && (debug_level > 0))
				printf(" bundle sent %llu.%llu\n", (unsigned long long) bundle_id.creation_ts.secs, (unsigned long long) bundle_id.creation_ts.seqno);
			if (create_log)
				fprintf(log_file, " bundle sent %llu.%llu\n", (unsigned long long) bundle_id.creation_ts.secs, (unsigned long long) bundle_id.creation_ts.seqno);


			// Increment sent_bundles
			++sent_bundles;

			if ((debug) && (debug_level > 0))
				printf("\t[debug] now bundles_sent is %d of %d\n", sent_bundles, n_bundles);
			if (create_log)
				fprintf(log_file, "\t now bundles_sent is %d of %d\n", sent_bundles, n_bundles);
			if (csv_out)
			{
				fprintf(csv_log_file, "%Lf,%.0Lf,%llu,STATUS_SENT,%s",
				        calc_exec_time(p_start.tv_sec, p_start.tv_usec),
				        calc_timestamp(calc_epoch_time(bundle_id.creation_ts.secs)),
				        (unsigned long long) bundle_id.creation_ts.seqno,
				        bundle_spec.source.uri);
				fprintf(csv_log_file, ",%u,No,0,%s\n", relative_bundleId, bundle_spec.dest.uri);
			}

			pthread_mutex_unlock(&mutexdata);
		}

		if (perf_opt->transfer_file)
		{
			free(buffer);
			buffer = malloc(perf_opt->bundle_payload);
			memset(buffer, 0, perf_opt->bundle_payload * sizeof(char));
		}

		if ((debug) && (debug_level > 0))
			printf("\t[debug] entering loop...\n");
		if (create_log)
			fprintf(log_file, "\t entering loop...\n");

		for ( ;
		        (((perf_opt->transfer_file) && ((reads = read(real_filename_fd, buffer, perf_opt->bundle_payload)) > 0))
		         || (j < n_bundles));
		        ++j)
		{
		
			pthread_mutex_lock(&mutexdata);
			if (bundles_ready == 0)
			{
				pthread_cond_wait(&cond_sender, &mutexdata);
				--j;
				lseek(real_filename_fd, data_written, 0);
				pthread_mutex_unlock(&mutexdata);
				continue;
			}

			if (perf_opt->transfer_file)
			{
				// Read from the source file
				bufferLen = reads;

				// Create the file
				if ((debug) && (debug_level > 0))
					printf("[debug] creating file %s...", file_name_src);

				fd = open(file_name_src, O_CREAT | O_TRUNC | O_WRONLY | O_APPEND, 0666);

				if (fd < 0)
				{
					fprintf(stderr, "ERROR: couldn't create file [fd = %d]. Maybe you don't have permissions\n", fd);
					if (create_log)
						fprintf(log_file, "ERROR: couldn't create file [fd = %d]. Maybe you don't have permissions\n", fd);
					exit(2);
				}

				if ((debug) && (debug_level > 0))
					printf(" done\n");

				// Fill the new file with a content of the source file
				if ((debug) && (debug_level > 0))
					printf("[debug] filling the file (%s) with the file_source %s...", file_name_src, filename);

				actual_payload = write(fd, buffer, bufferLen);
				data_written += actual_payload;

				if ((debug) && (debug_level > 0))
					printf(" done. Written %d bytes\n", data_written);

				// Close the file
				if ((debug) && (debug_level > 0))
					printf("[debug] closing file (%s)...", file_name_src);
				close(fd);
				if ((debug) && (debug_level > 0))
					printf(" done\n");

				// Fill the payload if transfer_file is set
				dtn_set_payload(&send_payload, DTN_PAYLOAD_FILE, file_name_src, strlen(file_name_src));

				// Reset the buffer
				free(buffer);
				buffer = malloc(perf_opt->bundle_payload);
				memset(buffer, 0, perf_opt->bundle_payload);

				lseek(real_filename_fd, data_written, 0);
			}


			// Send the bundle
			if (debug)
				printf("\t sending the bundle...");

			memset(&bundle_id, 0, sizeof(bundle_id));

			if ((ret = dtn_send(handle, perf_opt->regid, &bundle_spec, &send_payload, &bundle_id)) != 0)
			{
				fprintf(stderr, "error sending bundle: %d (%s)\n",
				        ret, dtn_strerror(dtn_errno(handle)));
				if (create_log)
					fprintf(log_file, "error sending bundle: %d (%s)\n", ret, dtn_strerror(dtn_errno(handle)));

				exit(1);
			}

			if (debug)
				printf(" sent\n");

			gettimeofday(&p_start, NULL);
			--bundles_ready;
			++orphan_acks;

			relative_bundleId = add_info(send_info, bundle_id, p_start, perf_opt->slide_on_custody==1 ? ((perf_opt->window)+1000) : perf_opt->window);

			if ((debug) && (debug_level > 0))
				printf(" bundle sent %llu.%llu\n", (unsigned long long) bundle_id.creation_ts.secs, (unsigned long long) bundle_id.creation_ts.seqno);
			if (create_log)
				fprintf(log_file, " bundle sent %llu.%llu\n", (unsigned long long) bundle_id.creation_ts.secs, (unsigned long long) bundle_id.creation_ts.seqno);


			// Increment sent_bundles
			++sent_bundles;
			if ((debug) && (debug_level > 0))
				printf("\t[debug] now bundles_sent is %d of %d\n", sent_bundles, n_bundles);
			if (create_log)
				fprintf(log_file, "\t now bundles_sent is %d of %d\n", sent_bundles, n_bundles);
			if (csv_out)
			{
				fprintf(csv_log_file, "%Lf,%.0Lf,%llu,STATUS_SENT,%s", 
				        calc_exec_time(p_start.tv_sec, p_start.tv_usec),
				        calc_timestamp(calc_epoch_time(bundle_id.creation_ts.secs)),
				        (unsigned long long) bundle_id.creation_ts.seqno,
				        bundle_spec.source.uri);
				fprintf(csv_log_file, ",%u,No,0,%s\n", relative_bundleId, bundle_spec.dest.uri);
			}

			// Increment data_qty
			perf_opt->data_qty += actual_payload;
/*			if ((debug) && (debug_level > 0))*/
/*				printf("\t[debug] now data_qty is %lu\n", perf_opt->data_qty);*/
/*			if (create_log)*/
/*				fprintf(log_file, "\t now data_qty is %lu\n", perf_opt->data_qty);*/

			pthread_cond_signal(&cond_ackreceiver);
			pthread_mutex_unlock(&mutexdata);
		} // end for(n_bundles)

	}

	if ((debug) && (debug_level > 0))
		printf("[debug] ...out from loop\n");
	if (create_log)
		fprintf(log_file, " ...out from loop\n");

	pthread_mutex_lock(&mutexdata);
	close_ack_receiver = 1;
	pthread_cond_signal(&cond_ackreceiver);
	pthread_mutex_unlock(&mutexdata);
	pthread_exit(NULL);

} // end send_bundle



void* receive_ack(void *opt)
{
	dtnperf_options_t *perf_opt = ((global_options_t *)(opt))->dtnperf_opt;

	char* ack_sender = strdup(arg_dest);
	int ack_set = 0;
	struct timeval temp;

	int position = -1;

	while ((close_ack_receiver == 0) || (orphan_acks > 0) || (gettimeofday(&temp, NULL) == 0 && p_end.tv_sec - temp.tv_sec <= perf_opt->wait_before_exit))
	{
		pthread_mutex_lock(&mutexdata);
		if (close_ack_receiver == 0 && orphan_acks == 0)
		{
			pthread_cond_wait(&cond_ackreceiver, &mutexdata);
			pthread_mutex_unlock(&mutexdata);
			pthread_yield();
			continue;
		}

		// Wait for the reply
		if ((debug) && (debug_level > 0))
			printf("\t[debug] waiting for the reply...\n");

		if ((ret = dtn_recv(handle, &reply_spec, DTN_PAYLOAD_MEM, &reply_payload, orphan_acks == 0 ? perf_opt->wait_before_exit : -1)) < 0)
		{
			if(orphan_acks == 0 && close_ack_receiver == 1)
				break;
			fprintf(stderr, "error getting reply: %d (%s)\n", ret, dtn_strerror(dtn_errno(handle)));
			if (create_log)
				fprintf(log_file, "error getting reply: %d (%s)\n", ret, dtn_strerror(dtn_errno(handle)));
			exit(1);
		}
		gettimeofday(&p_end, NULL);


		// Set the source of the ack
		if (ack_set == 0)
		{
			if (perf_opt->slide_on_custody == 1)
			{
				if ((strcmp(reply_payload.status_report->bundle_id.source.uri, bundle_spec.source.uri) == 0) &&
				        (is_in_info(send_info, reply_payload.status_report->bundle_id, perf_opt->slide_on_custody == 1 ? ((perf_opt->window)+1000) : perf_opt->window) >= 0) &&
				        (strncmp(reply_spec.source.uri, bundle_spec.source.uri, (strlen(reply_spec.source.uri))) != 0) &&
				        (reply_payload.status_report->flags == STATUS_CUSTODY_ACCEPTED))
				{
					ack_sender = strdup(reply_spec.source.uri);
					ack_set=1;
				}
			}
		}

		if ((strncmp(reply_spec.source.uri, ack_sender, (strlen(reply_spec.source.uri))) == 0)
		        && (strcmp(reply_payload.status_report->bundle_id.source.uri, bundle_spec.source.uri) == 0)
		        && ((position = is_in_info(send_info, reply_payload.status_report->bundle_id, perf_opt->slide_on_custody == 1 ? ((perf_opt->window)+1000) : perf_opt->window)) >= 0)
		        && (perf_opt->slide_on_custody == 0 ? reply_payload.status_report->flags == STATUS_DELIVERED : reply_payload.status_report->flags == STATUS_CUSTODY_ACCEPTED))
		{
			if (csv_out)
					fprintf(csv_log_file, "%Lf,%.0Lf,%llu,", 
					    calc_exec_time(p_end.tv_sec, p_end.tv_usec),
 					    calc_timestamp( calc_epoch_time(reply_spec.creation_ts.secs)),
 					    (unsigned long long) reply_spec.creation_ts.seqno
 					    );
			
			if (reply_payload.status_report->flags == STATUS_DELIVERED)
			{
				if (debug)
					printf("\t Received ack\n");
				if ((debug) && (debug_level > 1))
					printf("\t[debug] STATUS_DELIVERED in %ld ms from its shipment\n", (((p_end.tv_sec - (send_info[position].send_time.tv_sec))*1000) + ((p_end.tv_usec - (send_info[position].send_time.tv_usec)) / 1000)) );
				if (create_log)
					fprintf(log_file, "\t %Lf\t STATUS_DELIVERED in %ld ms from its shipment\n", 
					    calc_exec_time(p_end.tv_sec, p_end.tv_usec),
             (((p_end.tv_sec - (send_info[position].send_time.tv_sec))*1000) + ((p_end.tv_usec - (send_info[position].send_time.tv_usec)) / 1000)) );
				if (csv_out)
					fprintf(csv_log_file, "STATUS_DELIVERED");

			  remove_from_info(send_info, position);
			  --orphan_acks;
			}
			else if (reply_payload.status_report->flags == STATUS_CUSTODY_ACCEPTED)
			{
				if ((debug) && (debug_level > 1))
					printf("\t[debug] signalling of STATUS_CUSTODY_ACCEPTED from %s for: %llu.%llu created by %s\n", reply_spec.source.uri, (unsigned long long) reply_payload.status_report->bundle_id.creation_ts.secs, (unsigned long long) reply_payload.status_report->bundle_id.creation_ts.seqno, reply_payload.status_report->bundle_id.source.uri);
				if (csv_out)
					fprintf(csv_log_file, "STATUS_CUSTODY_ACCEPTED");
				if (create_log)
					fprintf(log_file, "\t %Lf\t signalling of STATUS_CUSTODY_ACCEPTED from %s for: %llu.%llu created by %s\n",
					    calc_exec_time(p_end.tv_sec, p_end.tv_usec),
                 reply_spec.source.uri, (unsigned long long) reply_payload.status_report->bundle_id.creation_ts.secs, (unsigned long long) reply_payload.status_report->bundle_id.creation_ts.seqno, reply_payload.status_report->bundle_id.source.uri);
			}
			
			if (csv_out)
			{
					fprintf(csv_log_file, ",%s,%u,%s", reply_spec.source.uri, send_info[position].relative_id, (reply_payload.status_report->bundle_id.orig_length==0 && reply_payload.status_report->bundle_id.frag_offset ==0)? "No":"Yes");
					fprintf(csv_log_file, ",%u,%s\n", reply_payload.status_report->bundle_id.frag_offset, reply_payload.status_report->bundle_id.source.uri);
			}
			++bundles_ready;
		}
		else if ((strcmp(reply_payload.status_report->bundle_id.source.uri, bundle_spec.source.uri) == 0)
		         && ((position = is_in_info(send_info, reply_payload.status_report->bundle_id, perf_opt->slide_on_custody==1 ? ((perf_opt->window)+1000) : perf_opt->window)) >= 0))
		{
		  if (csv_out)
					fprintf(csv_log_file, "%Lf,%.0Lf,%llu,",
					    calc_exec_time(p_end.tv_sec, p_end.tv_usec),
              calc_timestamp(calc_epoch_time(reply_spec.creation_ts.secs)),
 					   (unsigned long long) reply_spec.creation_ts.seqno
              );
					
			if (reply_payload.status_report->flags == STATUS_DELIVERED)
			{
				if (debug)
					printf("\t Received ack\n");
				if ((debug) && (debug_level > 1))
					printf("\t[debug] STATUS_DELIVERED in %ld ms from its shipment\n", (((p_end.tv_sec - (send_info[position].send_time.tv_sec))*1000) + ((p_end.tv_usec - (send_info[position].send_time.tv_usec)) / 1000)) );
				if (create_log)
					fprintf(log_file, "\t %Lf\t STATUS_DELIVERED in %ld ms from its shipment\n", 
					    calc_exec_time(p_end.tv_sec, p_end.tv_usec),
                 (((p_end.tv_sec - (send_info[position].send_time.tv_sec))*1000) + ((p_end.tv_usec - (send_info[position].send_time.tv_usec)) / 1000)) );
				if (csv_out)
					fprintf(csv_log_file, "STATUS_DELIVERED");
			remove_from_info(send_info, position);
			--orphan_acks;
			}
			else if (reply_payload.status_report->flags == STATUS_RECEIVED)
			{
				if ((debug) && (debug_level > 1))
					printf("\t[debug] signalling of STATUS_RECEIVED from %s for: %llu.%llu created by %s\n", reply_spec.source.uri, (unsigned long long) reply_payload.status_report->bundle_id.creation_ts.secs, (unsigned long long) reply_payload.status_report->bundle_id.creation_ts.seqno, reply_payload.status_report->bundle_id.source.uri);
				if (csv_out)
					fprintf(csv_log_file, "STATUS_RECEIVED");
				if (create_log)
					fprintf(log_file, "\t %Lf\t signalling of STATUS_RECEIVED from %s for: %llu.%llu created by %s\n",
					    calc_exec_time(p_end.tv_sec, p_end.tv_usec),
                 reply_spec.source.uri, (unsigned long long) reply_payload.status_report->bundle_id.creation_ts.secs, (unsigned long long) reply_payload.status_report->bundle_id.creation_ts.seqno, reply_payload.status_report->bundle_id.source.uri);
			}
			else if (reply_payload.status_report->flags == STATUS_CUSTODY_ACCEPTED)
			{
				if ((debug) && (debug_level > 1))
					printf("\t[debug] signalling of STATUS_CUSTODY_ACCEPTED from %s for: %llu.%llu created by %s\n", reply_spec.source.uri, (unsigned long long) reply_payload.status_report->bundle_id.creation_ts.secs, (unsigned long long) reply_payload.status_report->bundle_id.creation_ts.seqno, reply_payload.status_report->bundle_id.source.uri);
				if (csv_out)
					fprintf(csv_log_file, "STATUS_CUSTODY_ACCEPTED");
				if (create_log)
					fprintf(log_file, "\t %Lf\t signalling of STATUS_CUSTODY_ACCEPTED from %s for: %llu.%llu created by %s\n",
					    calc_exec_time(p_end.tv_sec, p_end.tv_usec),
                 reply_spec.source.uri, (unsigned long long) reply_payload.status_report->bundle_id.creation_ts.secs, (unsigned long long) reply_payload.status_report->bundle_id.creation_ts.seqno, reply_payload.status_report->bundle_id.source.uri);
			}
			else if (reply_payload.status_report->flags == STATUS_FORWARDED)
			{
				if ((debug) && (debug_level > 1))
					printf("\t[debug] signalling of STATUS_FORWARDED from %s for: %llu.%llu created by %s\n", reply_spec.source.uri, (unsigned long long) reply_payload.status_report->bundle_id.creation_ts.secs, (unsigned long long) reply_payload.status_report->bundle_id.creation_ts.seqno, reply_payload.status_report->bundle_id.source.uri);
				if (csv_out)
					fprintf(csv_log_file, "STATUS_FORWARDED");
				if (create_log)
					fprintf(log_file, "\t %Lf\t signalling of STATUS_FORWARDED from %s for: %llu.%llu created by %s\n",
					    calc_exec_time(p_end.tv_sec, p_end.tv_usec),
                 reply_spec.source.uri, (unsigned long long) reply_payload.status_report->bundle_id.creation_ts.secs, (unsigned long long) reply_payload.status_report->bundle_id.creation_ts.seqno, reply_payload.status_report->bundle_id.source.uri);
			}
			else if (reply_payload.status_report->flags == STATUS_DELETED)
			{
				if ((debug) && (debug_level > 1))
					printf("\t[debug] signalling of STATUS_DELETED from %s for: %llu.%llu created by %s\n", reply_spec.source.uri, (unsigned long long) reply_payload.status_report->bundle_id.creation_ts.secs, (unsigned long long) reply_payload.status_report->bundle_id.creation_ts.seqno, reply_payload.status_report->bundle_id.source.uri);
				if (csv_out)
					fprintf(csv_log_file, "STATUS_DELETED");
				if (create_log)
					fprintf(log_file, "\t %Lf\t signalling of STATUS_DELETED from %s for: %llu.%llu created by %s\n",
					    calc_exec_time(p_end.tv_sec, p_end.tv_usec),
                 reply_spec.source.uri, (unsigned long long) reply_payload.status_report->bundle_id.creation_ts.secs, (unsigned long long) reply_payload.status_report->bundle_id.creation_ts.seqno, reply_payload.status_report->bundle_id.source.uri);
			}
			else if (reply_payload.status_report->flags == STATUS_ACKED_BY_APP)
			{
				if ((debug) && (debug_level > 1))
					printf("\t[debug] signalling of STATUS_ACKED_BY_APP from %s for: %llu.%llu created by %s\n", reply_spec.source.uri, (unsigned long long) reply_payload.status_report->bundle_id.creation_ts.secs, (unsigned long long) reply_payload.status_report->bundle_id.creation_ts.seqno, reply_payload.status_report->bundle_id.source.uri);
				if (csv_out)
					fprintf(csv_log_file, "STATUS_ACKED_BY_APP");
				if (create_log)
					fprintf(log_file, "\t %Lf\t signalling of STATUS_ACKED_BY_APP from %s for: %llu.%llu created by %s\n",
					    calc_exec_time(p_end.tv_sec, p_end.tv_usec),
                 reply_spec.source.uri, (unsigned long long) reply_payload.status_report->bundle_id.creation_ts.secs, (unsigned long long) reply_payload.status_report->bundle_id.creation_ts.seqno, reply_payload.status_report->bundle_id.source.uri);
			}
			
  		if (csv_out)
			{
					fprintf(csv_log_file, ",%s,%u,%s", reply_spec.source.uri, send_info[position].relative_id, (reply_payload.status_report->bundle_id.orig_length==0 && reply_payload.status_report->bundle_id.frag_offset ==0)? "No":"Yes");
					fprintf(csv_log_file, ",%u,%s\n", reply_payload.status_report->bundle_id.frag_offset, reply_payload.status_report->bundle_id.source.uri);
			}
		}

		else
		{
			if ((debug) && (debug_level > 1))
				printf("\t[debug] received bundle outside sequence: %llu.%llu\n", (unsigned long long) reply_payload.status_report->bundle_id.creation_ts.secs, (unsigned long long) reply_payload.status_report->bundle_id.creation_ts.seqno);
			if (create_log)
				fprintf(log_file, "\t %Lf\t received bundle outside sequence: %llu.%llu\n",
					    calc_exec_time(p_end.tv_sec, p_end.tv_usec),
                 (unsigned long long) reply_payload.status_report->bundle_id.creation_ts.secs, (unsigned long long) reply_payload.status_report->bundle_id.creation_ts.seqno);
		}

		dtn_free_payload(&reply_payload);

		pthread_cond_signal(&cond_sender);
		pthread_mutex_unlock(&mutexdata);
		pthread_yield();
	} // end while(n_bundles)

	if (perf_opt->op_mode != 't')
	{
	  // Data Mode
		// Calculate TOTAL end time
		if ((debug) && (debug_level > 0))
			printf("[debug] calculating TOTAL end time...");

		gettimeofday(&end, NULL);

		if ((debug) && (debug_level > 0))
			printf(" end.tv_sec = %u sec\n", (u_int)end.tv_sec);


		// Show the TOTAL report
		printf("%d bundles sent, each with a %ld bytes payload\n", sent_bundles, perf_opt->bundle_payload);
		show_report(reply_payload.buf.buf_len,
		            reply_spec.source.uri,
		            start,
		            end,
		            perf_opt->data_qty,
		            NULL);


		if (create_log)
		{
			fprintf(log_file, "%d bundles sent, each with a %ld bytes payload\n", sent_bundles, perf_opt->bundle_payload);
			show_report(reply_payload.buf.buf_len,
			            reply_spec.source.uri,
			            start,
			            end,
			            perf_opt->data_qty,
			            log_file);
		}

		if (csv_out == 1)
		{
			csv_data_report(sent_bundles, perf_opt->data_qty, start, end, csv_log_file);
		}
	}
	pthread_exit(NULL);
	return NULL;
} // end receive_ack


void sigint(int sig)
{
	(void)sig;
	if(csv_log_file != NULL)
		fclose(csv_log_file);	
	
	if(log_file != NULL)
		fclose(log_file);	

	exit(0);	
}

/* --------------------------------------------------
 * file_exists
 * -------------------------------------------------- */
int file_exists(const char * filename)
{
    FILE * file;
    if ((file = fopen(filename, "r")) != NULL)
    {
        fclose(file);
        return 0;
    }
    return 1;
} // end file_exists

long double calc_exec_time ( long long unsigned sec, long long unsigned sec_no ) 
{
  return ((long double)(sec) - start.tv_sec) + (((long double)(sec_no) - start.tv_usec) / 1000000.0 ) ;
}

long double calc_timestamp ( long long unsigned sec ) {
  return ((long double)(sec) - start.tv_sec);
}

long long unsigned calc_epoch_time ( long long unsigned dtn_time ) 
{
  long long unsigned offset = 946684800; // 2000-01-01 DTN epoch
  return dtn_time + offset;
}
