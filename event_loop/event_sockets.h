/* 
 * File:   TcpChannel.h
 * Author: lap11894
 *
 * Created on January 8, 2019, 16:20 AM
 */

#ifndef ZRTC_EVENTSOCKETS_H
#define ZRTC_EVENTSOCKETS_H

#include <string>
#include <string.h>

#ifdef ZRTC_WIN
#include <ws2tcpip.h>
#include <ws2def.h>
#else
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h> // for TCP_NODELAY
#include <sys/socket.h>
#include <arpa/inet.h>
#endif


#ifdef ZRTC_WIN
#include <string> // avoid compiling failed because of 'errno' redefined as 'WSAGetLastError()'
#define errno WSAGetLastError()
#include <windows.h>

#include <ws2tcpip.h>
#include <WinSock2.h>
#include <io.h>
#include <ws2ipdef.h>

typedef int ssize_t;
#define iovec _WSABUF
#define iov_base buf
#define iov_len  len

#else
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/uio.h>
#include <errno.h>
#ifndef SOCKET
#define SOCKET int           /**< SOCKET definition */
#endif
#ifndef INVALID_SOCKET
#define INVALID_SOCKET -1    /**< invalid socket definition */
#endif
#endif

#ifdef ZRTC_WIN

/*
 * Windows Sockets errors redefined as regular Berkeley error constants.
 * Copied from winsock.h
 */
#define EWOULDBLOCK             WSAEWOULDBLOCK
#define EINPROGRESS             WSAEINPROGRESS
#define EALREADY                WSAEALREADY
#define ENOTSOCK                WSAENOTSOCK
#define EDESTADDRREQ            WSAEDESTADDRREQ
#define EMSGSIZE                WSAEMSGSIZE
#define EPROTOTYPE              WSAEPROTOTYPE
#define ENOPROTOOPT             WSAENOPROTOOPT
#define EPROTONOSUPPORT         WSAEPROTONOSUPPORT
#define ESOCKTNOSUPPORT         WSAESOCKTNOSUPPORT
#define EOPNOTSUPP              WSAEOPNOTSUPP
#define EPFNOSUPPORT            WSAEPFNOSUPPORT
#define EAFNOSUPPORT            WSAEAFNOSUPPORT
#define EADDRINUSE              WSAEADDRINUSE
#define EADDRNOTAVAIL           WSAEADDRNOTAVAIL
#define ENETDOWN                WSAENETDOWN
#define ENETUNREACH             WSAENETUNREACH
#define ENETRESET               WSAENETRESET
#define ECONNABORTED            WSAECONNABORTED
#define ECONNRESET              WSAECONNRESET
#define ENOBUFS                 WSAENOBUFS
#define EISCONN                 WSAEISCONN
#define ENOTCONN                WSAENOTCONN
#define ESHUTDOWN               WSAESHUTDOWN
#define ETOOMANYREFS            WSAETOOMANYREFS
#define ETIMEDOUT               WSAETIMEDOUT
#define ECONNREFUSED            WSAECONNREFUSED
#define ELOOP                   WSAELOOP
#define ENAMETOOLONG            WSAENAMETOOLONG
#define EHOSTDOWN               WSAEHOSTDOWN
#define EHOSTUNREACH            WSAEHOSTUNREACH
#define ENOTEMPTY               WSAENOTEMPTY
#define EPROCLIM                WSAEPROCLIM
#define EUSERS                  WSAEUSERS
#define EDQUOT                  WSAEDQUOT
#define ESTALE                  WSAESTALE
#define EREMOTE                 WSAEREMOTE

#define EAGAIN EWOULDBLOCK // Added by @weizili at 20160610

#define gai_strerror gai_strerrorA

#endif // end of ZRTC_WIN

#if (defined(ZRTC_WIN) || defined(ZRTC_MAC))

#ifndef HAVE_MSG_NOSIGNAL
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif
#endif

#ifndef HAVE_MSG_DONTWAIT
#ifndef MSG_DONTWAIT
#define MSG_DONTWAIT 0
#endif
#endif

#endif


// Copied from libevent2.0/util-internal.h
#ifdef ZRTC_WIN

#define EVUTIL_ERR_RW_RETRIABLE(e)                  \
    ((e) == WSAEWOULDBLOCK || \
     (e) == WSAETIMEDOUT || \
     (e) == WSAEINTR)

#define EVUTIL_ERR_CONNECT_RETRIABLE(e)                 \
    ((e) == WSAEWOULDBLOCK || \
     (e) == WSAEINTR || \
     (e) == WSAEINPROGRESS || \
     (e) == WSAEINVAL)

#define EVUTIL_ERR_ACCEPT_RETRIABLE(e)          \
    EVUTIL_ERR_RW_RETRIABLE(e)

#define EVUTIL_ERR_CONNECT_REFUSED(e)                   \
    ((e) == WSAECONNREFUSED)

#else

/* True iff e is an error that means a read/write operation can be retried. */
#define EVUTIL_ERR_RW_RETRIABLE(e)              \
    ((e) == EINTR || (e) == EAGAIN)
/* True iff e is an error that means an connect can be retried. */
#define EVUTIL_ERR_CONNECT_RETRIABLE(e)         \
    ((e) == EINTR || (e) == EINPROGRESS)
/* True iff e is an error that means a accept can be retried. */
#define EVUTIL_ERR_ACCEPT_RETRIABLE(e)          \
    ((e) == EINTR || (e) == EAGAIN || (e) == ECONNABORTED)

/* True iff e is an error that means the connection was refused */
#define EVUTIL_ERR_CONNECT_REFUSED(e)                   \
    ((e) == ECONNREFUSED)

#endif


#ifdef ZRTC_WIN
#define socket_t intptr_t
#else
#define socket_t int
#endif

#define signal_number_t socket_t

namespace evloop {

std::string strerror(int e);

namespace sock {

socket_t CreateNonblockingSocket();
socket_t CreateUDPServer(int port);
void SetKeepAlive(socket_t fd, bool on);
void SetReuseAddr(socket_t fd);
void SetReusePort(socket_t fd);
void SetTCPNoDelay(socket_t fd, bool on);
void SetTimeout(socket_t fd, uint32_t timeout_ms);
std::string ToIPPort(const struct sockaddr_storage* ss);
std::string ToIPPort(const struct sockaddr* ss);
std::string ToIPPort(const struct sockaddr_in* ss);
std::string ToIP(const struct sockaddr* ss);


// @brief Parse a literal network address and return an internet protocol family address
// @param[in] address - A network address of the form "host:port" or "[host]:port"
// @return bool - false if parse failed.
bool ParseFromIPPort(const char* address, struct sockaddr_storage& ss);

inline struct sockaddr_storage ParseFromIPPort(const char* address) {
    struct sockaddr_storage ss;
    bool rc = ParseFromIPPort(address, ss);
    if (rc) {
        return ss;
    } else {
        memset(&ss, 0, sizeof(ss));
        return ss;
    }
}

// @brief Splits a network address of the form "host:port" or "[host]:port"
//  into host and port. A literal address or host name for IPv6
// must be enclosed in square brackets, as in "[::1]:80" or "[ipv6-host]:80"
// @param[in] address - A network address of the form "host:port" or "[ipv6-host]:port"
// @param[out] host -
// @param[out] port - the port in local machine byte order
// @return bool - false if the network address is invalid format
bool SplitHostPort(const char* address, std::string& host, int& port);

struct sockaddr_storage GetLocalAddr(socket_t sockfd);

inline bool IsZeroAddress(const struct sockaddr_storage* ss) {
    const char* p = reinterpret_cast<const char*>(ss);
    for (size_t i = 0; i < sizeof(*ss); ++i) {
        if (p[i] != 0) {
            return false;
        }
    }
    return true;
}

template<typename To, typename From>
inline To implicit_cast(From const& f) {
    return f;
}

inline const struct sockaddr* sockaddr_cast(const struct sockaddr_in* addr) {
    return static_cast<const struct sockaddr*>(evloop::sock::implicit_cast<const void*>(addr));
}

inline struct sockaddr* sockaddr_cast(struct sockaddr_in* addr) {
    return static_cast<struct sockaddr*>(evloop::sock::implicit_cast<void*>(addr));
}

inline struct sockaddr* sockaddr_cast(struct sockaddr_storage* addr) {
    return static_cast<struct sockaddr*>(evloop::sock::implicit_cast<void*>(addr));
}

inline const struct sockaddr_in* sockaddr_in_cast(const struct sockaddr* addr) {
    return static_cast<const struct sockaddr_in*>(evloop::sock::implicit_cast<const void*>(addr));
}

inline struct sockaddr_in* sockaddr_in_cast(struct sockaddr* addr) {
    return static_cast<struct sockaddr_in*>(evloop::sock::implicit_cast<void*>(addr));
}

inline struct sockaddr_in* sockaddr_in_cast(struct sockaddr_storage* addr) {
    return static_cast<struct sockaddr_in*>(evloop::sock::implicit_cast<void*>(addr));
}

inline struct sockaddr_in6* sockaddr_in6_cast(struct sockaddr_storage* addr) {
    return static_cast<struct sockaddr_in6*>(evloop::sock::implicit_cast<void*>(addr));
}

inline const struct sockaddr_in* sockaddr_in_cast(const struct sockaddr_storage* addr) {
    return static_cast<const struct sockaddr_in*>(evloop::sock::implicit_cast<const void*>(addr));
}

inline const struct sockaddr_in6* sockaddr_in6_cast(const struct sockaddr_storage* addr) {
    return static_cast<const struct sockaddr_in6*>(evloop::sock::implicit_cast<const void*>(addr));
}

inline const struct sockaddr_storage* sockaddr_storage_cast(const struct sockaddr* addr) {
    return static_cast<const struct sockaddr_storage*>(evloop::sock::implicit_cast<const void*>(addr));
}

inline const struct sockaddr_storage* sockaddr_storage_cast(const struct sockaddr_in* addr) {
    return static_cast<const struct sockaddr_storage*>(evloop::sock::implicit_cast<const void*>(addr));
}

inline const struct sockaddr_storage* sockaddr_storage_cast(const struct sockaddr_in6* addr) {
    return static_cast<const struct sockaddr_storage*>(evloop::sock::implicit_cast<const void*>(addr));
}

}

}

#ifdef ZRTC_WIN
int readv(socket_t sockfd, struct iovec* iov, int iovcnt);
#endif

#endif // ZRTC_EVENTSOCKETS_H
