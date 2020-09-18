/*
 * Copyright 2007 BBN Technologies Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License. You
 * may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied.
 */

/*
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#ifdef BSP_ENABLED

#include <cstring>
#include <cstdio>
#include <oasys/util/Base16.h>

#include "SecurityCommand.h"
#include "security/KeyDB.h"
#include "security/Ciphersuite.h"


namespace dtn {

SecurityCommand::SecurityCommand()
    : TclCommand("security")
{
    add_to_help("setkey <host> <cs_num> <key>",
                "Set the symmetric key to use for the specified host and ciphersuite\n"
                "number.  <host> may also be the wildcard symbol \"*\".");
#ifdef LTPUDP_AUTH_ENABLED
    add_to_help("setkeyltp <engine> <cs_num> <key> <key_id>",
                "Currently valid only for ciphersuite 0 "
                "Set the symmetric key to use for the specified engine and ciphersuite key_id for LTP\n"
                "number.<key_id> is an integer value  <engine> is an integer indicating the engine id for the key set.");
#endif
    add_to_help("dumpkeys",
                "Dump the symmetric keys to the screen.");
    add_to_help("flushkeys",
                "Erase all the symmetric keys.");
    add_to_help("certdir <directory>",
                "Set the directory where RSA/EC certificates are stored");
    add_to_help("privdir <directory>",
                "Set the directory where RSA private keys are stored");
    add_to_help("keypaths [ pub_keys_enc | priv_keys_dec | priv_keys_sig | pub_keys_ver ] <csnum> <filename> ",
                "Set the filenames for RSA/EC private keys and public key certificates. <NODE> may be used\n"
    		    "in the filename and will be replaced with the EID.");
    add_to_help("listpaths",
                "List the paths for the RSA private keys and public key certificates");
    add_to_help("outgoing add <src id pattern> <dest id pattern> <sec dest> <csnum>",
    		    "Add an outgoing rule");
    add_to_help("outgoing reset",
    		    "Turn off all outgoing BSP");
    add_to_help("incoming add <src id pattern> <dest id pattern> <sec src pattern> <sec dest pattern> <csnum> [csnum] [csnum] ...",
    		    "Add an incoming rule.");
    add_to_help("outgoing del <rulenum>",
                "Delete an outgoing rule");
    add_to_help("incoming del <rulenum>",
                "Delete an incoming rule");
    add_to_help("incoming reset",
    		    "Don't require any incoming BSP");
    add_to_help("listpolicy",
    		    "Display infomation on the incoming and outgoing ciphersuites");
}

int
SecurityCommand::exec(int argc, const char** argv, Tcl_Interp* interp)
{
    (void)interp;

    if (argc < 2) {
        resultf("need a security subcommand");
        return TCL_ERROR;
    }

    const char* cmd = argv[1];

   if(strcmp(cmd, "outgoing") == 0) {
       if(argc < 3) {
           wrong_num_args(argc, argv, 2, 3, 7);
           return TCL_ERROR;
       }
       if(strcmp(argv[2], "reset") == 0) {
           Ciphersuite::config->outgoing.clear();
           return TCL_OK;
       }
       if(strcmp(argv[2], "del") == 0) {
           if(argc != 4) {
               wrong_num_args(argc, argv, 3, 4, 4);
               return TCL_ERROR;
            }
            int num = atoi(argv[3]);
            Ciphersuite::config->outgoing.erase(Ciphersuite::config->outgoing.begin()+num);
            return TCL_OK;
       }
       if(strcmp(argv[2], "add") == 0) {
           if(argc != 7) {
               wrong_num_args(argc, argv, 3, 7, 7);
               return TCL_ERROR;
           }
            OutgoingRule rule;
            rule.src = EndpointIDPattern(argv[3]);
            rule.dest = EndpointIDPattern(argv[4]);
            rule.secdest = EndpointIDNULL(string(argv[5]));
            rule.csnum = atoi(argv[6]);
            Ciphersuite::config->outgoing.push_back(rule);
            return TCL_OK;
       }
       resultf("subcommand must be one of add, del, reset");
       return TCL_ERROR;
    } else if(strcmp(cmd, "incoming") == 0) {
       if(argc < 3) {
           wrong_num_args(argc, argv, 2, 3, 7);
           return TCL_ERROR;
       }
       if(strcmp(argv[2], "reset") == 0) {
           Ciphersuite::config->incoming.clear();
           return TCL_OK;
       }
       if(strcmp(argv[2], "del") == 0) {
           if(argc != 4) {
               wrong_num_args(argc, argv, 3, 4, 4);
               return TCL_ERROR;
            }
            int num = atoi(argv[3]);
            Ciphersuite::config->incoming.erase(Ciphersuite::config->incoming.begin()+num);
            return TCL_OK;
       }
       if(strcmp(argv[2], "add") == 0) {
           if(argc < 7) {
               wrong_num_args(argc, argv, 3, 8, 0);
               return TCL_ERROR;
           }
            IncomingRule rule;
            rule.src = EndpointIDPattern(argv[3]);
            rule.dest = EndpointIDPattern(argv[4]);
            rule.secsrc = EndpointIDPatternNULL(string(argv[5]));
            rule.secdest = EndpointIDPatternNULL(string(argv[6]));
            set<int> csnums;
            for(int i=7;i<argc;i++) {
                csnums.insert(atoi(argv[i]));
            }
            rule.csnums = csnums;
            Ciphersuite::config->incoming.push_back(rule);
            return TCL_OK;
       }
       resultf("subcommand must be one of add, del, reset");
       return TCL_ERROR;
 
    } else if(strcmp(cmd, "listpolicy") == 0) {
        set_result(Ciphersuite::config->list_policy().c_str());

    } else if (strcmp(cmd, "setkey") == 0) {
        // security setkey <host> <cs_num> <key>
        if (argc != 5) {
            wrong_num_args(argc, argv, 2, 5, 5);
            return TCL_ERROR;
        }

        const char* host_arg   = argv[2];
        const char* cs_num_arg = argv[3];
        const char* key_arg    = argv[4];

        int cs_num;
        if (sscanf(cs_num_arg, "%i", &cs_num) != 1) {
            resultf("invalid cs_num argument \"%s\"", cs_num_arg);
            return TCL_ERROR;
        }
        if (! KeyDB::validate_cs_num(cs_num)) {
            resultf("invalid ciphersuite number %#x", cs_num);
            return TCL_ERROR;
        }

        size_t key_arg_len = strlen(key_arg);
        if ((key_arg_len % 2) != 0) {
            resultf("invalid key argument (must be even length)");
            return TCL_ERROR;
        }

        size_t key_len = key_arg_len / 2;
        if (! KeyDB::validate_key_len(cs_num, &key_len)) {
            resultf("wrong key length for ciphersuite (expected %d bytes)",
                    (int)key_len);
            return TCL_ERROR;
        }
            
        // convert key from ASCII hexadecimal to raw bytes
        u_char* key = new u_char[key_len];
        for (int i = 0; i < (int)key_len; i++)
        {
            int b = 0;

            for (int j = 0; j <= 1; j++)
            {
                int c = (int)key_arg[2*i+j];
                b <<= 4;

                if (c >= '0' && c <= '9')
                    b |= c - '0';
                else if (c >= 'A' && c <= 'F')
                    b |= c - 'A' + 10;
                else if (c >= 'a' && c <= 'f')
                    b |= c - 'a' + 10;
                else {
                    resultf("invalid character '%c' in key argument",
                            (char)c);
                    delete key;
                    return TCL_ERROR;
                }
            }
            
            key[i] = (u_char)(b);
        }

        KeyDB::Entry new_entry =
            KeyDB::Entry(host_arg, cs_num, key, key_len);
        KeyDB::set_key(new_entry);

        delete[] key;
        return TCL_OK;

    }
#ifdef LTPUDP_AUTH_ENABLED
    else if (strcmp(cmd, "setkeyltp") == 0) {
        string engine_override;
        // security setkey <engine_id> <cs_num> <key> <key_id>
        if (argc != 6) {
            wrong_num_args(argc, argv, 2, 6, 6);
            return TCL_ERROR;
        }
       
        const char* engine_arg = argv[2];
        const char* cs_num_arg = argv[3];
        const char* key_arg    = argv[4];

        int test_val = atoi(argv[2]);
        if (test_val == 0) {
            resultf("invalid engine argument \"%s\"", engine_arg);
            return TCL_ERROR;
        }

        for(int check = 0 ; check < (int) strlen(argv[2]); check++) {
            if(!isdigit(argv[2][check])) {
                resultf("invalid engine argument \"%s\"", argv[2]);
                return TCL_ERROR;
            }
        }
        for(int check = 0 ; check < (int) strlen(argv[5]); check++) {
            if(!isdigit(argv[5][check])) {
                resultf("invalid key_id argument \"%s\"", argv[5]);
                return TCL_ERROR;
            }
        }
        engine_override = std::string("LTP:") + std::string(argv[2]) + std::string(":") + std::string(argv[5]);
        engine_arg = engine_override.c_str();

        int cs_num;

        if (sscanf(cs_num_arg, "%i", &cs_num) != 1) {
            resultf("invalid cs_num argument \"%s\"", cs_num_arg);
            return TCL_ERROR;
        }

        if (cs_num != 0) {
            resultf("invalid cs_num (%d)", cs_num);
            return TCL_ERROR;
        }

        int key_arg_len = strlen(key_arg);
        if ((key_arg_len % 2) != 0) {
            resultf("invalid key argument (must be even length)");
            return TCL_ERROR;
        }

        size_t key_len = key_arg_len / 2;
        if(key_len != 20 && cs_num == 0) {
            resultf("wrong key length (%d) for ciphersuite (expected 20 bytes)",
                    (int)key_len);
            return TCL_ERROR;
        }
           
        // convert key from ASCII hexadecimal to raw bytes
        u_char* key = new u_char[key_len];
        for (int i = 0; i < (int)key_len; i++)
        {
            int b = 0;

            for (int j = 0; j <= 1; j++)
            {
                int c = (int)key_arg[2*i+j];
                b <<= 4;

                if (c >= '0' && c <= '9')
                    b |= c - '0';
                else if (c >= 'A' && c <= 'F')
                    b |= c - 'A' + 10;
                else if (c >= 'a' && c <= 'f')
                    b |= c - 'a' + 10;
                else {
                    resultf("invalid character '%c' in key argument",
                            (char)c);
                    delete key;
                    return TCL_ERROR;
                }
            }
            
            key[i] = (u_char)(b);
        }

        KeyDB::Entry new_entry =
            KeyDB::Entry(engine_arg, cs_num, key, key_len);
        KeyDB::set_key(new_entry);

        delete[] key;
        return TCL_OK;

    }
#endif
    else if (strcmp(cmd, "dumpkeys") == 0) {
        // security dumpkeys
        if (argc != 2) {
            wrong_num_args(argc, argv, 2, 2, 2);
            return TCL_ERROR;
        }

        oasys::StringBuffer buf;
        KeyDB::dump_header(&buf);
        KeyDB::dump(&buf);
        set_result(buf.c_str());
        return TCL_OK;

    } else if (strcmp(cmd, "flushkeys") == 0) {
        // security flushkeys
        if (argc != 2) {
            wrong_num_args(argc, argv, 2, 2, 2);
            return TCL_ERROR;
        }

        KeyDB::flush_keys();
    } else if (strcmp(cmd, "certdir") == 0) {
        if (argc != 3) {
            wrong_num_args(argc, argv, 1, 3, 3);
            return TCL_ERROR;
        }
        const char *loc = argv[2];
        Ciphersuite::config->certdir = string(loc);
    } else if (strcmp(cmd, "privdir") == 0) {
        if (argc != 3) {
            wrong_num_args(argc, argv, 1, 3, 3);
            return TCL_ERROR;
        }
        const char *loc = argv[2];
        Ciphersuite::config->privdir = string(loc);
    } else if(strcmp(cmd, "keypaths") == 0) {
        if(argc != 5) {
            wrong_num_args(argc, argv, 2, 5, 5);
            return TCL_ERROR;
        }

        const char *map = argv[2];
        int csnum = atoi(argv[3]);
        const char *filename = argv[4];
        /*
     	 map<int, string> pub_keys_enc;
    	 map<int, string> pub_keys_ver;
    	 map<int, string> priv_keys_sig;
    	 map<int, string> priv_keys_dec;
            */
        if(strcmp(map, "priv_keys_dec")==0) {
        	Ciphersuite::config->priv_keys_dec[csnum] = string(filename);
        } else if(strcmp(map, "pub_keys_ver")==0) {
        	Ciphersuite::config->pub_keys_ver[csnum] = string(filename);
        } else if(strcmp(map, "pub_keys_enc")==0) {
        	Ciphersuite::config->pub_keys_enc[csnum] = string(filename);
        } else if(strcmp(map, "priv_keys_sig") == 0) {
        	Ciphersuite::config->priv_keys_sig[csnum] = string(filename);
        } else{
        	resultf("no such path exists");
        }
    } else if(strcmp(cmd, "listpaths") == 0) {
        set_result(Ciphersuite::config->list_maps().c_str());
    } else {
        resultf("no such security subcommand %s", cmd);
        return TCL_ERROR;
    }

    return TCL_OK;
}

} // namespace dtn

#endif  /* BSP_ENABLED */
