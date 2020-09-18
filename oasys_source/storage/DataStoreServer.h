/* 
 * Copyright 2007 BBN Technologies Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <string>
#include <vector>

#include "DS.h"
#include "DataStore.h"

//
// DataStoreServer class
//
// A DataStore that can process (eval), marshal, unmarshal, process,
// send, and receive messags. 

// You can implement the control logic in one of three ways:
//
// (a) Call serve(istream, ostream), which will pull requests from
// istream, process them, and send replies to ostream.
//
// (b) Write your own main loop, and at appropriate times call recv()
// to pull a request from an input stream, process() to process the 
// request, and send() to send the reply.
//
// (c) Write your own main loop and own I/O routines, and call
// unmarshal() to convert XML to a request, process() to process the
// request, and marshal() to convert the response to XML.
// 

namespace oasys {

class XercesXMLUnmarshal;

class DataStoreServer: public Logger {
public:

    DataStoreServer(DataStore &ds,
                    const char *schema,
                    const char *logpath, 
                    const char *specific_class = "DataStoreServer");
    virtual ~DataStoreServer();

    // unmarshal an XML string into a request
    // returns 0 on success
    int unmarshal(std::string &xml, ds_request_type_p &req);

    // marshal a reply into an XML string
    // returns 0 on success, sets the string to the XML
    int marshal(dsmessage::ds_reply_type &reply, std::string &xml);

    // process a ds_request_type and get back a ds_reply_type.
    // returns 0 on success
    int process(/* IN */ dsmessage::ds_request_type &, 
                /* OUT */ ds_reply_type_p &);

    //-------------------------------------------------------------
    // iostream code
    //-------------------------------------------------------------

    // serve requests from istream, send responses to ostream
    // loops until EOF on istream or ostream
    int serve(std::istream &, std::ostream &);

    // pull a request from the input stream, unmarshal it.
    // returns 0 on success
    int recv(std::istream &is, ds_request_type_p &req);

    // marshal a reply, send it to the output stream.
    // returns 0 on success
    int send(std::ostream &os, dsmessage::ds_reply_type &reply);


    //-------------------------------------------------------------
    // file descriptor code
    //-------------------------------------------------------------

    // serve requests from infd, send responses to outfd
    // loops until EOF on infd or outfd
    int serve(int infd, int outfd);

    // pull a request from an fd, unmarshal it.
    // returns 0 on success
    int recv(int fd, ds_request_type_p &req);

    // marshal a reply, send it to an fd.
    // returns 0 on success
    int send(int fd, dsmessage::ds_reply_type &reply);

protected:
    std::string schema_;

    oasys::XercesXMLUnmarshal *parser_;

    DataStore &ds_;

};
}
