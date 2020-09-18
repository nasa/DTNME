/*
 *    Copyright 2004-2006 Intel Corporation
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
#  include <dtn-config.h>
#endif

#include <sys/poll.h>
#include <stdlib.h>

#include <oasys/io/FileUtils.h>
#include <oasys/util/OptParser.h>
#include <oasys/util/HexDumpBuffer.h>

#include "SerialConvergenceLayer.h"
#include "bundling/BundleDaemon.h"
#include "contacts/ContactManager.h"

namespace dtn {

SerialConvergenceLayer::SerialLinkParams
    SerialConvergenceLayer::default_link_params_(true);

//----------------------------------------------------------------------
SerialConvergenceLayer::SerialLinkParams::SerialLinkParams(bool init_defaults)
    : StreamLinkParams(init_defaults),
      hexdump_(false),
      initstr_(""),
      ispeed_(19200),
      ospeed_(19200),
      sync_interval_(1000)
{
}

//----------------------------------------------------------------------
void
SerialConvergenceLayer::SerialLinkParams::serialize(oasys::SerializeAction *a)
{
	log_debug_p("SerialLinkParams", "SerialLinkParams::serialize");
	StreamConvergenceLayer::StreamLinkParams::serialize(a);
	a->process("hexdump", &hexdump_);
	a->process("initstr", &initstr_);
	a->process("ispeed", &ispeed_);
	a->process("ospeed", &ospeed_);
    a->process("sync_interval", &sync_interval_);
}

//----------------------------------------------------------------------
SerialConvergenceLayer::SerialConvergenceLayer()
    : StreamConvergenceLayer("SerialConvergenceLayer", "serial",
                             SERIALCL_VERSION)
{
}

//----------------------------------------------------------------------
CLInfo*
SerialConvergenceLayer::new_link_params()
{
    return new SerialLinkParams(default_link_params_);
}

//----------------------------------------------------------------------
bool
SerialConvergenceLayer::init_link(const LinkRef& link,
                                  int argc, const char* argv[])
{
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() == NULL);

    log_debug("adding %s link %s", link->type_str(), link->nexthop());

    // Create a new parameters structure, parse the options, and store
    // them in the link's cl info slot.
    SerialLinkParams* params = dynamic_cast<SerialLinkParams *>(new_link_params());
    ASSERT(params != NULL);

    // Try to parse the link's next hop, but continue on even if the
    // parse fails since the hostname may not be resolvable when we
    // initialize the link. Each subclass is responsible for
    // re-checking when opening the link.
    parse_nexthop(link, params);

    const char* invalid;
    if (! parse_link_params(params, argc, argv, &invalid)) {
        log_err("error parsing link options: invalid option '%s'", invalid);
        delete params;
        return false;
    }

	// Calls the StreamConvergenceLayer method
    if (! finish_init_link(link, params)) {
        log_err("error in finish_init_link");
        delete params;
        return false;
    }

    link->set_cl_info(params);

    return true;
}

//----------------------------------------------------------------------
bool
SerialConvergenceLayer::parse_link_params(LinkParams* lparams,
                                       int argc, const char** argv,
                                       const char** invalidp)
{
    SerialLinkParams* params = dynamic_cast<SerialLinkParams*>(lparams);
    ASSERT(params != NULL);

    oasys::OptParser p;
    
    p.addopt(new oasys::BoolOpt("hexdump", &params->hexdump_));
    p.addopt(new oasys::StringOpt("initstr", &params->initstr_));
    p.addopt(new oasys::UIntOpt("ispeed", &params->ispeed_));
    p.addopt(new oasys::UIntOpt("ospeed", &params->ospeed_));
    p.addopt(new oasys::UIntOpt("sync_interval", &params->sync_interval_));
    
    int count = p.parse_and_shift(argc, argv, invalidp);
    if (count == -1) {
        return false; // bogus value
    }
    argc -= count;
    
    // continue up to parse the parent class
    return StreamConvergenceLayer::parse_link_params(lparams, argc, argv,
                                                     invalidp);
}

//----------------------------------------------------------------------
void
SerialConvergenceLayer::dump_link(const LinkRef& link, oasys::StringBuffer* buf)
{
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() != NULL);

    StreamConvergenceLayer::dump_link(link, buf);
    
    SerialLinkParams* params = dynamic_cast<SerialLinkParams*>(link->cl_info());
    ASSERT(params != NULL);
    
    buf->appendf("hexdump: %s\n", params->hexdump_ ? "enabled" : "disabled");
    buf->appendf("initstr: %s\n", params->initstr_.c_str());
    buf->appendf("ispeed: %d bps\n", params->ispeed_);
    buf->appendf("ospeed: %d bps\n", params->ospeed_);
    buf->appendf("sync_interval: %d\n", params->sync_interval_);
}

//----------------------------------------------------------------------
bool
SerialConvergenceLayer::set_link_defaults(int argc, const char* argv[],
                                       const char** invalidp)
{
    return parse_link_params(&default_link_params_, argc, argv, invalidp);
}

//----------------------------------------------------------------------
bool
SerialConvergenceLayer::parse_nexthop(const LinkRef& link, LinkParams* lparams)
{
    SerialLinkParams* params = dynamic_cast<SerialLinkParams*>(lparams);
    ASSERT(params != NULL);

    if (! oasys::FileUtils::readable(link->nexthop()))
    {
        log_warn("can't read tty device file %s", link->nexthop());
        return false;
    }

    return true;
}

//----------------------------------------------------------------------
CLConnection*
SerialConvergenceLayer::new_connection(const LinkRef& link, LinkParams* p)
{
    SerialLinkParams* params = dynamic_cast<SerialLinkParams*>(p);
    ASSERT(params != NULL);
    return new Connection(this, link, params);
}

//----------------------------------------------------------------------
SerialConvergenceLayer::Connection::Connection(SerialConvergenceLayer* cl,
                                               const LinkRef&          link,
                                               SerialLinkParams*       params)
    : StreamConvergenceLayer::Connection("SerialConvergenceLayer::Connection",
                                         cl->logpath(), cl, params,
                                         true /* call connect() */)
{
    logpathf("%s/conn/%p", cl->logpath(), this);
    
    // set up the base class' nexthop parameter
    set_nexthop(link->nexthop());

    // the actual tty wrapper
    tty_ = new oasys::TTY(logpath_);
    tty_->logpathf("%s/tty", logpath_);

    synced_ = false;
}

//----------------------------------------------------------------------
SerialConvergenceLayer::Connection::~Connection()
{
    delete tty_;
}

//----------------------------------------------------------------------
void
SerialConvergenceLayer::Connection::serialize(oasys::SerializeAction *a)
{
    // XXX/demmer this should be fixed
    (void)a;
}

//----------------------------------------------------------------------
void
SerialConvergenceLayer::Connection::initialize_pollfds()
{
    // XXX/demmer maybe rename this hook to just "initialize"
    
    const LinkRef& link = contact_->link();
    
    // the first thing we do is try to parse the next hop address...
    // if we're unable to do so, the link can't be opened.
    if (! cl_->parse_nexthop(link, params_)) {
        log_info("can't resolve nexthop address '%s'", link->nexthop());
        break_contact(ContactEvent::BROKEN);
        return;
    }

    // open the tty
    int ret = tty_->open(link->nexthop(), O_RDWR | O_NOCTTY);
    if (ret == -1) {
        log_info("opening %s failed... %s", link->nexthop(), strerror(errno));
        break_contact(ContactEvent::BROKEN);
        return;
    }

    log_debug("opened %s", link->nexthop());
    if (!tty_->isatty()) {
        log_err("%s is not a TTY", link->nexthop());
        break_contact(ContactEvent::BROKEN);
        return;
    }

    log_debug("setting tty parameters...");
    tty_->tcgetattr();
    tty_->cfmakeraw();
    tty_->cfsetispeed(serial_lparams()->ispeed_);
    tty_->cfsetospeed(serial_lparams()->ospeed_);
    tty_->tcflush(TCIOFLUSH);
    tty_->tcsetattr(TCSANOW);
    tty_->set_nonblocking(true);

    tty_pollfd_  = &pollfds_[0];
    num_pollfds_ = 1;
    
    tty_pollfd_->fd     = tty_->fd();
    tty_pollfd_->events = POLLIN;
    
    poll_timeout_ = serial_lparams()->sync_interval_;
}

//----------------------------------------------------------------------
void
SerialConvergenceLayer::Connection::connect()
{
    // initialize the timer here (it's reset in initiate_contact) so
    // we know to stop syncing after a while
    ::gettimeofday(&data_rcvd_, 0);

    // if there's a dialing string, send it now
    SerialLinkParams* params = serial_lparams();
    size_t initstr_len = params->initstr_.length();
    if (initstr_len != 0) {
        log_debug("copying initialization string \"%s\"",
                  params->initstr_.c_str());
        
        // just to be safe, reserve space in the buffer
        sendbuf_.reserve(initstr_len);
        memcpy(sendbuf_.end(), params->initstr_.data(), initstr_len);
        sendbuf_.fill(initstr_len);
    }
    
    // send a sync byte to kick things off
    send_sync();
}

//----------------------------------------------------------------------
void
SerialConvergenceLayer::Connection::disconnect()
{
    if (tty_->fd() != -1) {
        tty_->close();
    }
}

//----------------------------------------------------------------------
void
SerialConvergenceLayer::Connection::send_sync()
{
    // it's highly unlikely that this will hit, but if it does, we
    // should be ready
    if (sendbuf_.tailbytes() == 0) {
        log_debug("send_sync: "
                  "send buffer has %zu bytes queued, suppressing sync",
                  sendbuf_.fullbytes());
        return;
    }
    ASSERT(sendbuf_.tailbytes() > 0);

    *(sendbuf_.end()) = SYNC;
    sendbuf_.fill(1);
    
    send_data();
}

//----------------------------------------------------------------------
void
SerialConvergenceLayer::Connection::handle_poll_timeout()
{
    if (!synced_) {
        struct timeval now;
        u_int elapsed;
        SerialLinkParams* params = serial_lparams();
        
        ::gettimeofday(&now, 0);
        
        // check that it hasn't been too long since we got some data from
        // the other side (copied from StreamConvergenceLayer)
        elapsed = TIMEVAL_DIFF_MSEC(now, data_rcvd_);
        if (elapsed > params->data_timeout_) {
            log_info("handle_poll_timeout: no data heard for %d msecs "
                     "(data_rcvd %u.%u, now %u.%u, data_timeout %d) "
                     "-- closing contact",
                     elapsed,
                     (u_int)data_rcvd_.tv_sec, (u_int)data_rcvd_.tv_usec,
                     (u_int)now.tv_sec, (u_int)now.tv_usec,
                     params->data_timeout_);
            
            break_contact(ContactEvent::BROKEN);
            return;
        }

        log_debug("handle_poll_timeout: sending another sync byte");
        send_sync();
    } else {
        // once synced, let the StreamCL connection handle it
        StreamConvergenceLayer::Connection::handle_poll_timeout();
    }
}

//----------------------------------------------------------------------
void
SerialConvergenceLayer::Connection::handle_poll_activity()
{
    if (tty_pollfd_->revents & POLLHUP) {
        log_info("tty closed connection -- returned POLLHUP");
        break_contact(ContactEvent::BROKEN);
        return;
    }
    
    if (tty_pollfd_->revents & POLLERR) {
        log_info("error condition on tty -- returned POLLERR");
        break_contact(ContactEvent::BROKEN);
        return;
    }
    
    // first check for write readiness, meaning either we're getting a
    // notification that the deferred connect() call completed, or
    // that we are no longer write blocked
    if (tty_pollfd_->revents & POLLOUT)
    {
        log_debug("poll returned write ready, clearing POLLOUT bit");
        tty_pollfd_->events &= ~POLLOUT;
        send_data();
    }
    
    // check that the connection was not broken during the data send
    if (contact_broken_)
    {
        return;
    }
    
    // finally, check for incoming data
    if (tty_pollfd_->revents & POLLIN) {
        recv_data();
        process_data();

        // Sanity check to make sure that there's space in the buffer
        // for a subsequent read_data() call
        if (recvbuf_.tailbytes() == 0) {
            log_err("process_data left no space in recvbuf!!");
        }

        if (contact_up_ && ! contact_broken_) {
            check_keepalive();
        }
    }
}

//----------------------------------------------------------------------
void
SerialConvergenceLayer::Connection::send_data()
{
    // XXX/demmer this assertion is mostly for debugging to catch call
    // chains where the contact is broken but we're still using the
    // socket
    ASSERT(! contact_broken_);

    u_int towrite = sendbuf_.fullbytes();
    if (params_->test_write_limit_ != 0) {
        towrite = std::min(towrite, params_->test_write_limit_);
    }
    
    log_debug("send_data: trying to drain %u bytes from send buffer...",
              towrite);
    ASSERT(towrite > 0);

    int cc = tty_->write(sendbuf_.start(), towrite);
    if (cc > 0) {
        log_debug("send_data: wrote %d/%zu bytes from send buffer",
                  cc, sendbuf_.fullbytes());
        if (serial_lparams()->hexdump_) {
            oasys::HexDumpBuffer hex;
            hex.append((u_char*)sendbuf_.start(), cc);
            log_multiline(oasys::LOG_ALWAYS, hex.hexify().c_str());
        }
        
        sendbuf_.consume(cc);
        
        if (sendbuf_.fullbytes() != 0) {
            log_debug("send_data: incomplete write, setting POLLOUT bit");
            tty_pollfd_->events |= POLLOUT;

        } else {
            if (tty_pollfd_->events & POLLOUT) {
                log_debug("send_data: drained buffer, clearing POLLOUT bit");
                tty_pollfd_->events &= ~POLLOUT;
            }
        }
    } else if (errno == EWOULDBLOCK) {
        log_debug("send_data: write returned EWOULDBLOCK, setting POLLOUT bit");
        tty_pollfd_->events |= POLLOUT;
        
    } else {
        log_info("send_data: remote connection unexpectedly closed: %s",
                 strerror(errno));
        break_contact(ContactEvent::BROKEN);
    }
}

//----------------------------------------------------------------------
void
SerialConvergenceLayer::Connection::recv_data()
{
    // XXX/demmer this assertion is mostly for debugging to catch call
    // chains where the contact is broken but we're still using the
    // socket
    ASSERT(! contact_broken_);
    
    // this shouldn't ever happen
    if (recvbuf_.tailbytes() == 0) {
        log_err("no space in receive buffer to accept data!!!");
        return;
    }
    
    if (params_->test_read_delay_ != 0) {
        log_debug("recv_data: sleeping for test_read_delay msecs %u",
                  params_->test_read_delay_);
        
        usleep(params_->test_read_delay_ * 1000);
    }

    u_int toread = recvbuf_.tailbytes();
    if (params_->test_read_limit_ != 0) {
        toread = std::min(toread, params_->test_read_limit_);
    }

    log_debug("recv_data: draining up to %u bytes into recv buffer...", toread);
    int cc = tty_->read(recvbuf_.end(), toread);
    if (cc < 1) {
        log_info("remote connection unexpectedly closed");
        break_contact(ContactEvent::BROKEN);
        return;
    }

    log_debug("recv_data: read %d bytes, rcvbuf has %zu bytes",
              cc, recvbuf_.fullbytes());
    if (serial_lparams()->hexdump_) {
        oasys::HexDumpBuffer hex;
        hex.append((u_char*)recvbuf_.end(), cc);
        log_multiline(oasys::LOG_ALWAYS, hex.hexify().c_str());
    }
    recvbuf_.fill(cc);

    // once we hear some data on the channel, it means the other side
    // is up and trying to sync, so send the contact header
    if (! contact_initiated_) {
        initiate_contact();
    }
    
    // if we're at the start of the connection, then ignore SYNC bytes
    if (! synced_)
    {
        while ((recvbuf_.fullbytes() != 0) &&
               (*(u_char*)recvbuf_.start() == SYNC))
        {
            log_debug("got a sync byte... ignoring");
            recvbuf_.consume(1);
        }

        // if something is left, then it's the start of the contact
        // header, so we're done syncing
        if (recvbuf_.fullbytes() != 0)
        {
            log_debug("done reading sync bytes, clearing synced flag");
            synced_ = true;
        }

        // reset the poll timeout
        SerialLinkParams* params = serial_lparams();
        poll_timeout_ = params->data_timeout_;
        
        if (params->keepalive_interval_ != 0 &&
            (params->keepalive_interval_ * 1000) < params->data_timeout_)
        {
            poll_timeout_ = params->keepalive_interval_ * 1000;
        }
    }
}

} // namespace dtn
