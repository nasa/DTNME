/*
 *    Copyright 2006 Baylor University
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

#ifdef OASYS_BLUETOOTH_ENABLED

#include <oasys/bluez/Bluetooth.h>
#include <oasys/bluez/BluetoothSDP.h>
#include <oasys/util/Random.h>
#include <oasys/util/OptParser.h>
#include "bundling/BundleDaemon.h"
#include "contacts/ContactManager.h"
#include "BluetoothConvergenceLayer.h"

namespace dtn {

BluetoothConvergenceLayer::BluetoothLinkParams
    BluetoothConvergenceLayer::default_link_params_(true);

//----------------------------------------------------------------------
BluetoothConvergenceLayer::BluetoothLinkParams::BluetoothLinkParams(
                                                    bool init_defaults )
    : StreamLinkParams(init_defaults),
      local_addr_(*(BDADDR_ANY)),
      remote_addr_(*(BDADDR_ANY)),
      channel_(BTCL_DEFAULT_CHANNEL)
{
}

//----------------------------------------------------------------------
BluetoothConvergenceLayer::BluetoothConvergenceLayer()
    : StreamConvergenceLayer("BluetoothConvergenceLayer",
                             "bt",BTCL_VERSION)
{
}

//----------------------------------------------------------------------
CLInfo*
BluetoothConvergenceLayer::new_link_params()
{
    return new BluetoothLinkParams(default_link_params_);
}

//----------------------------------------------------------------------
bool
BluetoothConvergenceLayer::init_link(const LinkRef& link,
                                     int argc, const char* argv[])
{
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() == NULL);

    log_debug("adding %s link %s", link->type_str(), link->nexthop());

    // Create a new parameters structure, parse the options, and store
    // them in the link's cl info slot.
    BluetoothLinkParams* params = dynamic_cast<BluetoothLinkParams *>(new_link_params());
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
BluetoothConvergenceLayer::parse_link_params(LinkParams* lparams,
                                              int argc, const char** argv,
                                              const char** invalidp)
{

    BluetoothLinkParams* params = dynamic_cast<BluetoothLinkParams*>(lparams);
    ASSERT(params != NULL);

    oasys::OptParser p;

    p.addopt(new oasys::BdAddrOpt("local_addr",&params->local_addr_));
    p.addopt(new oasys::BdAddrOpt("remote_addr",&params->remote_addr_));
    p.addopt(new oasys::UInt8Opt("channel",&params->channel_));

    int count = p.parse_and_shift(argc, argv, invalidp);
    if (count == -1) {
        return false; // bogus value
    }

    argc -= count;

    // validate the local address
    if (bacmp(&params->local_addr_,BDADDR_ANY) == 0) {
        // try again by reading address
        oasys::Bluetooth::hci_get_bdaddr(&params->local_addr_);
        if (bacmp(&params->local_addr_,BDADDR_ANY) == 0) {
            log_err("cannot find local Bluetooth adapter address");
            return false;
        }
    }

    // continue up to parse the parent class
    return StreamConvergenceLayer::parse_link_params(lparams, argc, argv,
                                                     invalidp);
}

//----------------------------------------------------------------------
void
BluetoothConvergenceLayer::dump_link(const LinkRef& link,
                                     oasys::StringBuffer* buf)
{
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    ASSERT(link->cl_info() != NULL);

    StreamConvergenceLayer::dump_link(link,buf);
    BluetoothLinkParams* params =
        dynamic_cast<BluetoothLinkParams*>(link->cl_info());
    ASSERT(params != NULL);
    buf->appendf("local_addr: %s\n", bd2str(params->local_addr_));
    buf->appendf("remote_addr: %s\n", bd2str(params->remote_addr_));
    buf->appendf("channel: %u\n",params->channel_);
}

//----------------------------------------------------------------------
bool
BluetoothConvergenceLayer::set_link_defaults(int argc, const char* argv[],
                                              const char** invalidp)
{
    return parse_link_params(&default_link_params_, argc, argv, invalidp);
}

//----------------------------------------------------------------------
bool
BluetoothConvergenceLayer::parse_nexthop(const LinkRef& link,
                                         LinkParams* lparams)
{
    BluetoothLinkParams* params = dynamic_cast<BluetoothLinkParams*>(lparams);
    ASSERT(params != NULL);

    std::string tmp;
    bdaddr_t ba;
    const char* p = link->nexthop();
    int numcolons = 5; // expecting 12 hex digits, 5 colons total

    while (numcolons > 0) {
        p = strchr(p+1, ':');
        if (p != NULL) {
            numcolons--;
        } else {
            log_warn("bad format for remote Bluetooth address: '%s'",
                     link->nexthop());
            return false;
        }
    }
    tmp.assign(link->nexthop(), p - link->nexthop() + 3);
    oasys::Bluetooth::strtoba(tmp.c_str(),&ba);

    bacpy(&params->remote_addr_,&ba);
    return true;
}

//----------------------------------------------------------------------
CLConnection*
BluetoothConvergenceLayer::new_connection(const LinkRef& link, LinkParams* p) 
{
    (void)link;
    BluetoothLinkParams *params = dynamic_cast<BluetoothLinkParams*>(p);
    ASSERT(params != NULL);
    return new Connection(this, params);
}

//----------------------------------------------------------------------
bool
BluetoothConvergenceLayer::interface_up(Interface* iface,
                                         int argc, const char* argv[])
{
    log_debug("adding interface %s", iface->name().c_str());
    bdaddr_t local_addr;
    u_int8_t channel = BTCL_DEFAULT_CHANNEL;

    memset(&local_addr,0,sizeof(bdaddr_t));

    oasys::OptParser p;
    p.addopt(new oasys::UInt8Opt("channel",&channel));

    const char* invalid = NULL;
    if (! p.parse(argc, argv, &invalid)) {
        log_err("error parsing interface options: invalid option '%s'",
                invalid);
        return false;
    }

    // read adapter address from default adapter (hci0)
    oasys::Bluetooth::hci_get_bdaddr(&local_addr);
    if (bacmp(&local_addr,BDADDR_ANY) == 0) {
        log_err("invalid local address setting of BDADDR_ANY");
        return false;
    }

    if (channel < 1 || channel > 30) {
        log_err("invalid channel setting of %d",channel);
        return false;
    }

    // create a new server socket for the requested interface
    Listener* listener = new Listener(this);
    listener->logpathf("%s/iface/%s", logpath_, iface->name().c_str());

    int ret = listener->bind(local_addr, channel);

    // be a little forgiving -- if the address is in use, wait for a
    // bit and try again
    if (ret != 0 && errno == EADDRINUSE) {
        listener->logf(oasys::LOG_WARN,
                       "WARNING: error binding to requested socket: %s",
                       strerror(errno));
        listener->logf(oasys::LOG_WARN,
                       "waiting for 10 seconds then trying again");
        sleep(10);

        ret = listener->bind(local_addr, channel);
    }

    if (ret != 0) {
        return false; // error already logged
    }

    // start listening and then start the thread to loop calling accept()
    //dzdebug listener->listen();
    //dzdebug listener->start();

    // store the new listener object in the cl specific portion of the
    // interface
    iface->set_cl_info(listener);

    return true;
}

//----------------------------------------------------------------------
void
BluetoothConvergenceLayer::interface_activate(Interface* iface)
{
    Listener* listener = dynamic_cast<Listener*>(iface->cl_info());
    listener->listen();
    listener->start();
}

//----------------------------------------------------------------------
bool
BluetoothConvergenceLayer::interface_down(Interface* iface)
{
    Listener* listener = dynamic_cast<Listener*>(iface->cl_info());
    ASSERT(listener != NULL);
    listener->stop();
    delete listener;
    return true;
}

//----------------------------------------------------------------------
void
BluetoothConvergenceLayer::dump_interface(Interface* iface,
                                           oasys::StringBuffer* buf)
{
    Listener* listener = dynamic_cast<Listener*>(iface->cl_info());
    ASSERT(listener != NULL);

    bdaddr_t addr;
    listener->local_addr(addr);
    buf->appendf("\tlocal_addr: %s channel: %u\n",
                 bd2str(addr), listener->channel());
}

//----------------------------------------------------------------------
BluetoothConvergenceLayer::Listener::Listener(BluetoothConvergenceLayer* cl)
    : IOHandlerBase(new oasys::Notifier("/dtn/cl/bt/listener")),
      RFCOMMServerThread("/dtn/cl/bt/listener",oasys::Thread::INTERRUPTABLE),
      cl_(cl)
{
    logfd_ = false;
}

//----------------------------------------------------------------------
void
BluetoothConvergenceLayer::Listener::accepted(int fd, bdaddr_t addr,
                                               u_int8_t channel)
{
    log_debug("new connection from %s on channel %u", bd2str(addr),channel);
    Connection *conn =
        new Connection(cl_, &BluetoothConvergenceLayer::default_link_params_,
                       fd, addr, channel);
    conn->start();
}

//----------------------------------------------------------------------
BluetoothConvergenceLayer::Connection::Connection(
        BluetoothConvergenceLayer* cl, BluetoothLinkParams* params)
    : StreamConvergenceLayer::Connection(
            "BluetoothConvergenceLayer::Connection",
            cl->logpath(), cl, params, true /* call connect() */)
{
    logpathf("%s/conn/%s",cl->logpath(),bd2str(params->remote_addr_));

    // set the nexthop parameter for the base class
    set_nexthop(bd2str(params->remote_addr_));

    // Bluetooth socket based on RFCOMM profile stream semantics
    sock_ = new oasys::RFCOMMClient(logpath_);
    sock_->set_local_addr(params->local_addr_);
    sock_->logpathf("%s/sock",logpath_);
    sock_->set_logfd(false);
    sock_->set_remote_addr(params->remote_addr_);
    sock_->set_channel(params->channel_);
    sock_->init_socket(); // make sure sock_::fd_ exists for set_nonblocking
    // sock_->set_nonblocking(true); // defer until after connect()
}

//----------------------------------------------------------------------
BluetoothConvergenceLayer::Connection::Connection(
                                BluetoothConvergenceLayer* cl,
                                BluetoothLinkParams* params,
                                int fd, bdaddr_t addr, u_int8_t channel)
    : StreamConvergenceLayer::Connection(
         "BluetoothConvergenceLayer::Connection", cl->logpath(), cl, params,
         false /* call accept() */)
{
    logpathf("%s/conn/%s-%d",cl->logpath(),bd2str(addr),channel);

    // set the nexthop parameter for the base class
    set_nexthop(bd2str(addr));
    ::bacpy(&params->remote_addr_,&addr);

    sock_ = new oasys::RFCOMMClient(fd, addr, channel, logpath_);
    sock_->set_logfd(false);
    sock_->set_nonblocking(true);
}

//----------------------------------------------------------------------
BluetoothConvergenceLayer::Connection::~Connection()
{
    delete sock_;
}

//----------------------------------------------------------------------
void
BluetoothConvergenceLayer::BluetoothLinkParams::serialize(
                                    oasys::SerializeAction *a) {
    char *la = batostr(&local_addr_);
    char *ra = batostr(&remote_addr_);

	StreamConvergenceLayer::StreamLinkParams::serialize(a);
    a->process("local_addr", (u_char *) la);
    a->process("remote_addr", (u_char *) ra);
    a->process("channel", &channel_);

    free(la);
    free(ra);
}

//----------------------------------------------------------------------
void
BluetoothConvergenceLayer::Connection::initialize_pollfds()
{
    sock_pollfd_ = &pollfds_[0];
    num_pollfds_ = 1;

    sock_pollfd_->fd     = sock_->fd();
    sock_pollfd_->events = POLLIN;

    if (sock_pollfd_->fd == -1) {
        log_err("initialize_pollfds was given a bad socket descriptor");
        break_contact(ContactEvent::BROKEN);
    }

    BluetoothLinkParams* params = dynamic_cast<BluetoothLinkParams*>(params_);
    ASSERT(params != NULL);

    poll_timeout_ = params->data_timeout_;

    if (params->keepalive_interval_ != 0 &&
        (params->keepalive_interval_ * 1000) < params->data_timeout_)
    {
        poll_timeout_ = params->keepalive_interval_ * 1000;
    }
}

//----------------------------------------------------------------------
void
BluetoothConvergenceLayer::Connection::connect()
{
    bdaddr_t addr;
    sock_->remote_addr(addr);
//    sock_->set_channel(params_->channel_);
    log_debug("connect: connecting to %s-%d",bd2str(addr),sock_->channel());

    ASSERT(active_connector_);
    ASSERT(contact_ == NULL || contact_->link()->isopening());

    ASSERT(sock_->state() != oasys::BluetoothSocket::ESTABLISHED);

    int ret = sock_->connect();

    if (ret == 0) {
        log_debug("connect: succeeded immediately");
        ASSERT(sock_->state() == oasys::BluetoothSocket::ESTABLISHED);

        sock_->set_nonblocking(true);
        initiate_contact();

    } else {
        log_info("failed to connect to %s: %s",bd2str(addr),strerror(errno));
        break_contact(ContactEvent::BROKEN);
    }
}

//----------------------------------------------------------------------
void
BluetoothConvergenceLayer::Connection::accept()
{
    bdaddr_t addr;
    memset(&addr,0,sizeof(bdaddr_t));
    ASSERT(sock_->state() == oasys::BluetoothSocket::ESTABLISHED);
    sock_->remote_addr(addr);
    log_debug("accept: got connection from %s",bd2str(addr));
    initiate_contact();
}

//----------------------------------------------------------------------
void
BluetoothConvergenceLayer::Connection::disconnect()
{
    if (sock_->state() != oasys::BluetoothSocket::CLOSED) {
        sock_->close();
    }
}

//----------------------------------------------------------------------
void
BluetoothConvergenceLayer::Connection::handle_poll_activity()
{
    if ((sock_pollfd_->revents & POLLNVAL) == POLLNVAL) {
        log_info("invalid file descriptor -- returned POLLNVAL");
        break_contact(ContactEvent::BROKEN);
        return;
    }

    if ((sock_pollfd_->revents & POLLHUP) == POLLHUP) {
        log_info("remote socket closed connection -- returned POLLHUP");
        break_contact(ContactEvent::BROKEN);
        return;
    }

    if ((sock_pollfd_->revents & POLLERR) == POLLERR) {
        log_info("error condition on remote socket -- returned POLLERR");
        break_contact(ContactEvent::BROKEN);
        return;
    }

    // first check for write readiness, meaning either we're getting a 
    // notification that the deferred connect() call completed, or
    // that we are no longer write blocked
    if ((sock_pollfd_->revents & POLLOUT) == POLLOUT)
    {
        log_debug("poll returned write ready, clearing POLLOUT bit");
        sock_pollfd_->events &= ~POLLOUT;

//        if (sock_->state() == oasys::BluetoothSocket::CONNECTING) {
//            bdaddr_t addr;
//            sock_->remote_addr(addr);
//            int result = sock_->async_connect_result();
//            if (result == 0 && sendbuf_.fullbytes() == 0) {
//                log_debug("delayed connect() to %s succeeded",bd2str(addr));
//                initiate_contact();

//            } else {
//                log_info("connection attempt to %s failed ... %s",
//                         bd2str(addr),strerror(errno));
//                break_contact(ContactEvent::BROKEN);
//            }

//            return;
//        }

        send_data();
    }

    //check that the connection was not broken during the data send
    if (contact_broken_)
    {
       return;
    }

    // check for incoming data
    if ((sock_pollfd_->revents & POLLIN) == POLLIN) {
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
BluetoothConvergenceLayer::Connection::send_data()
{
    // XXX/demmer this assertion is mostly for debugging to catch call
    // chains where the contact is broken but we're still using the
    // socket
    ASSERT(! contact_broken_);

    log_debug("send_data: trying to drain %zu bytes from send buffer...",
              sendbuf_.fullbytes());
    ASSERT(sendbuf_.fullbytes() > 0);
    int cc = sock_->write(sendbuf_.start(), sendbuf_.fullbytes());
    if (cc > 0) {
        log_debug("send_data: wrote %d/%zu bytes from send buffer",
                  cc, sendbuf_.fullbytes());
        sendbuf_.consume(cc);

        if (sendbuf_.fullbytes() != 0) {
            log_debug("send_data: incomplete write, setting POLLOUT bit");
            sock_pollfd_->events |= POLLOUT;
        } else {
            if (sock_pollfd_->events & POLLOUT) {
                log_debug("send_data: drained buffer, clearing POLLOUT bit");
                sock_pollfd_->events &= ~POLLOUT;
            }
        }

    } else if (errno == EWOULDBLOCK) {
        log_debug("send_data: write returned EWOULDBLOCK, setting POLLOUT bit");
        sock_pollfd_->events |= POLLOUT;

    } else {
        log_info("send_data: remote connection unexpectedly closed: %s",
                 strerror(errno));
        break_contact(ContactEvent::BROKEN);
    }
}

//----------------------------------------------------------------------
void
BluetoothConvergenceLayer::Connection::recv_data()
{
    // XXX/demmer this assertion is mostly for debugging to catch call
    // chains where the contact is broken but we're still using the
    // socket
    ASSERT(! contact_broken_);

    // this shouldn't ever happen
    if (recvbuf_.tailbytes() == 0) {
        log_err("no space in receive buffer to accept data!!!");
        break_contact(ContactEvent::BROKEN);
        return;
    }

    if (params_->test_read_delay_ != 0) {
        log_debug("recv_data: sleeping for test_read_delay msecs %u",
                  params_->test_read_delay_);
        usleep(params_->test_read_delay_ * 1000);
    }

    log_debug("recv_data: draining up to %zu bytes into recv buffer...",
              recvbuf_.tailbytes());
    int cc = sock_->read(recvbuf_.end(), recvbuf_.tailbytes());
    if (cc < 1) {
        log_info("remote connection unexpectedly closed: %s (%d)",
                 strerror(errno),errno);
        break_contact(ContactEvent::BROKEN);
        return;
    }

    log_debug("recv_data: read %d bytes, rcvbuf has %zu bytes",
              cc, recvbuf_.fullbytes());
    recvbuf_.fill(cc);
}

} // dtn

#endif // OASYS_BLUETOOTH_ENABLED
