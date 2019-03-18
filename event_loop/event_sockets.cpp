#include "zrtc/event_loop/event_sockets.h"

#include "zrtc/event_loop/libevent.h"

#include "zrtc/event_loop/event_common.h"
#include "zrtc/common/Utility.h"

namespace evloop {

static const std::string empty_string;

std::string strerror(int e) {
#ifdef ZRTC_WIN
    LPVOID buf = nullptr;
    ::FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, e, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&buf, 0, nullptr);

    if (buf) {
        std::string s = (char*)buf;
        LocalFree(buf);
        return s;
    }

#elif defined(ZRTC_MAC)
    char buf[2048] = {};
    int rc = strerror_r(e, buf, sizeof(buf) - 1); // XSI-compliant
    if (rc == 0) {
        return std::string(buf);
    }
	
#elif defined(ZRTC_ANDROID)
	return empty_string;
#else
    char buf[2048] = {};
    #if (_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600) && ! _GNU_SOURCE
    int rc = strerror_r(e, buf, sizeof(buf) - 1); // XSI-compliant
    if (rc == 0) {
        return std::string(buf);
    }
    #else
    const char* s = strerror_r(e, buf, sizeof(buf) - 1); // GNU-specific
    if (s) {
        return std::string(s);
    }

    #endif
#endif
    return empty_string;
}

namespace sock {
socket_t CreateNonblockingSocket() {
    int serrno = 0;

    /* Create listen socket */
    socket_t fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        serrno = errno;
        LOG_F(LS_ERROR) << "socket error " << strerror(serrno) << std::endl;
        return INVALID_SOCKET;
    }

    if (evutil_make_socket_nonblocking(fd) < 0) {
        goto out;
    }

#ifndef ZRTC_WIN
    if (fcntl(fd, F_SETFD, 1) == -1) {
        serrno = errno;
        LOG_F(LS_ERROR) << "fcntl(F_SETFD)" << strerror(serrno) << std::endl;
        goto out;
    }
#endif

    SetKeepAlive(fd, true);
    SetReuseAddr(fd);
    SetReusePort(fd);
    return fd;
out:
    EVUTIL_CLOSESOCKET(fd);
    return INVALID_SOCKET;
}

socket_t CreateUDPServer(int port) {
    socket_t fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1) {
        int serrno = errno;
        LOG_F(LS_ERROR) << "socket error " << strerror(serrno) << std::endl;
        return INVALID_SOCKET;
    }
    SetReuseAddr(fd);
    SetReusePort(fd);

    std::string addr = std::string("0.0.0.0:") + zrtc::Utility::sprintf("%d", port); //std::to_string(port);
    struct sockaddr_storage local = ParseFromIPPort(addr.c_str());
    if (::bind(fd, (struct sockaddr*)&local, sizeof(struct sockaddr))) {
        int serrno = errno;
        LOG_F(LS_ERROR) << "socket bind error=" << serrno << " " << strerror(serrno) << std::endl;
        return INVALID_SOCKET;
    }

    return fd;
}

bool ParseFromIPPort(const char* address, struct sockaddr_storage& ss) {
    memset(&ss, 0, sizeof(ss));
    std::string host;
    int port;
    if (!SplitHostPort(address, host, port)) {
        return false;
    }

    short family = AF_INET;
    auto index = host.find(':');
    if (index != std::string::npos) {
        family = AF_INET6;
    }

    struct sockaddr_in* addr = sockaddr_in_cast(&ss);
#if LIBEVENT_VERSION_NUMBER >= 0x02001500
        int rc = ::evutil_inet_pton(family, host.data(), &addr->sin_addr);
#else
		int rc = ::inet_pton(family, host.data(), &addr->sin_addr);
#endif
    if (rc == 0) {
        LOG_F(LS_INFO) << "ParseFromIPPort evutil_inet_pton (AF_INET '" << host.data() << "', ...) rc=0. " << host.data() << " is not a valid IP address. Maybe it is a hostname." << std::endl;
        return false;
    } else if (rc < 0) {
        int serrno = errno;
        if (serrno == 0) {
            LOG_F(LS_INFO) << "[" << host.data() << "] is not a IP address. Maybe it is a hostname." << std::endl;
        } else {
            LOG_F(LS_WARNING) << "ParseFromIPPort evutil_inet_pton (AF_INET, '" << host.data() << "', ...) failed : " << strerror(serrno) << std::endl;
        }
        return false;
    }

    addr->sin_family = family;
    addr->sin_port = htons(port);

    return true;
}

bool SplitHostPort(const char* address, std::string& host, int& port) {
    std::string a = address;
    if (a.empty()) {
        return false;
    }

    size_t index = a.rfind(':');
    if (index == std::string::npos) {
        LOG_F(LS_ERROR) << "Address specified error <" << address << ">. Cannot find ':'" << std::endl;
        return false;
    }

    if (index == a.size() - 1) {
        return false;
    }

    port = std::atoi(&a[index + 1]);

    host = std::string(address, index);
    if (host[0] == '[') {
        if (*host.rbegin() != ']') {
            LOG_F(LS_ERROR) << "Address specified error <" << address << ">. '[' ']' is not pair." << std::endl;
            return false;
        }

        // trim the leading '[' and trail ']'
        host = std::string(host.data() + 1, host.size() - 2);
    }

    // Compatible with "fe80::886a:49f3:20f3:add2]:80"
    if (*host.rbegin() == ']') {
        // trim the trail ']'
        host = std::string(host.data(), host.size() - 1);
    }

    return true;
}

struct sockaddr_storage GetLocalAddr(socket_t sockfd) {
    struct sockaddr_storage laddr;
    memset(&laddr, 0, sizeof laddr);
    socklen_t addrlen = static_cast<socklen_t>(sizeof laddr);
    if (::getsockname(sockfd, sockaddr_cast(&laddr), &addrlen) < 0) {
        LOG_F(LS_ERROR) << "GetLocalAddr:" << strerror(errno) << std::endl;
        memset(&laddr, 0, sizeof laddr);
    }

    return laddr;
}

std::string ToIPPort(const struct sockaddr_storage* ss) {
    std::string saddr;
    int port = 0;

    if (ss->ss_family == AF_INET) {
        struct sockaddr_in* addr4 = const_cast<struct sockaddr_in*>(sockaddr_in_cast(ss));
        char buf[INET_ADDRSTRLEN] = {};
#if LIBEVENT_VERSION_NUMBER >= 0x02001500
        const char* addr = ::evutil_inet_ntop(ss->ss_family, &addr4->sin_addr, buf, INET_ADDRSTRLEN);
#else
		const char *addr = ::inet_ntop(ss->ss_family, &addr4->sin_addr, buf, INET_ADDRSTRLEN);
#endif

        if (addr) {
            saddr = addr;
        }

        port = ntohs(addr4->sin_port);
    } else if (ss->ss_family == AF_INET6) {
        struct sockaddr_in6* addr6 = const_cast<struct sockaddr_in6*>(sockaddr_in6_cast(ss));
        char buf[INET6_ADDRSTRLEN] = {};
#if LIBEVENT_VERSION_NUMBER >= 0x02001500
        const char* addr = ::evutil_inet_ntop(ss->ss_family, &addr6->sin_addr, buf, INET_ADDRSTRLEN);
#else
		const char *addr = ::inet_ntop(ss->ss_family, &addr6->sin6_addr, buf, INET_ADDRSTRLEN);
#endif

        if (addr) {
            saddr = std::string("[") + addr + "]";
        }

        port = ntohs(addr6->sin6_port);
    } else {
        LOG_F(LS_ERROR) << "unknown socket family connected" << std::endl;
        return empty_string;
    }

    if (!saddr.empty()) {
//        saddr.append(":", 1).append(std::to_string(port));
		saddr.append(":", 1).append(zrtc::Utility::sprintf("%d", port));
    }

    return saddr;
}

std::string ToIPPort(const struct sockaddr* ss) {
    return ToIPPort(sockaddr_storage_cast(ss));
}

std::string ToIPPort(const struct sockaddr_in* ss) {
    return ToIPPort(sockaddr_storage_cast(ss));
}

std::string ToIP(const struct sockaddr* s) {
    auto ss = sockaddr_storage_cast(s);
    if (ss->ss_family == AF_INET) {
        struct sockaddr_in* addr4 = const_cast<struct sockaddr_in*>(sockaddr_in_cast(ss));
        char buf[INET_ADDRSTRLEN] = {};
#if LIBEVENT_VERSION_NUMBER >= 0x02001500
        const char* addr = ::evutil_inet_ntop(ss->ss_family, &addr4->sin_addr, buf, INET_ADDRSTRLEN);
#else
		const char *addr = ::inet_ntop(ss->ss_family, &addr4->sin_addr, buf, INET_ADDRSTRLEN);
#endif
        if (addr) {
            return std::string(addr);
        }
    } else if (ss->ss_family == AF_INET6) {
        struct sockaddr_in6* addr6 = const_cast<struct sockaddr_in6*>(sockaddr_in6_cast(ss));
        char buf[INET6_ADDRSTRLEN] = {};
#if LIBEVENT_VERSION_NUMBER >= 0x02001500
        const char* addr = ::evutil_inet_ntop(ss->ss_family, &addr6->sin_addr, buf, INET_ADDRSTRLEN);
#else
		const char *addr = ::inet_ntop(ss->ss_family, &addr6->sin6_addr, buf, INET_ADDRSTRLEN);
#endif
        if (addr) {
            return std::string(addr);
        }
    } else {
        LOG_F(LS_ERROR) << "unknown socket family connected" << std::endl;
    }

    return empty_string;
}

void SetTimeout(socket_t fd, uint32_t timeout_ms) {
#ifdef ZRTC_WIN
    DWORD tv = timeout_ms;
#else
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
#endif
    int ret = setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    assert(ret == 0);
    if (ret != 0) {
        int err = errno;
        LOG_F(LS_ERROR) << "setsockopt SO_RCVTIMEO ERROR " << err << strerror(err) << std::endl;
    }
}

void SetKeepAlive(socket_t fd, bool on) {
    int optval = on ? 1 : 0;
    int rc = ::setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE,
                          reinterpret_cast<const char*>(&optval), static_cast<socklen_t>(sizeof optval));
    if (rc != 0) {
        int serrno = errno;
        LOG_F(LS_ERROR) << "setsockopt(SO_KEEPALIVE) failed, errno=" << serrno << " " << strerror(serrno) << std::endl;
    }
}

void SetReuseAddr(socket_t fd) {
    int optval = 1;
    int rc = ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
                          reinterpret_cast<const char*>(&optval), static_cast<socklen_t>(sizeof optval));
    if (rc != 0) {
        int serrno = errno;
        LOG_F(LS_ERROR) << "setsockopt(SO_REUSEADDR) failed, errno=" << serrno << " " << strerror(serrno) << std::endl;
    }
}

void SetReusePort(socket_t fd) {
#ifdef SO_REUSEPORT
    int optval = 1;
    int rc = ::setsockopt(fd, SOL_SOCKET, SO_REUSEPORT,
                          reinterpret_cast<const char*>(&optval), static_cast<socklen_t>(sizeof optval));
    LOG_F(LS_INFO) << "setsockopt SO_REUSEPORT fd=" << fd << " rc=" << rc << std::endl;
    if (rc != 0) {
        int serrno = errno;
        LOG_F(LS_ERROR) << "setsockopt(SO_REUSEPORT) failed, errno=" << serrno << " " << strerror(serrno) << std::endl;
    }
#endif
}


void SetTCPNoDelay(socket_t fd, bool on) {
    int optval = on ? 1 : 0;
    int rc = ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY,
                          reinterpret_cast<const char*>(&optval), static_cast<socklen_t>(sizeof optval));
    if (rc != 0) {
        int serrno = errno;
        LOG_F(LS_ERROR) << "setsockopt(TCP_NODELAY) failed, errno=" << serrno << " " << strerror(serrno) << std::endl;
    }
}

} // namespace sock
}

#ifdef ZRTC_WIN
int readv(socket_t sockfd, struct iovec* iov, int iovcnt) {
    DWORD readn = 0;
    DWORD flags = 0;

    if (::WSARecv(sockfd, iov, iovcnt, &readn, &flags, nullptr, nullptr) == 0) {
        return readn;
    }

    return -1;
}
#endif