/*
 * Created on Mon Nov 12 2018
 *
 * Copyright (c) 2018 VNGCorp
 * Author: cuongcb
 * File: TcpIOThread.h
 */

#ifndef ZRTCTCPIOTHREAD_H
#define ZRTCTCPIOTHREAD_H

#include <deque>

#include "zrtc/network/TcpBuffer.h"
#include "zrtc/network/TcpIOThreadIf.h"
#include "zrtc/base/Runnable.h"
#include "zrtc/network/NetworkRetryThread.h"
#include "zrtc/zcommon/QueuingManager.h"
#include "zrtc/common/Stats.h"

#include "zrtc/event_loop/tcp_connector.h"
#include "zrtc/event_loop/event_loop.h"
#include "zrtc/event_loop/tcp_channel.h"


BEG_NSP_ZRTC();

class IOModuleInterface;

class TcpIOThread:
public zrtc::TcpIOThreadIf,
public zrtc::Runnable {
public:
	explicit TcpIOThread(bool enable_loopback = false);
	virtual ~TcpIOThread();

	virtual bool UpdateConnections(const std::string &host,
									uint32_t port) override;
	virtual bool CreateConnection() override;
	virtual bool CreateConnection(const std::string &host,
									uint32_t port) override;
	virtual bool CreateAsyncConnection(const std::string &host,
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
	void NewConnectionHandler(int fd, const std::string &local_addr);
	void StartConnector(const std::string &raddr);
	void StopConnector();
	
	void HandleRead();
	int HandleReadInternal(int fd, uint8_t * buffer, uint32_t len);
	
	void HandleWrite();
	void HandleClose();
	void HandleError(int err);
	void HandleConnect();

	void HandleLoopback(const uint8_t *data, uint32_t len);
	
private:	
	enum ClientStatus {
		kDisconnected = 0,
		kDisconnecting = 1,
		kConnected = 2,
		kConnecting = 3,
	};
	
	enum ClientReadState {
		kReadFrameSize = 0,
		kReadFrameData = 1,
	};

	// Runner thread
	Mutex mutex_;

	PocoThread thread_;
	AtomicI8 running_;
	
	TcpNetworkIOHandler *handler_;

	std::deque<TcpBuffer::Ptr> msg_queue_;
	
	Stats input_bw_stat_;
	Stats output_bw_stat_;

	uint32_t connect_time_out_ms_;
	std::shared_ptr<evloop::TcpConnector> connector_;
	PocoThread connector_thread_;
	
	evloop::EventLoop loop_;
	std::unique_ptr<evloop::TcpChannel> chan_;
	
	std::atomic<ClientStatus> status_;
	bool auto_reconnect_;
	
	std::string remote_host_;
	uint32_t remote_port_;
	struct ::sockaddr_in sock_addr_;
	
	bool enable_loopback_;
	std::unique_ptr<IOModuleInterface> loopback_module_;
	
	ClientReadState read_state_;
	uint32_t frame_size_;
	std::unique_ptr<uint8_t []> input_buffer_;
	uint32_t write_index_;
	uint32_t read_index_;
};

END_NSP_ZRTC();
#endif //ZRTCTCPIOTHREAD_H