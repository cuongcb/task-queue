/* 
 * File:   TcpConnector.h
 * Author: lap11894
 *
 * Created on January 8, 2019, 14:21 AM
 */

#ifndef ZRTC_DNSRESOLVER_H
#define ZRTC_DNSRESOLVER_H

#include <netinet/in.h>

#include "zrtc/event_loop/event_common.h"

struct evdns_base;
struct evdns_getaddrinfo_request;

namespace evloop {
	
class EventLoop;
class TimerEventWatcher;

class DNSResolver {
public:
	typedef std::function<void(std::vector<struct in_addr> &addrs)> Functor;
	
	DNSResolver(EventLoop *loop,
				const std::string &host,
				int timeout,
				const Functor &f);
	
	~DNSResolver();
	
	void Start();
	void Cancel();
	
	const std::string &host() const {
		return host_;
	}
	
private:
	void SyncDNSResolve();
	void AsyncDNSResolve();
	void AsyncWait();
	void OnTimeout();
	void OnCanceled();
	void ClearTimer();
	void OnResolved(int errcode, struct addrinfo *addr);
	void OnResolved();
	static void OnResolved(int errcode, struct addrinfo *addr, void *arg);
	
private:
	typedef std::shared_ptr<DNSResolver> DNSResolverPtr;
	
	EventLoop * loop_;
	struct ::evdns_base *dnsbase_;
	struct ::evdns_getaddrinfo_request *dns_req_;
	std::string host_;
	int timeout_ms_;
	Functor functor_;
	
	std::unique_ptr<TimerEventWatcher> timer_;
	std::vector<struct ::in_addr> addrs_;
};
	
	
} // namespace evloop

#endif // ZRTC_DNSRESOLVER_H