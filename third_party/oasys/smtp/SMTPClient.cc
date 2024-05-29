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

#ifdef HAVE_CONFIG_H
#  include <oasys-config.h>
#endif

#include "SMTPClient.h"

namespace oasys {

SMTPClient::SMTPClient(const char* logpath)
    : TCPClient(logpath, true /* init_socket_immediately */ ),
      in_(this), out_(this),
      smtp_(&in_, &out_, SMTP::DEFAULT_CONFIG, logpath),
      first_session_(true)
{
}

int
SMTPClient::send_message(SMTPSender* sender)
{
    int ret = smtp_.client_session(sender, first_session_);
    first_session_ = false;
    return ret;
}

SMTPFdClient::SMTPFdClient(int fd_in, int fd_out, const char* logpath)
    : fdio_in_(fd_in), fdio_out_(fd_out),
      in_(&fdio_in_), out_(&fdio_out_),
      smtp_(&in_, &out_, SMTP::DEFAULT_CONFIG, logpath),
      first_session_(true)
{
}

int
SMTPFdClient::send_message(SMTPSender* sender)
{
    int ret = smtp_.client_session(sender, first_session_);
    first_session_ = false;
    return ret;
}

} // namespace oasys
