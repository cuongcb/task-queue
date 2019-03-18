/* 
 * File:   TcpConnector.h
 * Author: lap11894
 *
 * Created on January 7, 2019, 11:21 AM
 */

#ifndef ZRTC_TCPCONNECTOR_H
#define ZRTC_TCPCONNECTOR_H

#include <memory>

#include "zrtc/event_loop/event_common.h"
#include "zrtc/event_loop/event_sockets.h"

namespace evloop {

class FdChannel;
class EventLoop;
class EventWatcher;
class DNSResolver;


class Connector:
public std::enable_shared_from_this<Connector> {
public:
	typedef std::function<void(int, const std::string &)> NewConnectionCallback;
	
	Connector(EventLoop * loop,
				const std::string &local_addr,
				const std::string &remote_addr,
				int timeout_ms,
				bool auto_reconnect,
				int interval_ms);

	~Connector();
	
	void Start();
	void Cancel();
	
public:
	void SetNewConnectionCallback(NewConnectionCallback cb) {
		conn_fn_ = cb;
	}
	
	bool IsConnecting() const {
		return status_ == kConnecting;
	}
	
	bool IsConnected() const {
		return status_ == kConnected;
	}
	
	bool IsDisconnected() const {
		return status_ == kDisconnected;
	}
	
	int status() const {
		return status_;
	}
	
private:
	void Connect();
	void HandleWrite();
	void HandleError();
	void OnConnectTimeout();
	void OnDNSResolved(const std::vector <struct in_addr> &addrs);
	
private:
	enum ConnectorStatus {
		kDisconnected = 0,
		kDNSResolving = 1,
		kDNSResolved = 2,
		kConnecting = 3,
		kConnected = 4,
	};
	
	ConnectorStatus status_;
	
	EventLoop * loop_;
	bool own_loop_;
	
	std::string local_address_;	
	std::string remote_address_;
	
	std::string remote_host_;
	int remote_port_;
	
	int connecting_timeout_ms_;
	
	bool auto_reconnect_;
	int reconnect_interval_ms_;
	
	struct sockaddr_storage raddr_;

	int fd_;
	bool own_fd_;
	
	std::unique_ptr<FdChannel> chan_;
	std::unique_ptr<EventWatcher> timer_;
	std::shared_ptr<DNSResolver> dns_resolver_;
	NewConnectionCallback conn_fn_;
};

} // namespace evloop

#endif /* ZRTC_TCPCONNECTOR_H */

