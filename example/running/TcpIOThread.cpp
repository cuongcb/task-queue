/*
 * Created on Mon Nov 12 2018
 *
 * Copyright (c) 2018 VNGCorp
 * Author: cuongcb
 * File: TcpIOThread.cpp
 */

#include "zrtc/network/TcpIOThread.h"
#include "zrtc/common/Utility.h"

// c headers
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>

// c++ headers
#include <memory>
// project headers
#include "zrtc/network/SocketUtil.h"
#include "zrtc/event_loop/tcp_connector.h"
#include "zrtc/event_loop/event_loop.h"
#include "zrtc/event_loop/tcp_channel.h"
#include "zrtc/event_loop/event_sockets.h"
#include "zrtc/network/LoopbackIOModule.h"

BEG_NSP_ZRTC();

namespace {
	constexpr char kDefaultHost[] = "120.138.69.88";
	constexpr uint32_t kDefaultPort = 3015;
	constexpr char kDefaultNetworkAddress[] = "120.138.69.88:3015";
	constexpr char kDefaultLocalNetworkAddress[] = "10.199.217.142:5005";
	constexpr int64_t kMaxQueueSize = 500;
}

TcpIOThread::TcpIOThread(bool enable_loopback)
	: running_(false) 
	, handler_(nullptr)
	, connect_time_out_ms_(3000)
	, status_(kDisconnected)
	, auto_reconnect_(true)
	, remote_host_(kDefaultHost)
	, remote_port_(kDefaultPort)
	, enable_loopback_(false)
	, read_state_(kReadFrameSize)
	, frame_size_(0)
	, write_index_(0)
	, read_index_(0) {

	ZRTC_DEBUG("TcpIOThread::TcpIOThread() Create a TCP IO thread...");
	if (enable_loopback_) {
		loopback_module_.reset(new LoopbackIOModule());
		loopback_module_->RegisterOnReceiveCallback(std::bind(&TcpIOThread::HandleLoopback, this, std::placeholders::_1, std::placeholders::_2));
}
}

TcpIOThread::~TcpIOThread() {

}

bool TcpIOThread::UpdateConnections(const std::string &host,
									uint32_t port) {
	remote_host_ = host;
	remote_port_ = port;
	return true;
}

bool TcpIOThread::CreateConnection() {
	return CreateConnection(remote_host_, remote_port_);
}

bool TcpIOThread::CreateConnection(const std::string &host, uint32_t port) {
	if (!host.empty()) {
		remote_host_ = host;
	}
	
	if (port > 0) {
		remote_port_ = port;
	}
	
	if (connector_.get() && connector_->IsConnecting()) {
		ZRTC_DEBUG("TcpIOThread::CreateConnection: Client is connecting...");
		return false;
	}
	
	sock_addr_.sin_family = AF_INET;
	sock_addr_.sin_port = htons(port);
	sock_addr_.sin_addr.s_addr = inet_addr(host.c_str());
	
	if (enable_loopback_) {
		status_ = kConnected;
		if (handler_) {
			handler_->OnEstablishConnection(true);
		}
		return true;
	}
	
	std::string raddr = remote_host_;
	raddr += ":";
	raddr += Utility::sprintf("%d", remote_port_);
	
	StopConnector();
	status_ = kConnecting;
	StartConnector(raddr);

	return true;
}

bool TcpIOThread::CreateAsyncConnection(const std::string &host,
										uint32_t port) {
	
	return true;
}

void TcpIOThread::run() {
	ZRTC_DEBUG("Tcp IO thread started...");
	loop_.Run();
	ZRTC_DEBUG("Tcp IO Thread stopped...");
}

void TcpIOThread::Start() {
	if (running_.get()) {
		return;
	}
	
	ZRTC_DEBUG("TcpIOThread::Start() TCP IO thread starting...");
	
	running_ = true;
	
	if (loopback_module_.get()) {
		loopback_module_->Start();
	}
	
	thread_.start(*this);
}

void TcpIOThread::Stop() {
	ZRTC_DEBUG("TcpIOThread::Stop() TCP IO thread stopped...");
	if (!running_.get()) {
		return;
	}
	running_ = false;
	
	if (loopback_module_.get()) {
		loopback_module_->Stop();
	}
	
//	ZRTC_DEBUG("TcpIOThread::NewConnectionHandler connector's status: %d", connector_->status());
	StopConnector();

	loop_.Stop();
	try {
		thread_.tryJoin(500);
	} catch (...) {

	}
}

void TcpIOThread::Reset() {
	ZRTC_DEBUG("TcpIOThread::Reset()");
}

void TcpIOThread::RegisterNetworkHandler(TcpNetworkIOHandler *handler) {
	handler_ = handler;
}

bool TcpIOThread::SendData(const uint8_t *data, size_t size) {
	if (loopback_module_.get()) {
		loopback_module_->Send(data, size);
		output_bw_stat_.writeStats(size);
		return true;
	}
	
	TcpBuffer::Ptr wrapper = TcpBuffer::Ptr(new TcpBuffer(data, size));
	
	int32_t nwritten = 0;
	int32_t remaining = wrapper->length();
	int32_t flags = 0;
	
#ifdef MSG_NOSIGNAL
	flags |= MSG_NOSIGNAL;
#endif
	
	if (status_ == kConnected && msg_queue_.empty()) {
		nwritten = ::send(chan_->fd(), wrapper->data(), remaining, flags);
		if (nwritten >= 0) {
			remaining = wrapper->length() - nwritten;
			if (remaining == 0) {
				// send_complete_fn callback goes here
			}
			else if (remaining < 0) {
				// Should never reach here

			}
			else {
				wrapper->Skip(nwritten);
				MutexScopedLock lock(&mutex_);
				msg_queue_.push_front(wrapper);
			}
			
//			ZRTC_DEBUG("TcpIOThread::SendData send out via socket(%d), bytes(%d)", chan_->fd(), nwritten);
			output_bw_stat_.writeStats(nwritten);
			return true;
		}
		else {
			int err = errno;
			ZRTC_DEBUG("TcpIOThread::SendData %s", strerror(err));
			HandleError(err);
		}
	}
	else {
		MutexScopedLock lock(&mutex_);
		if (msg_queue_.size() == kMaxQueueSize) {
			// Clear the message queue
			// Keep the first msg to achieve stream integrity
			TcpBuffer::Ptr front_msg = msg_queue_.front();
			msg_queue_.clear();
			msg_queue_.push_front(front_msg);
			ZRTC_DEBUG("TcpIOThread::SendData drop message queue!!");
		}
		msg_queue_.push_back(wrapper);
//		ZRTC_DEBUG("TcpIOThread::SendData push message to queue");
	}
	
	if (status_ != kConnected) {
		return false;
	}
	
	if (!msg_queue_.empty() && chan_.get()) {
		if (!chan_->IsWritable()) {
			chan_->EnableWriteEvent();
		}
	}
	
	return true;
}

void TcpIOThread::NewConnectionHandler(int fd, const std::string& local_addr) {
	if (fd < 0) {
		status_ = kDisconnected;
		handler_->OnEstablishConnection(false);
	}
	else {
		evloop::sock::SetTCPNoDelay(fd, true);
		chan_.reset(new evloop::TcpChannel(&loop_, fd, true, true, true));
		chan_->SetReadCallback(std::bind(&TcpIOThread::HandleRead, this));
		chan_->SetWriteCallback(std::bind(&TcpIOThread::HandleWrite, this));
		chan_->AttachToLoop();
		
		status_ = kConnected;
		
		handler_->OnEstablishConnection(true);
	}
}

void TcpIOThread::HandleRead() {
	int n = 0;
	switch(read_state_) {
		case kReadFrameSize:
			union {
				uint8_t buf[sizeof (uint32_t)];
				uint32_t size;
			} framing;
			
			framing.size = frame_size_;
			
			n = HandleReadInternal(chan_->fd(), &framing.buf[write_index_], sizeof(frame_size_) - write_index_);
			if (n > 0) {
				write_index_ += n;
			} else {
				return;
			}
			
			frame_size_ = framing.size;

			if (write_index_ < sizeof(frame_size_)) {
				return;
			}
			
			if (frame_size_ > ZRTC_MTU_SIZE || frame_size_ <= 4) {
				status_ = kDisconnecting;
				HandleClose();
				return;
			}
			
			input_buffer_.reset(new uint8_t[frame_size_]);
			read_index_ = 4;
			read_state_ = kReadFrameData;
			
			break;
		case kReadFrameData:
			n = HandleReadInternal(chan_->fd(), input_buffer_.get() + write_index_, frame_size_ - write_index_);
			if (n > 0) {
				write_index_ += n;
			} else {
				return;
			}
			
			if (write_index_ == frame_size_) {
				rtc::PacketTime recv_time = rtc::CreatePacketTime(0);
				handler_->OnReadTcpPacket(recv_time,
											input_buffer_.get() + read_index_,
											write_index_ - read_index_,
											sock_addr_);
				
				input_buffer_.reset();
				read_index_ = 0;
				write_index_ = 0;
				frame_size_ = 0;
				read_state_ = kReadFrameSize;
			}
			
			if (write_index_ > frame_size_) {
				status_ = kDisconnecting;
				HandleClose();
			}

			break;
		default:
			break;
	}
}

int TcpIOThread::HandleReadInternal(int fd, uint8_t* buffer, uint32_t len) {
	assert(buffer);
	int32_t n = ::recv(fd, buffer, len, 0);
	if (n == 0) {
		status_ = kDisconnecting;
		HandleClose();
	} else if (n < 0) {
		int err = errno;
		HandleError(err);
	} else {
		input_bw_stat_.writeStats(n);
	}
	
	return n;
}


void TcpIOThread::HandleWrite() {	
	TcpBuffer::Ptr msg;
	{
		MutexScopedLock lock(&mutex_);
		if (msg_queue_.empty()) {
			if (chan_.get() && chan_->IsWritable()) {
				chan_->DisableWriteEvent();
			}
			return;
		}
		msg = msg_queue_.front();
	}

	assert(chan_.get() && chan_->IsWritable());
	
	int32_t remaining = msg->length();
	int32_t flags = 0;
	
#ifdef MSG_NOSIGNAL
	flags |= MSG_NOSIGNAL;
#endif
	
	int32_t n = ::send(chan_->fd(), msg->data(), remaining, flags);
	if (n >= 0) {
		remaining = remaining - n;
		if (remaining == 0) {
			// write_complete_fn callback goes here
			{
				MutexScopedLock lock(&mutex_);
				msg_queue_.pop_front();
			}		
		}
		else if (remaining > 0) {
			msg->Skip(n);
		}
		
		output_bw_stat_.writeStats(n);
//		ZRTC_DEBUG("TcpIOThread::HandleWrite send out via socket(%d), bytes(%d)", chan_->fd(), n);
	}
	else {
		int err = errno;
		ZRTC_DEBUG("TcpIOThread::HandleWrite %s", strerror(err));
		HandleError(err);
	}
}

void TcpIOThread::HandleClose() {
	chan_->DisableAllEvent();
	chan_->Close();
	chan_.reset();
	
	read_state_ = kReadFrameSize;
	frame_size_ = 0;
	read_index_ = 0;
	write_index_ = 0;
	input_buffer_.reset();
	
	status_ = kDisconnected;
	
	if (auto_reconnect_) {
		ZRTC_DEBUG("TcpIOThread::HandleClose reconnect");
		loop_.QueueInLoop(std::bind((bool(TcpIOThread::*)())&TcpIOThread::CreateConnection, this));
	}
}

void TcpIOThread::HandleError(int err) {
	if (!EVUTIL_ERR_RW_RETRIABLE(err)) {
			if (err == EPIPE || err == ECONNRESET) {
				status_ = kDisconnecting;
				HandleClose();
			}
	}
	else {
		ZRTC_DEBUG("HandleError: %s", strerror(err));
	}
}

void TcpIOThread::HandleConnect() {
	ZRTC_DEBUG("TcpIOThread::HandleConnect stop connector");
	loop_.QueueInLoop(std::bind(&TcpIOThread::StopConnector, this));
}

void TcpIOThread::HandleLoopback(const uint8_t* data, uint32_t len) {
	if (handler_) {
		input_bw_stat_.writeStats(len);
		rtc::PacketTime recv_time = rtc::CreatePacketTime(0);
		handler_->OnReadTcpPacket(recv_time, const_cast<uint8_t *>(data), len, sock_addr_);
	}
}

void TcpIOThread::StartConnector(const std::string &raddr) {
	ZRTC_DEBUG("TcpIOThread::StartConnector connect to %s", raddr.c_str());
	std::string host_addr;
	if (raddr.empty()) {
		host_addr = kDefaultNetworkAddress;
	}
	else {
		host_addr = raddr;
	}
	
	connector_.reset(new evloop::TcpConnector("", host_addr,
											connect_time_out_ms_,
											true,
											3000));
	connector_->SetNewConnectionCallback(std::bind(&TcpIOThread::NewConnectionHandler,
													this,
													std::placeholders::_1,
													std::placeholders::_2));
	connector_->SetCompleteConnectionCallback(std::bind(&TcpIOThread::HandleConnect, this));
	
	connector_thread_.start(*connector_.get());
}

void TcpIOThread::StopConnector() {
	if (!connector_.get()) {
		return;
	}
	
	if (connector_->IsConnecting()) {
		connector_->Cancel();
	}
	
	evloop::EventLoop *loop = connector_->self_loop();
	if (loop && loop->IsRunning()) {
		connector_->self_loop()->Stop();
	}
	
	try {
		connector_thread_.tryJoin(500);
	}
	catch (...) {
		
	}
	
	connector_.reset();
}

int32_t TcpIOThread::InputBwKbit() {
	return Utility::bytesToKbit(input_bw_stat_.getStatsAndReset());
}

int32_t TcpIOThread::OutputBwKbit() {
	return Utility::bytesToKbit(output_bw_stat_.getStatsAndReset());
}

END_NSP_ZRTC();

