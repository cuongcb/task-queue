#include <sys/types.h>

#include "zrtc/event_loop/tcp_conn.h"

#include "zrtc/event_loop/event_common.h"

#include "zrtc/event_loop/libevent.h"

#include "zrtc/event_loop/fd_channel.h"
#include "zrtc/event_loop/event_loop.h"
#include "zrtc/event_loop/event_sockets.h"
#include "zrtc/event_loop/invoke_timer.h"

namespace {
	constexpr size_t kDefaultMaxQueueSize = 200;
	constexpr size_t kMaxPaketSizeByte = 1500;
	constexpr int32_t kDefaultPingIntervalMs = 1000;
}

namespace evloop {

TcpConn::TcpConn(EventLoop* l,
                 const std::string& n,
                 socket_t sockfd,
                 const std::string& laddr,
                 const std::string& raddr,
                 uint64_t conn_id)
    : loop_(l)
    , fd_(sockfd)
    , id_(conn_id)
    , name_(n)
    , local_addr_(laddr)
    , remote_addr_(raddr)
	, input_bw_stat_(0)
	, output_bw_stat_(0)
    , type_(kIncoming)
    , status_(kDisconnected)
	, close_delay_ms_(0)
	, buffer_(new zrtc::TcpBuffer(kMaxPaketSizeByte))
	, enable_ping_(true)
	, clock_(webrtc::Clock::GetRealTimeClock())
	, rtt_(0) {
    if (sockfd >= 0) {
        chan_.reset(new FdChannel(l, sockfd, false, false));
        chan_->SetReadCallback(std::bind(&TcpConn::HandleRead, this));
        chan_->SetWriteCallback(std::bind(&TcpConn::HandleWrite, this));
    }

    LOG_T_F(LS_INFO) << "TcpConn::[" << name_ << "] channel=" << chan_.get() << " fd=" << sockfd << " addr=" << AddrToString();
	if (enable_ping_) {
		auto f = [this]() {
			assert(loop_->IsInLoopThread());
			ping_timer_ = loop_->RunEvery(kDefaultPingIntervalMs, std::bind(&TcpConn::Ping, shared_from_this()));
		};
		
		// Queue here because of failure on shared_from_this()
		loop_->QueueInLoop(f);
	}
}

TcpConn::~TcpConn() {
    LOG_T_F(LS_INFO) << "name=" << name()
        << " channel=" << chan_.get()
        << " fd=" << fd_ << " type=" << int(type())
        << " status=" << StatusToString() << " addr=" << AddrToString();
    assert(status_ == kDisconnected);

    if (fd_ >= 0) {
        assert(chan_);
        assert(fd_ == chan_->fd());
        assert(chan_->IsNoneEvent());
        EVUTIL_CLOSESOCKET(fd_);
        fd_ = INVALID_SOCKET;
    }

    assert(!delay_close_timer_.get());
}

void TcpConn::Close() {
    LOG_T_F(LS_INFO) << "fd=" << fd_ << " status=" << StatusToString() << " addr=" << AddrToString();
    status_ = kDisconnecting;
    auto c = shared_from_this();
    auto f = [c]() {
        assert(c->loop_->IsInLoopThread());
        c->HandleClose();
    };

    // Use QueueInLoop to fix TCPClient::Close bug when the application delete TCPClient in callback
    loop_->QueueInLoop(f);
}

#define lebeswap_64(x)                          \
    ((((x) & 0xff00000000000000ull) >> 56)       \
     | (((x) & 0x00ff000000000000ull) >> 40)     \
     | (((x) & 0x0000ff0000000000ull) >> 24)     \
     | (((x) & 0x000000ff00000000ull) >> 8)      \
     | (((x) & 0x00000000ff000000ull) << 8)      \
     | (((x) & 0x0000000000ff0000ull) << 24)     \
     | (((x) & 0x000000000000ff00ull) << 40)     \
     | (((x) & 0x00000000000000ffull) << 56))

// [4bytes | 4bytes | 8bytes]
// [   0   |   id   |  time ]

void TcpConn::SerializePing(uint8_t *buffer, PingPacket ping) {
	uint32_t offset = 0;
	
	uint32_t ping_type = 0;
	memcpy(buffer + offset, &ping_type, sizeof ping_type);
	offset += sizeof ping_type;
	
	uint32_t ping_id = htonl(ping.id);
	memcpy(buffer + offset, &ping_id, sizeof ping_id);
	offset += sizeof ping_id;
	
	int64_t ping_time = lebeswap_64(ping.time);
	memcpy(buffer + offset, &ping_time, sizeof ping_time);
}

TcpConn::PingPacket TcpConn::DeserializePing(const uint8_t *buffer) {
	uint32_t id = 0;
	memcpy(&id, buffer + 4, sizeof id);
	int64_t send_time = 0;
	memcpy(&send_time, buffer + 8, sizeof send_time);
	return PingPacket(ntohl(id), lebeswap_64(send_time));
}

void TcpConn::Ping() {
	if (status_ != kConnected) {
		return;
	}
	
	PingPacket ping = CreatePingMessage();
	uint8_t buffer[16] = {0};
	SerializePing(buffer, ping);
	
	int32_t remaining = sizeof(buffer);
	int32_t nwritten = 0;
	// maybe it will be stuck here, cover on application
	while (remaining) {
		nwritten = ::send(fd_, buffer + nwritten, remaining, MSG_NOSIGNAL);
		if (nwritten < 0) {
			int err = errno;
			HandleError(err);
			
			return;
		}
		remaining -= nwritten;
	}
	
	if (write_complete_fn_) {
		loop_->RunInLoop(std::bind(write_complete_fn_, shared_from_this(), nullptr));
	}

    LOG_T_F(LS_INFO) << "fd=" << fd_ << " status=" << StatusToString() << " addr=" << AddrToString();
}

void TcpConn::Pong() {
    LOG_T_F(LS_INFO) << "fd=" << fd_ << " status=" << StatusToString() << " addr=" << AddrToString();
	if (buffer_->data_size() >= 16) {
		PingPacket pong = DeserializePing(buffer_->data());
		int64_t now = clock_->TimeInMilliseconds();
		rtt_ = (now - pong.time);
		LOG_T_F(LS_INFO) << "rtt=" << rtt_;
		
		buffer_->DiscardFirst(16);
	}
}

bool TcpConn::Send(const uint8_t* data, size_t len) {
	return Send(zrtc::TcpBuffer::Ptr(new zrtc::TcpBuffer(data, len)));
}

bool TcpConn::Send(const zrtc::TcpBuffer::Ptr &buf) {
	if (status_ != kConnected) {
		return false;
	}
	LOG_T_F(LS_INFO) << "use_count=" << buf.use_count();
	loop_->RunInLoop(std::bind(&TcpConn::SendInLoop, shared_from_this(), buf));
	
	return true;
}

void TcpConn::SendInLoop(const zrtc::TcpBuffer::Ptr &buf) {
	assert(loop_->IsInLoopThread());
	
	int32_t nwritten = 0;
	int32_t remaining = buf->data_size();
	LOG_T_F(LS_INFO) << "use_count=" << buf.use_count(); // from std::bind
	
	LOG_T_F(LS_INFO) << "status=" << StatusToString() << ", chan_=" << chan_->EventsToString();
	
	if (status_ == kConnected) {
		nwritten = ::send(fd_, buf->data(), remaining, MSG_NOSIGNAL);
		if (write_complete_fn_) {
			auto n = std::max(nwritten, 0);
			buf->Skip(n);
			write_complete_fn_(shared_from_this(), buf);
			output_bw_stat_.writeStats(n);
			LOG_T_F(LS_INFO) << "Send out via socket(" << chan_->fd() << "), bytes(" << nwritten << ")";
		}

		if (nwritten < 0) {
			int err = errno;
			HandleError(err);
		}
	}
	
	return;
}

void TcpConn::HandleRead() {
    assert(loop_->IsInLoopThread());
//    if (!buffer_->IsValid()) {
//		buffer_->Reset();
//	}

	int32_t n = ::recv(fd_, buffer_->available(), buffer_->size(), 0);
	LOG_T_F(LS_INFO) << "fd=" << fd_ << ", bytes=" << n;
    if (n > 0) {
		buffer_->Advance(n);
		input_bw_stat_.writeStats(n);
		if (buffer_->frame_size() == 0) {
				// pong msg
			Pong();
		} else if (buffer_->ready()) {
			msg_fn_(shared_from_this(), buffer_->packet(), buffer_->packet_size());
			buffer_->Rewind();
		}
    } else if (n == 0) {
        if (type() == kOutgoing) {
            // This is an outgoing connection, we own it and it's done. so close it
            LOG_T_F(LS_INFO) << "fd=" << fd_ << ". We read 0 bytes and close the socket.";
            status_ = kDisconnecting;
            HandleClose();
        } else {
            chan_->DisableReadEvent();
            if (close_delay_ms_ == 0) {
                LOG_T_F(LS_INFO) << "channel (fd=" << chan_->fd() << ") DisableReadEvent. delay time " << close_delay_ms_ / 1000 << "s. We close this connection immediately";
                DelayClose();
            } else {
                // This is an incoming connection, we need to preserve the
                // connection for a while so that we can reply to it.
                // And we set a timer to close the connection eventually.
                LOG_T_F(LS_INFO) << "channel (fd=" << chan_->fd() << ") DisableReadEvent. And set a timer to delay close this TcpConn, delay time " << close_delay_ms_ / 1000 << "s";
                delay_close_timer_ = loop_->RunAfter(close_delay_ms_, std::bind(&TcpConn::DelayClose, shared_from_this())); // TODO leave it to user layer close.
            }
        }
    } else {
		int err = errno;
        HandleError(err);
    }
}

void TcpConn::HandleWrite() {
    assert(loop_->IsInLoopThread());
    assert(!chan_->attached() || chan_->IsWritable());
	// TODO: raise to iothread 
	if (write_ready_fn_) {
		write_ready_fn_(shared_from_this());
	}
}

void TcpConn::DelayClose() {
    assert(loop_->IsInLoopThread());
    LOG_T_F(LS_INFO) << "addr=" << AddrToString() << " fd=" << fd_ << " status_=" << StatusToString();
    status_ = kDisconnecting;
    delay_close_timer_.reset();
    HandleClose();
}

void TcpConn::HandleClose() {
    LOG_T_F(LS_INFO) << "addr=" << AddrToString() << " fd=" << fd_ << " status_=" << StatusToString();

    // Avoid multi calling
    if (status_ == kDisconnected) {
        return;
    }

    // We call HandleClose() from TcpConn's method, the status_ is kConnected
    // But we call HandleClose() from out of TcpConn's method, the status_ is kDisconnecting
    assert(status_ == kDisconnecting);

    status_ = kDisconnecting;
    assert(loop_->IsInLoopThread());
    chan_->DisableAllEvent();
    chan_->Close();

    TcpConnPtr conn(shared_from_this());

    if (delay_close_timer_) {
        LOG_T_F(LS_INFO) << "loop=" << loop_ << " Cancel the delay closing timer.";
        delay_close_timer_->Cancel();
        delay_close_timer_.reset();
    }
	
	if (ping_timer_) {
		LOG_T_F(LS_INFO) << "loop=" << loop_ << ", Cancel the ping timer.";
		ping_timer_->Cancel();
		ping_timer_.reset();
	}

    if (conn_fn_) {
        // This callback must be invoked at status kDisconnecting
        // e.g. when the TCPClient disconnects with remote server,
        // the user layer can decide whether to do the reconnection.
        assert(status_ == kDisconnecting);
        conn_fn_(conn);
    }

    if (close_fn_) {
        close_fn_(conn);
    }
    LOG_T_F(LS_VERBOSE) << "addr=" << AddrToString() << " fd=" << fd_ << " status_=" << StatusToString() << " use_count=" << conn.use_count();
    status_ = kDisconnected;
}

void TcpConn::HandleError(int err) {
    LOG_T_F(LS_INFO) << "fd=" << fd_ << " status=" << StatusToString();
	LOG_T_F(LS_INFO) << "error=" << err << " " << strerror(err);
	if (!EVUTIL_ERR_RW_RETRIABLE(err)) {
		status_ = kDisconnecting;
		HandleClose();
	}
}

void TcpConn::OnAttachedToLoop() {
	LOG_T_F(LS_INFO) << "";
    assert(loop_->IsInLoopThread());
    status_ = kConnected;
    chan_->EnableReadEvent();

    if (conn_fn_) {
        conn_fn_(shared_from_this());
    }
}

void TcpConn::SetTCPNoDelay(bool on) {
    sock::SetTCPNoDelay(fd_, on);
}

std::string TcpConn::StatusToString() const {
	switch (status_) {
		case kDisconnected:
			return "kDisconnected";
		case kConnecting:
			return "kConnecting";
		case kConnected:
			return "kConnected";
		case kDisconnecting:
			return "kDisconnecting";
		default:
			return "Unknown";
	}
}

void TcpConn::EnableWrite() {
	auto f = [=]() {
		if (chan_.get() && !chan_->IsWritable()) {
			chan_->EnableWriteEvent();
		}
	};

	loop_->RunInLoop(f);
}

void TcpConn::DisableWrite() {
	auto f = [=]() {
		if (chan_.get() && chan_->IsWritable()) {
			chan_->DisableWriteEvent();
		}		
	};

	loop_->RunInLoop(f);
}

} // namespace evloop