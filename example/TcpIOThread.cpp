/*
 * Created on Mon Nov 12 2018
 *
 * Copyright (c) 2018 VNGCorp
 * Author: cuongcb
 * File: TcpIOThread.cpp
 */

#include "zrtc/network/TcpIOThread.h"

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
#include "zrtc/event_loop/connector.h"
#include "zrtc/event_loop/event_loop.h"
#include "zrtc/event_loop/fd_channel.h"
#include "zrtc/event_loop/event_sockets.h"
#include "zrtc/event_loop/tcp_conn.h"

BEG_NSP_ZRTC();

namespace {
	constexpr char kDefaultNetworkAddress[] = "120.138.69.88:3015";
	constexpr char kDefaultLocalNetworkAddress[] = "10.199.217.142:5005";
	constexpr uint32_t kDefaultReconnectIntervalMs = 3000;
	constexpr uint32_t kDefaultConnectTimeOutMs = 3000;
	constexpr int64_t kMaxDelaySendTimeMs = 5000;
	constexpr uint32_t kMaxReservedConnections = 3;
	constexpr uint32_t kDefaultReservedConnectionsCheckMs = 1000;
	constexpr uint32_t kMaxRttMs = 3000;
}

TcpIOThread::TcpIOThread()
	: running_(false)
	, handler_(nullptr)
	, connect_time_out_ms_(kDefaultConnectTimeOutMs)
	, auto_reconnect_(true)
	, remote_addr_(kDefaultNetworkAddress)
//	, local_addr_("")
	, last_send_time_ms_(-1)
	, clock_(webrtc::Clock::GetRealTimeClock()){

	LOG_T_F(LS_INFO) << "TcpIOThread::TcpIOThread() Create a TCP IO thread...";
	rtc::LogMessage::LogToDebug(rtc::LoggingSeverity::LS_SENSITIVE);
}

TcpIOThread::~TcpIOThread() {

}

bool TcpIOThread::UpdateRemoteAddress(const std::string &host,
									uint32_t port) {
	remote_addr_ = host;
	remote_addr_ += ":";
	remote_addr_ += Utility::sprintf("%d", port);
	
	return true;
}

bool TcpIOThread::CreateConnection(const std::string &host, uint32_t port) {
	if (host.empty() || port < 0 || port > 65536) {
		LOG_T_F(LS_INFO) << "TcpIOThread::CreateConnection invalid address.";
		return false;
	}
	
	UpdateRemoteAddress(host, port);
	
	loop_.QueueInLoop(std::bind(&TcpIOThread::DoConnect, this));

	return true;
}

void TcpIOThread::run() {
	LOG_T_F(LS_INFO) << "Tcp IO thread started...";
	loop_.Run();
	LOG_T_F(LS_INFO) << "Tcp IO Thread stopped...";
}

void TcpIOThread::Start() {
	if (running_.get()) {
		return;
	}
	
	LOG_T_F(LS_INFO) << "TcpIOThread::Start() TCP IO thread starting...";
	
	running_ = true;
	
	LOG_T_F(LS_INFO) << "TcpIOThread::PocoThread is running? " << thread_.isRunning();
	thread_.start(*this);
}

void TcpIOThread::Stop() {
	LOG_T_F(LS_INFO) << "TcpIOThread::Stop() TCP IO thread stopped...";
	if (!running_.get()) {
		return;
	}
	running_ = false;
	
	auto_reconnect_ = false;
	
//	LOG_T_F(LS_INFO) << "TcpIOThread::NewConnectionHandler connector's status: %d", connector_->status();
	Disconnect();

	loop_.Stop();
	try {
		thread_.tryJoin(500);
	} catch (...) {

	}
}

void TcpIOThread::Reset() {
	LOG_T_F(LS_INFO) << "TcpIOThread::Reset()";
}

void TcpIOThread::RegisterNetworkHandler(TcpNetworkIOHandler *handler) {
	handler_ = handler;
}

bool TcpIOThread::SendData(const uint8_t *data, size_t size) {
//	LOG_T_F(LS_INFO) << "TcpIOThread::SendData() size(" << size << ")";
	// TODO: check conn_.get() here or push to queue
	{
		ScopedLock lock(queue_guard_);
		LOG_T_F(LS_INFO) << "queue_size=" << queue_.size();
		if (queue_.empty()) {
			if (conn_.get()) {
				LOG_T_F(LS_INFO) << "TcpIOThread::SendData() size(" << size << ")";
				conn_->Send(data, size);
				return true;
			}
		}
		
		if (queue_.size() == 200) {
			// Clear the message queue
			// Keep the first msg to achieve stream integrity
			TcpBuffer::Ptr front_msg = queue_.front();
			queue_.clear();
			queue_.push_front(front_msg);
		}
		queue_.push_back(TcpBuffer::Ptr(new TcpBuffer(data, size)));
	}
	
	{
		ScopedLock lock(queue_guard_);
		if (!queue_.empty()) {
			if (conn_.get()) {
				conn_->EnableWrite();
			}
		}
	}

	return true;
}

void TcpIOThread::SendDataInternal() {
	LOG_T_F(LS_INFO) << "";
	{
		ScopedLock lock(queue_guard_);
		TcpBuffer::Ptr msg = queue_.front();
		
		if (conn_.get()) {
			conn_->Send(msg);
			queue_.pop_front();
		}
	}
}

void TcpIOThread::OnCompleteWrite(const TcpBuffer::Ptr &buf) {
	LOG_T_F(LS_INFO) << "";

	// only count fully sent message
	// ignore ping msg feedback
	if (!buf || buf->data_size() == 0) {
		last_send_time_ms_ = clock_->TimeInMilliseconds();
		return;
	}
	
	{
		ScopedLock lock(queue_guard_);
		queue_.push_front(buf);
		if (conn_.get()) {
			conn_->EnableWrite();
		}
	}
}

void TcpIOThread::OnReadyWrite() {
	LOG_T_F(LS_INFO) << "";
	{
		ScopedLock lock(queue_guard_);
		{
			if (queue_.empty()) {
				if (conn_.get()) {
					conn_->DisableWrite();
				}
				
				return;
			}
		}
	}

	SendDataInternal();
}

void TcpIOThread::MakeActiveConnection(const evloop::TcpConnPtr& conn) {
	conn->SetMessageCallback([this](const evloop::TcpConnPtr &conn,
								uint8_t *data,
								size_t len) {
		rtc::PacketTime recv_time = rtc::CreatePacketTime(0);
		handler_->OnReadTcpPacket(recv_time, data, len);
	});
	
	conn->SetWriteCompleteCallback([this](const evloop::TcpConnPtr &conn,
										const TcpBuffer::Ptr &buf) {
		OnCompleteWrite(buf);
	});
	
	conn->SetWriteReadyCallback([this](const evloop::TcpConnPtr &conn) {
		OnReadyWrite();
	});

	conn->SetCloseCallback([this](const evloop::TcpConnPtr &conn) {
		conn_.reset();
		MaybeUpdateConnection();
	});
}


void TcpIOThread::OnConnection(int fd, const std::string& local_addr) {
	LOG_T_F(LS_INFO) << "fd=" << fd;
	if (fd < 0) {
		handler_->OnEstablishConnection(false);
		if (auto_reconnect_) {
			DoConnect();
		}
		return;
	}

	evloop::sock::SetTCPNoDelay(fd, true);		
//	local_addr_ = local_addr;
	evloop::TcpConnPtr c = 
			evloop::TcpConnPtr(new evloop::TcpConn(&loop_,
													"active_conn",
													fd,
													local_addr,
													remote_addr_,
													0));
	c->set_type(evloop::TcpConn::kOutgoing);
	c->SetTCPNoDelay(true);

	MakeActiveConnection(c);
	
	conn_ = c;
	conn_->OnAttachedToLoop();

	handler_->OnEstablishConnection(true);
	
	conns_timer_ = loop_.RunEvery(kDefaultReservedConnectionsCheckMs,
					std::bind(&TcpIOThread::UpdateReservedConnection, this));
	conns_timer_->Start();
}

void TcpIOThread::OnReservedConnection(int fd, const std::string& local_addr) {
	LOG_T_F(LS_INFO) << "fd=" << fd;
	
	if (fd >= 0) {
		evloop::sock::SetTCPNoDelay(fd, true);		
	//	local_addr_ = local_addr;
		evloop::TcpConnPtr c = 
				evloop::TcpConnPtr(new evloop::TcpConn(&loop_,
														"conn",
														fd,
														local_addr,
														remote_addr_,
														0));
		c->set_type(evloop::TcpConn::kOutgoing);
		c->SetTCPNoDelay(true);

		c->SetCloseCallback([this](const evloop::TcpConnPtr &conn) {
	//		conn_.reset();		
			RemoveConnectionInternal(conn);
		});

		conns_vec_.push_back(std::move(c));
	}
}

void TcpIOThread::DoConnect() {
	LOG_T_F(LS_INFO) << "TcpIOThread::DoConnect connect to " << remote_addr_.c_str();
	if (connector_.get() && connector_->IsConnecting()) {
		LOG_T_F(LS_INFO) << "TcpIOThread::DoConnect: Client is connecting...";
		return;
	}

	std::string host_addr;
	if (remote_addr_.empty()) {
		host_addr = kDefaultNetworkAddress;
	}
	else {
		host_addr = remote_addr_;
	}
	
	auto f = [=]() {
		connector_.reset(new evloop::Connector(&loop_, "", host_addr,
											connect_time_out_ms_,
											false,
											kDefaultReconnectIntervalMs));
		connector_->SetNewConnectionCallback(std::bind(&TcpIOThread::OnConnection,
													this,
													std::placeholders::_1,
													std::placeholders::_2));
		connector_->Start();
	};
	
	loop_.QueueInLoop(f);
}

void TcpIOThread::DoReservedConnect() {
	LOG_T_F(LS_INFO) << "TcpIOThread::DoReservedConnect connect to " << remote_addr_.c_str();
	if (connector_.get() && connector_->IsConnecting()) {
		LOG_T_F(LS_INFO) << "TcpIOThread::DoReservedConnect: Client is connecting...";
		return;
	}
	
	std::string host_addr;
	if (remote_addr_.empty()) {
		host_addr = kDefaultNetworkAddress;
	}
	else {
		host_addr = remote_addr_;
	}

	auto f = [=]() {
		connector_.reset(new evloop::Connector(&loop_, "", host_addr,
											connect_time_out_ms_,
											false,
											kDefaultReconnectIntervalMs));
		connector_->SetNewConnectionCallback(std::bind(&TcpIOThread::OnReservedConnection,
													this,
													std::placeholders::_1,
													std::placeholders::_2));
		connector_->Start();
	};
	
	loop_.QueueInLoop(f);
}

void TcpIOThread::Disconnect() {
	loop_.QueueInLoop(std::bind(&TcpIOThread::DisconnectInLoop, this));
}

void TcpIOThread::DisconnectInLoop() {
	auto_reconnect_ = false;
	
	if (conn_.get()) {
		conn_->Close();
	}
	else {
		assert(connector_.get() && !connector_->IsConnected());
	}
	
	if (connector_->IsConnected() || connector_->IsDisconnected()) {
		
	}
	else {
		connector_->Cancel();
	}
	
	connector_.reset();
}

int32_t TcpIOThread::InputBwKbit() {
	if (!conn_.get()) {
		return 0;
	}
	return Utility::bytesToKbit(conn_->GetInputStat());
}

int32_t TcpIOThread::OutputBwKbit() {
	if (!conn_.get()) {
		return 0;
	}
	return Utility::bytesToKbit(conn_->GetOutputStat());
}

void TcpIOThread::MaybeUpdateConnection() {
	LOG_T_F(LS_INFO) << "";
	int64_t now = clock_->TimeInMilliseconds();
	if (!conn_.get()
	|| conn_->rtt() > kMaxRttMs
	|| (last_send_time_ms_ != -1
	&& now - last_send_time_ms_ > kMaxDelaySendTimeMs)) {
		loop_.QueueInLoop(std::bind(&TcpIOThread::ChangeConnection, this));
	}
}

void TcpIOThread::ChangeConnection() {
	//TODO: Implement algorithm here
	LOG_T_F(LS_INFO) << "";
//	Reconnect();
	if (conns_vec_.empty()) {
		return;
	}
	
	auto c = conns_vec_.begin();
	for (auto i = ++c; i != conns_vec_.end(); ++i) {
		if ((*i)->rtt() < (*c)->rtt()) {
			c = i;
		}
	}
	
	MakeActiveConnection(*c);
	conn_ = *c;
	conns_vec_.erase(c);
}

void TcpIOThread::UpdateReservedConnection() {
	LOG_T_F(LS_INFO) << "";
	if (conns_vec_.size() < kMaxReservedConnections) {
		DoReservedConnect();
	}
}



END_NSP_ZRTC();

