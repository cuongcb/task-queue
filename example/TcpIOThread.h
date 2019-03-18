/*
 * Created on Mon Nov 12 2018
 *
 * Copyright (c) 2018 VNGCorp
 * Author: cuongcb
 * File: TcpIOThread.h
 */

#ifndef ZRTC_TCPIOTHREAD_H
#define ZRTC_TCPIOTHREAD_H

#include <deque>
#include <mutex>

#include "zrtc/network/TcpBuffer.h"
#include "zrtc/network/TcpIOThreadIf.h"
#include "zrtc/base/Runnable.h"
#include "zrtc/network/NetworkRetryThread.h"
#include "zrtc/zcommon/QueuingManager.h"
#include "zrtc/common/Stats.h"

#include "zrtc/event_loop/connector.h"
#include "zrtc/event_loop/event_loop.h"
#include "zrtc/event_loop/fd_channel.h"
#include "zrtc/event_loop/tcp_callbacks.h"

#include "zrtc/webrtc/system_wrappers/include/clock.h"


BEG_NSP_ZRTC();

class TcpIOThread:
public zrtc::TcpIOThreadIf,
public zrtc::Runnable {
public:
	explicit TcpIOThread();
	virtual ~TcpIOThread();

	virtual bool UpdateRemoteAddress(const std::string &host,
									uint32_t port) override;
	virtual bool CreateConnection(const std::string &host,
									uint32_t port) override;

	virtual void Start() override;
	virtual void Stop() override;
	virtual void Reset() override;

	virtual void RegisterNetworkHandler(TcpNetworkIOHandler *handler) override;
	virtual bool SendData(const uint8_t *data, size_t size) override;
	
	virtual int32_t InputBwKbit() override;
	virtual int32_t OutputBwKbit() override;
	
public:
	void set_connect_timeout(uint32_t timeout_ms) {
		connect_time_out_ms_ = timeout_ms;
	}

protected:
	virtual void run() override;
	
private:
	void OnConnection(int fd, const std::string &local_addr);
	void OnReservedConnection(int fd, const std::string &local_addr);
	void DoConnect();
	void DoReservedConnect();
	void Disconnect();
	void DisconnectInLoop();
	void MakeActiveConnection(const evloop::TcpConnPtr &conn);
	
	void SendDataInternal();
	void OnCompleteWrite(const TcpBuffer::Ptr &buf);
	void OnReadyWrite();
	
	void MaybeUpdateConnection();
	void ChangeConnection();
	
	void RemoveConnectionInternal(const evloop::TcpConnPtr &conn) {
		auto found = std::find_if(conns_vec_.begin(),
								conns_vec_.end(),
								[conn](const evloop::TcpConnPtr &c) {
									return conn == c;
								});
		
		if (found != conns_vec_.end()) {
			conns_vec_.erase(found);
		}
	}
	
	void UpdateReservedConnection();

private:
	typedef std::lock_guard<std::mutex> ScopedLock;
	
	PocoThread thread_;
	AtomicI8 running_;
	
	TcpNetworkIOHandler *handler_;

	uint32_t connect_time_out_ms_;
	std::shared_ptr<evloop::Connector> connector_;
	
	evloop::EventLoop loop_;

	evloop::TcpConnPtr conn_;
	
	std::atomic<bool> auto_reconnect_;
	
	std::string remote_addr_; // host:port
//	std::string local_addr_; // host:port
	
	// main msg queue for all conns
	// TODO: wrapper this queue
	std::mutex queue_guard_;
	std::deque<TcpBuffer::Ptr> queue_;
	
	int64_t last_send_time_ms_;
	webrtc::Clock *clock_;
	
	std::vector<evloop::TcpConnPtr> conns_vec_;
	std::shared_ptr<evloop::InvokeTimer> conns_timer_;
};

END_NSP_ZRTC();
#endif //ZRTC_TCPIOTHREAD_H