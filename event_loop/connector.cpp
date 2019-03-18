#include "zrtc/event_loop/connector.h"

#include "zrtc/event_loop/libevent.h"

#include "zrtc/event_loop/event_common.h"
#include "zrtc/event_loop/event_loop.h"
#include "zrtc/event_loop/event_watcher.h"
#include "zrtc/event_loop/dns_resolver.h"
#include "zrtc/event_loop/fd_channel.h"
#include "zrtc/event_loop/event_sockets.h"

namespace evloop {

Connector::Connector(EventLoop * loop,
				const std::string &local_addr,
				const std::string &remote_addr,
				int timeout_ms,
				bool auto_reconnect,
				int interval_ms)
	: status_(kDisconnected)
	, own_loop_(false)
	, loop_(loop)
	, local_address_(local_addr)
	, remote_address_(remote_addr)
	, connecting_timeout_ms_(timeout_ms)
	, auto_reconnect_(auto_reconnect)
	, reconnect_interval_ms_(interval_ms) {
	if (sock::SplitHostPort(remote_address_.data(), remote_host_, remote_port_)) {
		raddr_ = sock::ParseFromIPPort(remote_address_.data());
	}
	LOG_T_F(LS_INFO) << "CUONGCB: Create new tcp connector";
}

Connector::~Connector() {
	assert(loop_->IsInLoopThread());
	LOG_T_F(LS_INFO) << "CUONGCB::~TcpConnector tcp connector";
	if (status_ == kDNSResolving) {
		
	}
	else if (!IsConnected()) {
		EVUTIL_CLOSESOCKET(fd_);
		fd_ = -1;
	}
	
	chan_.reset();
}

void Connector::Start() {
	assert(loop_->IsInLoopThread());
	LOG_T_F(LS_INFO) << "CUONGCB::Start tcp connector";
	
	timer_.reset(new TimerEventWatcher(loop_,
				std::bind(&Connector::OnConnectTimeout, shared_from_this()),
				connecting_timeout_ms_));
	timer_->Init();
	timer_->AsyncWait();
	
	if (!sock::IsZeroAddress(&raddr_)) {
		Connect();
		return;
	}
	
	// CuongCB: If zero address => Raise error
	
	status_ = kDNSResolving;
	auto f = std::bind(&Connector::OnDNSResolved,
						shared_from_this(),
						std::placeholders::_1);
	dns_resolver_.reset(new DNSResolver(loop_, remote_host_, connecting_timeout_ms_, f));
	dns_resolver_->Start();
}

void Connector::Cancel() {
	assert(loop_->IsInLoopThread());
	LOG_T_F(LS_INFO) << "CUONGCB::Cancel tcp connector";
	
	if (dns_resolver_.get()) {
		dns_resolver_->Cancel();
		dns_resolver_.reset();
	}
	
	assert(timer_);
	timer_->Cancel();
	timer_.reset();
	
	if (status_ == kDNSResolving) {
		conn_fn_(-1, "");
	}
	
	if (chan_.get()) {
		chan_->DisableAllEvent();
		chan_->Close();
	}
}

void Connector::Connect() {
	LOG_T_F(LS_INFO) << "CUONGCB::Connect tcp connector";
    assert(fd_ == -1);
    fd_ = sock::CreateNonblockingSocket();
    own_fd_ = true;
    assert(fd_ >= 0);

    if (!local_address_.empty()) {
        struct sockaddr_storage ss = sock::ParseFromIPPort(local_address_.data());
        struct sockaddr* addr = sock::sockaddr_cast(&ss);
        int rc = ::bind(fd_, addr, sizeof(*addr));
        if (rc != 0) {
            int serrno = errno;
            HandleError();
            return;
        }
    }
    struct sockaddr* addr = sock::sockaddr_cast(&raddr_);
    int rc = ::connect(fd_, addr, sizeof(*addr));
    if (rc != 0) {
        int serrno = errno;
        if (!EVUTIL_ERR_CONNECT_RETRIABLE(serrno)) {
            HandleError();
            return;
        } else {
            // TODO how to do it
        }
    }

    status_ = kConnecting;

    chan_.reset(new FdChannel(loop_, fd_, false, true));
    chan_->SetWriteCallback(std::bind(&Connector::HandleWrite, shared_from_this()));
    chan_->AttachToLoop();
}

void Connector::HandleWrite() {
	LOG_T_F(LS_INFO) << "CUONGCB::HandleWrite tcp connector";
    if (status_ == kDisconnected) {
        // The connecting may be timeout, but the write event handler has been
        // dispatched in the EventLoop pending task queue, and next loop time the handle is invoked.
        // So we need to check the status whether it is at a kDisconnected
        LOG_T_F(LS_INFO) << "fd=" << chan_->fd() << " remote_addr=" << remote_address_ << " receive write event when socket is closed";
        return;
    }

    assert(status_ == kConnecting);
    int err = 0;
    socklen_t len = sizeof(len);
    if (getsockopt(chan_->fd(), SOL_SOCKET, SO_ERROR, (char*)&err, (socklen_t*)&len) != 0) {
        err = errno;
        LOG_T_F(LS_ERROR) << "getsockopt failed err=" << err << " " << strerror(err);
    }

    if (err != 0) {
        EVUTIL_SET_SOCKET_ERROR(err);
        HandleError();
        return;
    }

    assert(fd_ == chan_->fd());
    struct sockaddr_storage addr = sock::GetLocalAddr(chan_->fd());
    std::string laddr = sock::ToIPPort(&addr);
	own_fd_ = false; // Move the ownership of the fd to TCPConn
	fd_ = -1;
	status_ = kConnected;
	timer_->Cancel();
	timer_.reset();
	chan_->DisableAllEvent();
	chan_->Close(); // If the application did reset the connector, closing chan_
					// is the ending point of this object by releasing chan_
					// write_fn_ =>> DO NOT do anything after this ending point
					// => It may cause memory leak or unstable object's state
	conn_fn_(chan_->fd(), laddr);
}

void Connector::HandleError() {
	LOG_T_F(LS_INFO) << "CUONGCB::HandleError tcp connector";
    assert(loop_->IsInLoopThread());
    int serrno = errno;

    // In this error handling method, we will invoke 'conn_fn_' callback function
    // to notify the user application layer in which the user maybe call TCPClient::Disconnect.
    // TCPClient::Disconnect may cause this TcpConnector object desctruct.
    auto self = shared_from_this();

    LOG_T_F(LS_ERROR) << "this=" << this << " status=" << status_ << " fd=" << fd_  << " use_count=" << self.use_count() << " errno=" << serrno << " " << strerror(serrno);

    status_ = kDisconnected;

    if (chan_) {
        assert(fd_ > 0);
        chan_->DisableAllEvent();
        chan_->Close();
    }

    // Avoid DNSResolver callback again when timeout
    if (dns_resolver_) {
        dns_resolver_->Cancel();
        dns_resolver_.reset();
    }

    timer_->Cancel();
    timer_.reset();

    // If the connection is refused or it will not try again,
    // We need to notify the user layer that the connection established failed.
    // Otherwise we will try to do reconnection silently.
    if (EVUTIL_ERR_CONNECT_REFUSED(serrno) || !auto_reconnect_) {
        conn_fn_(-1, "");
    }

    // Although TCPClient has a Reconnect() method to deal with automatically reconnection problem,
    // TCPClient's Reconnect() will be invoked when a established connection is broken down.
    //
    // But if we could not connect to the remote server at the very beginning,
    // the TCPClient's Reconnect() will never be triggered.
    // So TcpConnector needs to do reconnection automatically itself.
    if (auto_reconnect_) {

        // We must close(fd) firstly and then we can do the reconnection.
        if (fd_ > 0) {
            LOG_T_F(LS_INFO) << "TcpConnector::HandleError close(" << fd_ << ")";
            assert(own_fd_);
            EVUTIL_CLOSESOCKET(fd_);
            fd_ = INVALID_SOCKET;
        }

        LOG_T_F(LS_INFO) << "loop=" << loop_ << " auto reconnect in " << reconnect_interval_ms_ << "s thread=" << std::this_thread::get_id();
        loop_->RunAfter(reconnect_interval_ms_, std::bind(&Connector::Start, shared_from_this()));
    }
}

void Connector::OnConnectTimeout() {
    LOG_T_F(LS_INFO) << "this=" << this << " TcpConnector::OnConnectTimeout status=" << status_ << " fd=" << fd_ << " this=" << this;
    assert(status_ == kConnecting || status_ == kDNSResolving);
    EVUTIL_SET_SOCKET_ERROR(ETIMEDOUT);
    HandleError();
}

void Connector::OnDNSResolved(const std::vector <struct in_addr>& addrs) {
    LOG_T_F(LS_VERBOSE) << "addrs.size=" << addrs.size() << " this=" << this;
    if (addrs.empty()) {
        LOG_T_F(LS_ERROR) << "this=" << this << " DNS Resolve failed. host=" << dns_resolver_->host();
        HandleError();
        return;
    }

    struct sockaddr_in* addr = sock::sockaddr_in_cast(&raddr_);
    addr->sin_family = AF_INET;
    addr->sin_port = htons(remote_port_);
    addr->sin_addr = addrs[0];
    status_ = kDNSResolved;

    Connect();
}

	
} //namespace evloop