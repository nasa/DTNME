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

#include <ctype.h>

#include "SMTP.h"

namespace oasys {

const char* SMTP::nl_ = "\r\n";
SMTP::Config SMTP::DEFAULT_CONFIG;

//----------------------------------------------------------------------------
SMTP::SMTP(oasys::BufferedInput*  in,
           oasys::BufferedOutput* out,
           const Config&          config,
           const char*            logpath)
    : Logger("SMTP", "%s", logpath),
      in_(in), 
      out_(out),
      config_(config)
{
    ASSERT(in_);
    ASSERT(out_);
    
    in_->logpathf("%s/in", logpath);
    out_->logpathf("%s/out", logpath);
}

//----------------------------------------------------------------------------
int
SMTP::client_session(SMTPSender* sender, bool first_session)
{
    int err;

    std::string domain;
    std::string from;
    std::vector<std::string> to;
    std::string received;
    const std::string* message;

    if (first_session) {
        // handle the initial message
        if ((err = process_response(220)) != 0) return err;
        
        sender->get_HELO_domain(&domain);
        out_->printf("HELO %s\r\n", domain.c_str());
        if ((err = process_response(250)) != 0) return err;
    }

    sender->get_MAIL_from(&from);
    out_->printf("MAIL FROM: %s\r\n", from.c_str());
    if ((err = process_response(250)) != 0) return err;
        
    sender->get_RCPT_list(&to);
    for (size_t i = 0; i < to.size(); ++i) {
        out_->printf("RCPT TO: %s\r\n", to[i].c_str());
        if ((err = process_response(250)) != 0) return err;
    }
        
    out_->printf("DATA\r\n");
    if ((err = process_response(354)) != 0) return err;

    sender->get_RECEIVED(&received);
    sender->get_DATA(&message);
    size_t start = 0, end = 0;

    if (received.length() != 0) {
        out_->write(received.data(), received.length());
    }
    
    while (1) {
        end = message->find_first_of("\r\n", start);

        if (end == std::string::npos) {
            end = message->length();
        }

        const char* bp = message->data() + start;
        if (*bp == '.') {
            out_->write("."); // escape
        }
        if (end != start) { // handle blank lines
            out_->write(bp, end - start);
        }
        out_->write("\r\n");

        if (end == message->length()) {
            break;
        }

        start = end + 1;
        if ((*message)[start] == '\n') {
            ++start;
        }

        if (start == message->length()) {
            break;
         }
    }

    out_->write(".\r\n");
    out_->flush();

    if ((err = process_response(250)) != 0) return err;

    return 0;
}

//----------------------------------------------------------------------------
int
SMTP::server_session(oasys::SMTPHandler* handler)
{
    int err = send_signon();
    if (err < 0) {
        log_warn("disconnecting: couldn't send sign on message");
        return err;
    }

    while (true) {
        int resp = process_cmd(handler);

        if (resp > 0) {
            err = send_response(resp);
            if (err < 0) {
                log_warn("disconnecting: couldn't send response");
            }
            if (resp == 221) {
                log_info("quit SMTP session");
                break;
            }
        } else if (resp == 0) {
            log_info("disconnecting: SMTP session on eof");
            break;
        } else {
            log_warn("disconnecting: SMTP session on unexpected error");
            break;
        }
    }
    return err;
}

//----------------------------------------------------------------------------
int
SMTP::send_signon()
{
    return send_response(220);
}

//----------------------------------------------------------------------------
int
SMTP::process_cmd(SMTPHandler* handler)
{
    char* line;
    int cc = in_->read_line(nl_, &line, config_.timeout_);

    if (cc < 0) {
        log_warn("got error %d, disconnecting", cc);
        return -1;
    } else if (cc == 0) {
        log_info("got eof from connection");
        return 0;
    }

    char cmd[5];
    log_debug("read cc=%d", cc);
    if (cc < 4) {
        log_info("garbage input command");
        return 500;
    }

    ASSERT(line[cc - strlen(nl_)] == nl_[0]);
    line[cc - strlen(nl_)] = '\0';    // null terminate the input line

    memcpy(cmd, line, 4);
    cmd[4] = '\0';

    if (false) {} // symmetry
    else if (strcasecmp(cmd, "HELO") == 0)
    {
        if (line[4] != ' ') {
            return 501;
        }

#define SKIP_WS(_var)                           \
        while (1) {                             \
            if (*_var == '\0') {                \
                return 501;                     \
            }                                   \
            if (*_var != ' ') {                 \
                break;                          \
            }                                   \
            ++_var;                             \
        }

        char* domain = &line[5];
        SKIP_WS(domain);
        return handler->smtp_HELO(domain);
    }
    else if (strcasecmp(cmd, "MAIL") == 0)
    {
        if (strncasecmp(line, "MAIL FROM:", 10) != 0) {
            return 501;
        }
        
        char* from = &line[10];
        SKIP_WS(from);
        return handler->smtp_MAIL(from);
    }
    else if (strcasecmp(cmd, "RCPT") == 0)
    {
        if (strncasecmp(line, "RCPT TO:", 8) != 0) {
            return 501;
        }

        char* to = &line[8];
        SKIP_WS(to);
        return handler->smtp_RCPT(to);
    }
    else if (strcasecmp(cmd, "DATA") == 0)
    {
        int err = handler->smtp_DATA_begin();
        if (err != 0) {
            return err;
        }

        // send waiting for mail message
        send_response(354);

        while (true) {
            char* mail_line;
            int cc = in_->read_line(nl_, &mail_line, config_.timeout_);
            if (cc <= 0) {
                log_warn("got error %d, disconnecting", cc);
                return -1;
            }

            ASSERT(cc >= static_cast<int>(strlen(nl_)));
            ASSERT(mail_line[cc - strlen(nl_)] == nl_[0]);
            mail_line[cc - strlen(nl_)] = '\0';

            // check for escaped . or end of message (. on a line by itself)
            if (mail_line[0] == '.') {
                if (strlen(mail_line) == 1) {
                    break;
                }
                mail_line += 1;
            }

            int err = handler->smtp_DATA_line(mail_line);
            if (err != 0) {
                return err;
            }
        }

        return handler->smtp_DATA_end();
    }
    else if (strcasecmp(cmd, "RSET") == 0)
    {
        return handler->smtp_RSET();
    }
    else if (strcasecmp(cmd, "NOOP") == 0)
    {
        return 220;
    }
    else if (strcasecmp(cmd, "QUIT") == 0)
    {
        handler->smtp_QUIT();
        return 221;
    }
    else if (strcasecmp(cmd, "TURN") == 0 ||
             strcasecmp(cmd, "SEND") == 0 ||
             strcasecmp(cmd, "SOML") == 0 ||
             strcasecmp(cmd, "SAML") == 0 ||
             strcasecmp(cmd, "VRFY") == 0 ||
             strcasecmp(cmd, "EXPN") == 0 ||
             strcasecmp(cmd, "EHLO") == 0)
    {
        return 502;
    }

    return 500;
}

//----------------------------------------------------------------------------
int
SMTP::process_response(int expected_code)
{
    char* line;
    int cc = in_->read_line(nl_, &line, config_.timeout_);

    if (cc < 0) {
        log_warn("got error %d, disconnecting", cc);
        return -1;
    } else if (cc == 0) {
        log_info("got eof from connection");
        return 221;
    }

    log_debug("read cc=%d", cc);
    
    if (cc < 3) {
        log_info("garbage response");
        return 500;
    }

    char buf[4];
    memcpy(buf, line, 3);
    buf[3] = '\0';

    char* end;
    int code = strtoul(buf, &end, 10);
    if (end != &buf[3]) {
        log_info("garbage code value %s", buf);
        return 501;
    }

    if (code != expected_code) {
        log_info("code %d != expected %d", code, expected_code);
        return 503;
    }

    log_debug("OK: %s", line);

    return 0;
}

//----------------------------------------------------------------------------
int
SMTP::send_response(int code)
{
    int err = out_->format_buf("%d ", code);
    if (err < 0) return err;
    return out_->printf(response_code(code), config_.domain_.c_str());
}

//----------------------------------------------------------------------------
const char*
SMTP::response_code(int code) const
{
    switch (code) {
    case 211: return "System status, or system help reply\r\n";
    case 214: return "Help message\r\n";
    case 220: return "%s Service ready\r\n";
    case 221: return "%s Service closing transmission channel\r\n";
    case 250: return "Requested mail action okay, completed\r\n";
    case 251: return "User not local; will forward to nowhere.net\r\n";
    case 354: return "Start mail input; end with <CRLF>.<CRLF>\r\n";
    case 421: return "tierstore Service not available, "
                     "closing transmission channel\r\n";
    case 450: return "Requested mail action not taken: mailbox unavailable\r\n";
    case 451: return "Requested action aborted: local error in processing\r\n";
    case 452: return "Requested action not taken: insufficient system storage\r\n";
    case 500: return "Syntax error, command unrecognized\r\n";
    case 501: return "Syntax error in parameters or arguments\r\n";
    case 502: return "Command not implemented\r\n";
    case 503: return "Bad sequence of commands\r\n";
    case 504: return "Command parameter not implemented\r\n";
    case 550: return "Requested action not taken: mailbox unavailable\r\n";
    case 551: return "User not local; please try nowhere.net\r\n";
    case 552: return "Requested mail action aborted: exceeded "
                     "storage allocation\r\n";
    case 553: return "Requested action not taken: mailbox name not allowed\r\n";
    case 554: return "Transaction failed\r\n";
    default:  return 0;
    }
}

} // namespace oasys
