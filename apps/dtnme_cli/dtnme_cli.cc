/*
 *    Modifications made to this file are Copyright 2023 
 *    United States Government as represented by NASA Marshall Space Flight Center.
 *    All Rights Reserved.
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

//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <cstdlib>
#include <iostream>
#include <string>
#include <map>

using tcp = boost::asio::ip::tcp;               // from <boost/asio/ip/tcp.hpp>
namespace ssl = boost::asio::ssl;               // from <boost/asio/ssl.hpp>
namespace websocket = boost::beast::websocket;  // from <boost/beast/websocket.hpp>
namespace pt = boost::property_tree;            // from <boost/property_tree/ptree.hpp>

std::map<std::string, std::string> speed_dial_commands;


void usage()
{
    std::cerr <<
        "Usage: dtnme_cli <host> <port> <certificate_authority_path>\n" <<
        "Example:\n" <<
        "    dtnme_cli 127.0.0.1 5050 ./certs \n";

    std::cerr << "\n"
        "Local commands:\n" <<
        "  h or ?           : show this help\n" <<
        "  q, quit or exit  : disconnect from DTNME server\n" <<
        "  <#>=<command>    : assign <command> string to single digit <#>\n" <<
        "                       example: 0=bundle stats\n" <<
        "  <#>              : invoke command assigned to single digit <#>\n" <<
        "                       example: 0 will execute the command \"bundle stats\" \n";

    std::cerr << "\n"
        "Current \"speed dial\" commands (stored in file ~/.dtnme_cli_rc:\n";

    for (int ix=0; ix<10; ++ix)
    {
        std::string key = std::to_string(ix);
        std::string cmd = speed_dial_commands[key];

        std::cerr << "       " << ix << " = " << cmd << std::endl;
    }

    std::cerr << std::endl << std::endl << std::endl;
}

void save_speed_dial_commnds()
{
    std::string home_dir = std::getenv("HOME");
    std::string file_path = home_dir + "/.dtnme_cli_rc";

    pt::ptree tree;

    for (int ix=0; ix<10; ++ix)
    {
        std::string key = std::to_string(ix);
        std::string cmd = speed_dial_commands[key];

        tree.put(key, cmd);
    }

    pt::write_ini(file_path, tree);
}

void load_speed_dial_commnds()
{
    std::string home_dir = std::getenv("HOME");
    std::string file_path = home_dir + "/.dtnme_cli_rc";

    if (!boost::filesystem::exists(file_path))
    {
        save_speed_dial_commnds();

    }

    pt::ptree tree;
    pt::read_ini(file_path, tree);

    for (int ix=0; ix<10; ++ix)
    {
        std::string key = std::to_string(ix);
        std::string cmd = tree.get(key, "");

        speed_dial_commands[key] = cmd;
    }
}

// Sends a WebSocket message and prints the response
int main(int argc, char** argv)
{
    try
    {
        // Check command line arguments.
        if(argc != 4)
        {
            usage();
            return EXIT_FAILURE;
        }
        auto const host = argv[1];
        auto const port = argv[2];


        load_speed_dial_commnds();

        usage();

        // The io_context is required for all I/O
        boost::asio::io_context ioc;

        // The SSL context is required, and holds certificates
        ssl::context ctx{ssl::context::tlsv12_client};

        // This holds the root certificate used for verification
        //ctx.load_verify_file(argv[3]);
        boost::system::error_code ec;
        ctx.load_verify_file(argv[3],ec);
        if(ec){
            std::cerr << ec.message() << std::endl;
            return EXIT_FAILURE;
        }

        //load_root_certificates(ctx);

        // These objects perform our I/O
        tcp::resolver resolver{ioc};
        websocket::stream<ssl::stream<tcp::socket>> ws{ioc, ctx};
        //websocket::stream<tcp::socket> ws{ioc};

        // Look up the domain name
        auto const results = resolver.resolve(host, port);

        // Make the connection on the IP address we get from a lookup
        boost::asio::connect(ws.next_layer().next_layer(), results.begin(), results.end());
        //boost::asio::connect(ws.next_layer(), results.begin(), results.end());

        // Perform the SSL handshake
        ws.next_layer().handshake(ssl::stream_base::client);

        // Perform the websocket handshake
        ws.handshake(host, "/");

        while(true){
            std::string command;

            std::cout << "dtnme% ";

            std::getline(std::cin,command);

            std::string test_command = boost::to_upper_copy<std::string>(command);

            if(test_command == "QUIT" || test_command == "Q" || test_command == "EXIT"){
                break;
            }

            if (command.size() == 1) 
            {
                if (isdigit(command.at(0)))
                {
                    // replace input command with the speed dial command
                    std::string key = command;
                    command = speed_dial_commands[key];
                    if (command.size() == 0) {
                        command = " ";
                    }

                    std::cout << std::endl << "Cmd #" << key << " = " << command << std::endl;
                }
                else if (!command.compare("h") || !command.compare("?"))
                {
                    usage();
                    continue;
                }
            } 
            else if (command.size() >= 2)
            {
                if (isdigit(command.at(0)) && (command.at(1) == '='))
                {
                    std::string key = command.substr(0, 1);
                    std::string new_command = command.substr(2);
                    speed_dial_commands[key] = new_command;
                    save_speed_dial_commnds();

                    std::cerr <<
                        "assigned \"speed dial\" command #" <<
                        key << " = " << new_command << std::endl;
                    continue;
                }
            }


            // Send the message
            ws.write(boost::asio::buffer(command));

            // This buffer will hold the incoming message
            boost::beast::multi_buffer buffer;

            // Read a message into our buffer
            ws.read(buffer);

            // The buffers() function helps print a ConstBufferSequence
            //std::cout << "\n" << boost::beast::buffers(buffer.data()) << "\n\n";
            std::string result = boost::beast::buffers_to_string(buffer.data()).c_str();
            std::cout << "\n" << result << "\n\n";

            if (result.find("shutdown initiated") != std::string::npos)
            {
                break;
            }
        }

        // Close the WebSocket connection
        ws.close(websocket::close_code::normal);

        // If we get here then the connection is closed gracefully

        
    }
    catch(std::exception const& e)
    {
        if(std::strstr(e.what(),"truncated")){
            return EXIT_SUCCESS;
        }
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
