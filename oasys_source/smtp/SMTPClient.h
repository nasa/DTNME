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

#ifndef _OASYS_SMTP_CLIENT_H_
#define _OASYS_SMTP_CLIENT_H_

#include "../io/FdIOClient.h"
#include "../io/TCPClient.h"
#include "SMTP.h"

namespace oasys {

/**
 * Class to wrap a client socket to an smtp server.
 */
class SMTPClient : public TCPClient {
public:
    /// Default constructor
    SMTPClient(const char* logpath = "/oasys/smtp/client");

    /// Send a message using the SMTPSender interface. Returns 0 on
    /// success, an SMTP error code on failure.
    int send_message(SMTPSender* sender);
    
protected:
    BufferedInput  in_;
    BufferedOutput out_;
    SMTP           smtp_;
    bool           first_session_;
};

/**
 * Debugging class to provide basically the same functionality only
 * using a pair of file descriptors, not a socket
 */
class SMTPFdClient {
public:
    /// Default constructor
    SMTPFdClient(int fd_in, int fd_out,
                 const char* logpath = "/oasys/smtp/client");
     
    /// Send a message using the SMTPSender interface. Returns 0 on
    /// success, an SMTP error code on failure.
    int send_message(SMTPSender* sender);
     
protected:
    FdIOClient     fdio_in_;
    FdIOClient     fdio_out_;
    BufferedInput  in_;
    BufferedOutput out_;
    SMTP           smtp_;
    bool           first_session_;
};


} // namespace oasys

#endif /* _OASYS_SMTP_CLIENT_H_ */
