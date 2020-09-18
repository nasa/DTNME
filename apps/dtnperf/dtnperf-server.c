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
 *         DTNperf 2.5.1 - SERVER
 *
 *             developed by
 * 
 * Piero Cornice - piero.cornice(at)gmail.com
 * Marco Livini - marco.livini(at)gmail.com
 *
 * DEIS - Dipartimento di Elettronica, Informatica e Sistemistica
 * Universita' di Bologna
 * Italy
 * ----------------------------------------
 */



#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <strings.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <getopt.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <dtn_api.h>



#define TEMPSIZE 1024
#define BUFSIZE 16
#define BUNDLE_DIR_DEFAULT "/var/dtn/dtnperf"
#define OUTFILE "dtnbuffer.rcv"
#define MAXSIZE 256


/* ------------------------------
 *  Global variables and options
 * ------------------------------ */

// Daemon connection
int api_IP_set = 0;
char * api_IP = "127.0.0.1";
short api_port = 5010;

// values between [square brackets] are default
const char *progname ;
int use_file = 1;        // if set to 0, memorize received bundles into memory (max 50000 bytes)
// if set to 1, memorize received bundles into a file (-f) [0]
int verbose = 0;        // if set to 1, show debug_level 0 messages [0]
int aggregate = 0;        // if > 0, print after aggregate arrivals
int debug = 0;        // if set to 1, show debug messages [1]
int debug_level = 0;
char * endpoint = "/dtnperf:/dest";     // endpoint (-e) ["/dtnperf:/dest"]
char * bundle_dir = BUNDLE_DIR_DEFAULT;   // destination directory (-d)

/* ------------------------
 *  Function Prototypes
 * ------------------------ */
void print_usage(char*);
void parse_options(int, char**);
dtn_endpoint_id_t* parse_eid(dtn_handle_t, dtn_endpoint_id_t*, char*);
int ContainsChar(char* string, char c);
char* GetFileName(char* string, int position);
long GetFileLen(char* string, int position);
int move_file (char *fileIn, char *fileOut);


/* -------------------------------------------------
 * main
 * ------------------------------------------------- */
int main(int argc, char** argv)
{

	/* -----------------------
	 *  variables declaration
	 * ----------------------- */
	int k;
	int ret;
	dtn_handle_t handle;
	dtn_endpoint_id_t local_eid;
	dtn_reg_info_t reginfo;
	dtn_reg_id_t regid;
	dtn_bundle_spec_t spec;
	dtn_bundle_payload_t payload;
	char *buffer;                   // buffer used for shell commands
	char s_buffer[BUFSIZE];
	time_t current;
	dtn_bundle_payload_location_t pl_type = DTN_PAYLOAD_FILE; // payload saved into memory or into file
	int source_eid_len, dest_eid_len;
	char *source_eid, *dest_eid;
	char * filepath;
	char * filename = OUTFILE;      // source filename [OUTFILE]
	//int bufsize;
	int count = 0;
	int total = 0;
	int receiving_file = -1;
	int destination_file = -1;
	char temp[TEMPSIZE];
	char* receiving_fileName = NULL;
	long separator_position = 0;
	long receiving_fileLen = 0;
	long total_written = 0;
	long reads = 0;
	long written = 0;
	long total_saved = 0;
	
	
	/* -------
	 *  begin
	 * ------- */

	// parse command-line options
	parse_options(argc, argv);
	if ((debug) && (debug_level > 0))
		printf("[debug] parsed command-line options\n");

	// show requested options (debug)
	if (debug)
	{
		printf("\nOptions:\n");
		printf("\tendpoint       : %s\n", endpoint);
		printf("\tsave bundles to: %s\n", use_file ? "file" : "memory");
		printf("\tdestination dir: %s\n", bundle_dir);
		printf("\n");
	}

	// initialize buffer with shell command ("mkdir -p " + bundle_dir)
	if ((debug) && (debug_level > 0))
		printf("[debug] initializing buffer with shell command...");
	buffer = malloc(sizeof(char) * (strlen(bundle_dir) + 10));
	sprintf(buffer, "mkdir -p %s", bundle_dir);
	if ((debug) && (debug_level > 0))
		printf(" done. Shell command = %s\n", buffer);

	// execute shell command
	if ((debug) && (debug_level > 0))
		printf("[debug] executing shell command \"%s\"...", buffer);
	if (system(buffer) == -1)
	{
		fprintf(stderr, "Error opening bundle directory: %s\n", bundle_dir);
		exit(1);
	}
	free(buffer);
	if ((debug) && (debug_level > 0))
		printf(" done\n");

	// open the ipc handle
	if ((debug) && (debug_level > 0))
		printf("[debug] opening connection to dtn router...");

    int err = 0;
    if (api_IP_set) err = dtn_open_with_IP(api_IP,api_port,&handle);
    else err = dtn_open(&handle);

	if (err != DTN_SUCCESS)
	{
		fprintf(stderr, "fatal error opening dtn handle: %s\n",
		        dtn_strerror(err));
		exit(1);
	}
	if ((debug) && (debug_level > 0))
		printf(" done\n");

	// build a local tuple based on the configuration of local dtn router
	if ((debug) && (debug_level > 0))
		printf("[debug] building local eid...");
	dtn_build_local_eid(handle, &local_eid, endpoint);
	if ((debug) && (debug_level > 0))
		printf(" done\n");
	if (debug)
		printf("local_eid = %s\n", local_eid.uri);

	// create a new registration based on this eid
	if ((debug) && (debug_level > 0))
		printf("[debug] registering to local daemon...");
	memset(&reginfo, 0, sizeof(reginfo));
	dtn_copy_eid(&reginfo.endpoint, &local_eid);
	reginfo.flags = DTN_REG_DEFER;
	reginfo.regid = DTN_REGID_NONE;
	reginfo.expiration = 0;
	if ((ret = dtn_register(handle, &reginfo, &regid)) != 0)
	{
		fprintf(stderr, "error creating registration: %d (%s)\n",
		        ret, dtn_strerror(dtn_errno(handle)));
		exit(1);
	}
	if ((debug) && (debug_level > 0))
		printf(" done\n");
	if (debug)
		printf("regid 0x%x\n", regid);

	// set bundle destination type
	if ((debug) && (debug_level > 0))
		printf("[debug] choosing bundle destination type...");
	if (use_file)
		pl_type = DTN_PAYLOAD_FILE;
	else
		pl_type = DTN_PAYLOAD_MEM;
	if ((debug) && (debug_level > 0))
		printf(" done. Bundles will be saved into %s\n", use_file ? "file" : "memory");

	if ((debug) && (debug_level > 0))
		printf("[debug] entering infinite loop...\n");

	// infinite loop, waiting for bundles
	while (1)
	{

		// wait until receive a bundle
		memset(&spec, 0, sizeof(spec));
		memset(&payload, 0, sizeof(payload));

		if ((debug) && (debug_level > 0))
			printf("[debug] waiting for bundles...");
		if ((ret = dtn_recv(handle, &spec, pl_type, &payload, -1)) < 0)
		{
			fprintf(stderr, "error getting recv reply: %d (%s)\n",
			        ret, dtn_strerror(dtn_errno(handle)));
			exit(1);
		}
		if ((debug) && (debug_level > 0))
			printf(" bundle received\n");
		count++;

		size_t len;
		if (pl_type == DTN_PAYLOAD_MEM)
		{
			len = payload.buf.buf_len;
		}
		else
		{
			struct stat st;
			memset(&st, 0, sizeof(st));
			stat(payload.filename.filename_val, &st);
			len = st.st_size;
		}

		total += len;
		// mark current time
		if ((debug) && (debug_level > 0))
			printf("[debug] marking time...");
		current = time(NULL);
		if ((debug) && (debug_level > 0))
			printf(" done\n");
		if (aggregate == 0)
		{
			printf("%s : %zu bytes from %s\n",
			       ctime(&current),
			       len,
			       spec.source.uri);
		}
		else if (count % aggregate == 0)
		{
			printf("%s : %d bundles, total length %d bytes\n",
			       ctime(&current), count, total);
		}

		/* ---------------------------------------------------
		 *  parse admin string to select file target location
		 * --------------------------------------------------- */

		// copy SOURCE eid
		if ((debug) && (debug_level > 0))
			printf("[debug]\tcopying source eid...");
		source_eid_len = sizeof(spec.source.uri);
		source_eid = malloc(sizeof(char) * source_eid_len + 1);
		memcpy(source_eid, spec.source.uri, source_eid_len);
		source_eid[source_eid_len] = '\0';
		if ((debug) && (debug_level > 0))
		{
			printf(" done:\n");
			printf("\tsource_eid = %s\n", source_eid);
			printf("\n");
		}

		// copy DEST eid
		if ((debug) && (debug_level > 0))
			printf("[debug]\tcopying dest eid...");
		dest_eid_len = sizeof(spec.dest.uri);
		dest_eid = malloc(sizeof(char) * dest_eid_len + 1);
		memcpy(dest_eid, spec.dest.uri, dest_eid_len);
		dest_eid[dest_eid_len] = '\0';
		if ((debug) && (debug_level > 0))
		{
			printf(" done:\n");
			printf("\tdest_eid = %s\n", dest_eid);
			printf("\n");
		}

		// recursively create full directory path
		filepath = malloc(sizeof(char) * dest_eid_len + 10);
		sprintf(filepath, "mkdir -p %s", bundle_dir);
		if ((debug) && (debug_level > 0))
			printf("[debug] filepath = %s\n", filepath);
		system(filepath);

		// create file name
		sprintf(filepath, "%s/%s", bundle_dir, filename);
		if ((debug) && (debug_level > 0))
			printf("[debug] filepath = %s\n", filepath);

		// bundle name is the name of the bundle payload file
		buffer = payload.buf.buf_val;
		//bufsize = payload.buf.buf_len;

		if ((debug) && (debug_level > 0))
		{
			printf ("======================================\n");
			printf (" Bundle received at %s\n", ctime(&current));
			printf ("  source: %s\n", spec.source.uri);
			if (use_file)
			{
				printf ("  saved into    : %s\n", filepath);
			}

			printf ("--------------------------------------\n");
		}

		//receiving file
		if (TEMPSIZE >= len)
		{
			if (pl_type == DTN_PAYLOAD_FILE)
			{ // if bundle was saved into file
				if ((receiving_file = open(payload.filename.filename_val, O_RDONLY)) < 0)
				{
					fprintf(stderr, "fatal error opening file %s\n", payload.filename.filename_val);
					exit(1);
				}
			}

			if (((pl_type == DTN_PAYLOAD_FILE) && (read(receiving_file, temp, len) > 0)) || (payload.buf.buf_len > 0))
			{
				printf("&&(TEMPSIZE>=len)\n");
				fflush(stdout);
				if (payload.buf.buf_len > 0)
					strcpy(temp, payload.buf.buf_val);

				if ((separator_position = ContainsChar(temp, '/')) > 0)
				{
					if ((debug) && (debug_level > 0))
						printf("[debug]  Getting the name of file to receive...");
					receiving_fileName = GetFileName(temp, separator_position);
					if ((debug) && (debug_level > 0))
						printf("done\n");
					if ((debug) && (debug_level > 0))
						printf("[debug]  Getting the lenght of file to receive...");
					receiving_fileLen = GetFileLen(temp, separator_position);
					if ((debug) && (debug_level > 0))
						printf("done\n");
				}
			}

			if (pl_type == DTN_PAYLOAD_FILE)
			{
				close(receiving_file);
				receiving_file = -1;
			}
		}

		if (pl_type == DTN_PAYLOAD_FILE)
		{ // if bundle was saved into file

			if ((debug) && (debug_level > 0))
				printf("[debug] renaming file %s -> %s...",
				       payload.filename.filename_val, filepath);

			if (move_file(payload.filename.filename_val, filepath) != 0)
			{
				printf("[ERROR] Couldn't rename %s -> %s: %s\n",
				       payload.filename.filename_val, filepath, strerror(errno));
			}

		}
		else
		{ // if bundle was saved into memory

			if ((debug) && (debug_level > 0))
			{
				for (k = 0; k < (int)payload.buf.buf_len; k++)
				{
					if (buffer[k] >= ' ' && buffer[k] <= '~')
						s_buffer[k % BUFSIZE] = buffer[k];
					else
						s_buffer[k % BUFSIZE] = '.';

					if (k % BUFSIZE == 0) // new line every 16 bytes
					{
						printf("%07x ", k);
					}
					else if (k % 2 == 0)
					{
						printf(" "); // space every 2 bytes
					}

					printf("%02x", buffer[k] & 0xff);

					// print character summary (a la emacs hexl-mode)
					if (k % BUFSIZE == BUFSIZE - 1)
					{
						printf(" |  %.*s\n", BUFSIZE, s_buffer);
					}
				} // for

				// print spaces to fill out the rest of the line
				if (k % BUFSIZE != BUFSIZE - 1)
				{
					while (k % BUFSIZE != BUFSIZE - 1)
					{
						if (k % 2 == 0)
						{
							printf(" ");
						}
						printf("  ");
						k++;
					}
					printf("   |  %.*s\n",
					       (int)payload.buf.buf_len % BUFSIZE, s_buffer);
				} // if

				printf ("======================================\n");

			} // if (debug)

			free(filepath);
			free(source_eid);
			free(dest_eid);

		}

		fflush(stdout);

		//reciving file
		if (receiving_fileName != NULL)
		{
			printf("\nreceiving file %s - lenght %lu\n", receiving_fileName, receiving_fileLen);
			if (pl_type == DTN_PAYLOAD_FILE)
				close(receiving_file);
			receiving_file = -1;

			if ((destination_file = open(receiving_fileName, O_CREAT | O_TRUNC | O_WRONLY | O_APPEND, 0666)) < 0)
			{
				fprintf(stderr, "fatal error opening file %s\n", receiving_fileName);
				exit(1);
			}

			while (total_saved < receiving_fileLen)
			{

				// wait until receive a bundle
				memset(&spec, 0, sizeof(spec));
				memset(&payload, 0, sizeof(payload));

				if ((debug) && (debug_level > 0))
					printf("[debug] waiting for bundles...");
				if ((ret = dtn_recv(handle, &spec, pl_type, &payload, -1)) < 0)
				{
					fprintf(stderr, "error getting recv reply: %d (%s)\n", ret, dtn_strerror(dtn_errno(handle)));
					exit(1);
				}
				if ((debug) && (debug_level > 0))
					printf(" bundle received\n");
				count++;

				size_t len;
				if (pl_type == DTN_PAYLOAD_MEM)
				{
					len = payload.buf.buf_len;
				}
				else
				{
					struct stat st;
					memset(&st, 0, sizeof(st));
					stat(payload.filename.filename_val, &st);
					len = st.st_size;
				}

				total += len;
				// mark current time
				if ((debug) && (debug_level > 0))
					printf("[debug] marking time...");
				current = time(NULL);
				if ((debug) && (debug_level > 0))
					printf(" done\n");
				if (aggregate == 0)
				{
					printf("%s : %zu bytes from %s\n",
					       ctime(&current),
					       len,
					       spec.source.uri);
				}
				else if (count % aggregate == 0)
				{
					printf("%s : %d bundles, total length %d bytes\n",
					       ctime(&current), count, total);
				}

				/* ---------------------------------------------------
				 *  parse admin string to select file target location
				 * --------------------------------------------------- */

				// copy SOURCE eid
				if ((debug) && (debug_level > 0))
					printf("[debug]\tcopying source eid...");
				source_eid_len = sizeof(spec.source.uri);
				source_eid = malloc(sizeof(char) * source_eid_len + 1);
				memcpy(source_eid, spec.source.uri, source_eid_len);
				source_eid[source_eid_len] = '\0';

				if ((debug) && (debug_level > 0))
				{
					printf(" done:\n");
					printf("\tsource_eid = %s\n", source_eid);
					printf("\n");
				}

				// copy DEST eid
				if ((debug) && (debug_level > 0))
					printf("[debug]\tcopying dest eid...");
				dest_eid_len = sizeof(spec.dest.uri);
				dest_eid = malloc(sizeof(char) * dest_eid_len + 1);
				memcpy(dest_eid, spec.dest.uri, dest_eid_len);
				dest_eid[dest_eid_len] = '\0';
				if ((debug) && (debug_level > 0))
				{
					printf(" done:\n");
					printf("\tdest_eid = %s\n", dest_eid);
					printf("\n");
				}

				// recursively create full directory path
				filepath = malloc(sizeof(char) * dest_eid_len + 10);
				sprintf(filepath, "mkdir -p %s", bundle_dir);
				if ((debug) && (debug_level > 0))
					printf("[debug] filepath = %s\n", filepath);
				system(filepath);
				// create file name
				sprintf(filepath, "%s/%s", bundle_dir, filename);
				if ((debug) && (debug_level > 0))
					printf("[debug] filepath = %s\n", filepath);
				// bundle name is the name of the bundle payload file
				buffer = payload.buf.buf_val;
				//bufsize = payload.buf.buf_len;
				if ((debug) && (debug_level > 0))
				{
					printf ("======================================\n");
					printf (" Bundle received at %s\n", ctime(&current));
					printf ("  source: %s\n", spec.source.uri);
					if (use_file)
					{
						printf ("  saved into    : %s\n", filepath);
					}
					printf ("--------------------------------------\n");
				}

				if (pl_type == DTN_PAYLOAD_FILE)
				{ // if bundle was saved into file
					if ((receiving_file = open(payload.filename.filename_val, O_RDONLY)) < 0)
					{
						fprintf(stderr, "fatal error opening file %s\n", payload.filename.filename_val);
						exit(1);
					}
				}


				if (pl_type == DTN_PAYLOAD_FILE)
				{
					while (total_written < (long)len)
					{
						reads = read(receiving_file, temp, 1024);
						written = write(destination_file, temp, reads);
						total_written += written;
						lseek(receiving_file, total_written, 0);
					}

				}
				else
				{
					while (total_written < (long)len)
					{
						written = write(destination_file, payload.buf.buf_val, payload.buf.buf_len );
						total_written += written;
					}

				}

				total_saved += total_written;
				total_written = 0;

				printf("transfered %lu of %lu\n", total_saved, receiving_fileLen);

				if (pl_type == DTN_PAYLOAD_FILE)
				{ // if bundle was saved into file
					if ((debug) && (debug_level > 0))
						printf("[debug] renaming file %s -> %s...", payload.filename.filename_val, filepath);
					if (move_file(payload.filename.filename_val, filepath) != 0)
					{
						printf("[ERROR] Couldn't rename %s -> %s: %s\n", payload.filename.filename_val, filepath, strerror(errno));
					}

				}
				else
				{ // if bundle was saved into memory

					if ((debug) && (debug_level > 0))
					{
						for (k = 0; k < (int)payload.buf.buf_len; k++)
						{
							if (buffer[k] >= ' ' && buffer[k] <= '~')
								s_buffer[k % BUFSIZE] = buffer[k];
							else
								s_buffer[k % BUFSIZE] = '.';

							if (k % BUFSIZE == 0) // new line every 16 bytes
							{
								printf("%07x ", k);
							}
							else if (k % 2 == 0)
							{
								printf(" "); // space every 2 bytes
							}

							printf("%02x", buffer[k] & 0xff);

							// print character summary (a la emacs hexl-mode)
							if (k % BUFSIZE == BUFSIZE - 1)
							{
								printf(" |  %.*s\n", BUFSIZE, s_buffer);
							}
						} // for

						// print spaces to fill out the rest of the line
						if (k % BUFSIZE != BUFSIZE - 1)
						{
							while (k % BUFSIZE != BUFSIZE - 1)
							{
								if (k % 2 == 0)
								{
									printf(" ");
								}
								printf("  ");
								k++;
							}
							printf("   |  %.*s\n", (int)payload.buf.buf_len % BUFSIZE, s_buffer);
						} // if

						printf ("======================================\n");

					} // if (debug)

					free(filepath);
					free(source_eid);
					free(dest_eid);
				}
				close(receiving_file);
				fflush(stdout);
			}
			total_saved = 0;
			close(destination_file);
			receiving_file = -1;
			printf("completed tranfer of file %s\n\n", receiving_fileName);
			receiving_fileName = NULL;
		} //end receiving file

	} // while(1)

	// if this was ever changed to gracefully shutdown, it would be good to call:
	dtn_close(handle);

	return 0;

} //main

/* -------------------------------
 *        Utility Functions
 * ------------------------------- */

/* -------------
 *  print_usage
 * ------------- */
void print_usage(char* progname)
{
	fprintf(stderr, "\n");
	fprintf(stderr, "SYNTAX: %s [options]\n", progname);
	fprintf(stderr, "\n");
	fprintf(stderr, "options:\n");
	fprintf(stderr, " -d, --ddir <dir>\tDestination directory (if using -f), if dir is not indicated assume dir=%s.\n", BUNDLE_DIR_DEFAULT);
	fprintf(stderr, " -D, --debug <level>\tDebug messages [0-1], if level is not indicated assume level=0.\n");
	fprintf(stderr, " -m, --memory\tSave received bundles into memory.\n");
	fprintf(stderr, " -v    verbose\tSame as -D 0.\n");
	fprintf(stderr, " -a, --aggregate <n>\tPrint message every n arrivals.\n");
	fprintf(stderr, " -h, --help\tHelp.\n");
	fprintf(stderr, "\n");
	exit(1);
}

/* ---------------
 *  parse_options
 * --------------- */
void parse_options (int argc, char** argv)
{
	char c, done = 0;

	while (!done)
	{

		static struct option long_options[] =
		    {
			    {"ddir", required_argument, 0, 'd'
			    },
			    {"endpoint", required_argument, 0, 'e'},
			    {"memory", no_argument, 0, 'm'},
			    {"help", no_argument, 0, 'h'},
			    {"debug", required_argument, 0, 'D'},
			    {"aggregate", required_argument, 0, 'a'},
                {"api_IP", optional_argument, 0, 'A'},    
                {"api_port", optional_argument, 0, 'B'},
		    };

		int option_index = 0;
		c = getopt_long(argc, argv, "A:B:hvD::fmd:e:a:", long_options, &option_index);

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
		case 'h':            // show help
			print_usage(argv[0]);
			exit(1);
			return ;
		case 'v':            // debug mode
			debug = 1;
			break;
		case 'D':            // debug messages
			debug = 1;
			if (optarg != NULL)
				debug_level = atoi(optarg);
			break;
		case 'm':            // save received bundles into memory
			use_file = 0;
			break;
		case 'd':            // destination directory
			bundle_dir = optarg;
			break;

		case 'e':            // destination endpoint
			endpoint = optarg;
			break;

		case 'a':            // aggregate
			aggregate = atoi(optarg);
			break;

		case '?':
			break;

		case (char)(-1):
			done = 1;
			break;
		default:
			print_usage(argv[0]);
			exit(1);
		}
	}

#define CHECK_SET(_arg, _what)                                          \
    if (_arg == 0) {                                                    \
        fprintf(stderr, "\nSYNTAX ERROR: %s must be specified\n", _what);      \
        print_usage(argv[0]);                                                  \
        exit(1);                                                        \
    }

}

/* ----------------------------
 * parse_tuple
 * ---------------------------- */
dtn_endpoint_id_t* parse_eid(dtn_handle_t handle, dtn_endpoint_id_t* eid, char * str)
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
		if (debug)
			fprintf(stdout, "%s (local)\n", str);
		return eid;
	}
	else
	{
		fprintf(stderr, "invalid eid string '%s'\n", str);
		exit(1);
	}
} // end parse_tuple


int ContainsChar(char* string, char c)
{
	int i;
	for (i = strlen(string); i >= 0; i--)
	{
		if (string[i] == c)
			return i;
	}
	return -1;
}


char* GetFileName(char* string, int position)
{
	char temp[1024];
	char* res;
	strcpy(temp, string);
	temp[position] = '\0';
	res = strdup(temp);
	return res;
}


long GetFileLen(char* string, int position)
{
	int i;
	char temp[1024];
	strcpy(temp, string);
	for (i = (position + 1); i <= (int)(strlen(temp) + 1); i++)
	{
		temp[i - (position + 1)] = temp[i];
	}
	return atol(temp);
}



/* --------------------------------------------------
 * move_file
 * -------------------------------------------------- */
 
int move_file (char *fileIn, char *fileOut) {
    int rd, result, fdIn, fdOut;
    char buffer[MAXSIZE];
    
    result = rename(fileIn, fileOut);
    
    if (result < 0 && errno == EXDEV) {
   		
      if ((fdIn = open(fileIn, O_RDONLY)) < 0) {
			  fprintf(stderr, "fatal error opening file %s\n", fileIn);
			  exit(1);
		  }
		
		  if ((fdOut = open(fileOut, O_CREAT | O_TRUNC | O_WRONLY | O_APPEND, 0666)) < 0) {
			  fprintf(stderr, "fatal error opening file %s\n", fileOut);
			  exit(1);
		  }
		  
		  rd = 0;
		  
		  while ((rd = read(fdIn, buffer, MAXSIZE)) > 0) {
		    write(fdOut, buffer, rd);
		  }
		
		  close(fdIn);
		  close(fdOut);
		  
		  if (remove(fileIn) < 0)
        fprintf(stderr, "fatal error deleting file %s\n", fileIn);
		  
		  return 0;
    }
    
    return result;
    
} // end move_file
