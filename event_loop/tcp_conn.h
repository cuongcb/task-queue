/* 
 * File:   tcp_conn.h
 * Author: lap11894
 *
 * Created on January 16, 2019, 2:16 PM
 */

#ifndef ZRTC_TCPCONN_H
#define ZRTC_TCPCONN_H

#include <atomic>
#include <deque>
#include <mutex>

#include "zrtc/event_loop/tcp_callbacks.h"
#include "zrtc/event_loop/event_sockets.h"
#include "zrtc/network/TcpBuffer.h"
#include "zrtc/common/Stats.h"
#include "zrtc/event_loop/invoke_timer.h"

#include "zrtc/webrtc/system_wrappers/include/clock.h"

namespace evloop {

class EventLoop;
class FdChannel;
class InvokeTimer;

class TcpConn : public std::enable_shared_from_this<TcpConn> {
public:
    enum Type {
        kIncoming = 0, // The type of a TcpConn held by a Server
        kOutgoing = 1, // The type of a TcpConn held by a Client
    };
    enum Status {
        kDisconnected = 0,
        kConnecting = 1,
        kConnected = 2,
        kDisconnecting = 3,
    };
public:
    TcpConn(EventLoop* loop,
            const std::string& name,
            socket_t sockfd,
            const std::string& laddr,
            const std::string& raddr,
            uint64_t id);
    ~TcpConn();

    void Close();

	bool Send(const uint8_t *data, size_t len);
	bool Send(const zrtc::TcpBuffer::Ptr &buf);
public:
    EventLoop* loop() const {
        return loop_;
    }
    socket_t fd() const {
        return fd_;
    }
    uint64_t id() const {
        return id_;
    }
    // Return the remote peer's address with form "ip:port"
    const std::string& remote_addr() const {
        return remote_addr_;
    }
    const std::string& name() const {
        return name_;
    }
    bool IsConnected() const {
        return status_ == kConnected;
    }
    bool IsConnecting() const {
        return status_ == kConnecting;
    }
    bool IsDisconnected() const {
        return status_ == kDisconnected;
    }
    bool IsDisconnecting() const {
        return status_ == kDisconnecting;
    }
    Type type() const {
        return type_;
    }
    bool IsIncommingConn() const {
        return type_ == kIncoming;
    }
    Status status() const {
        return status_;
    }

    std::string AddrToString() const {
        if (IsIncommingConn()) {
            return "(" + remote_addr_ + "->" + local_addr_ + "(local))";
        } else {
            return "(" + local_addr_ + "(local)->" + remote_addr_ + ")";
        }
    }

    // @brief When it is an incoming connection, we need to preserve the
    //  connection for a while so that we can reply to it. And we set a timer
    //  to close the connection eventually.
    // @param[in] d - If d.IsZero() == true, we will close the connection immediately.
    void SetCloseDelayTime(uint32_t delay_ms) {
        assert(type_ == kIncoming);
        // This option is only available for the connection type kIncoming
        // Set the delay time to close the socket
        close_delay_ms_ = delay_ms;
    }

public:
    void SetTCPNoDelay(bool on);

    // TODO Add : SetLinger();
    void SetWriteCompleteCallback(const WriteCompleteCallback cb) {
		LOG_T_F(LS_INFO) << "";
        write_complete_fn_ = cb;
    }
	
	void SetWriteReadyCallback(const WriteReadyCallback cb) {
		LOG_T_F(LS_INFO) << "";
		write_ready_fn_ = cb;
	}
	
	int32_t GetInputStat() { return input_bw_stat_.getStatsAndReset(); }
	int32_t GetOutputStat() { return output_bw_stat_.getStatsAndReset(); }

public:
    // These methods are visible only for TcpClient and TcpServer.
    // We don't want the user layer to access these methods.
    void set_type(Type t) {
		LOG_T_F(LS_INFO) << "";
        type_ = t;
    }
    void SetMessageCallback(MessageCallback cb) {
		LOG_T_F(LS_INFO) << "";
        msg_fn_ = cb;
    }
    void SetConnectionCallback(ConnectionCallback cb) {
		LOG_T_F(LS_INFO) << "";
        conn_fn_ = cb;
    }
    void SetCloseCallback(CloseCallback cb) {
		LOG_T_F(LS_INFO) << "";
        close_fn_ = cb;
    }
    void OnAttachedToLoop();
    std::string StatusToString() const;
	
	int64_t rtt() const {
		return rtt_;
	}
	
	void EnableWrite();
	void DisableWrite();
private:
    void HandleRead();
    void HandleWrite();
    void HandleClose();
    void DelayClose();
    void HandleError(int err);
	void SendInLoop(const zrtc::TcpBuffer::Ptr &buf);

private:
//	enum SocketState {
//		SOCKET_RD_NONE = 0,
//		SOCKET_RD_FRAMESIZE = 1,
//		SOCKET_RD_DATA = 2,
//	};
	
    EventLoop * loop_;
    int fd_;
    uint64_t id_ = 0;
    std::string name_;
    std::string local_addr_; // the local address with form : "ip:port"
    std::string remote_addr_; // the remote address with form : "ip:port"
    std::unique_ptr<FdChannel> chan_;

	zrtc::Stats input_bw_stat_;
	zrtc::Stats output_bw_stat_;

    Type type_;
    std::atomic<Status> status_;

    // The delay time to close a incoming connection which has been shutdown by peer normally.
    // Default is 0 second which means we disable this feature by default.
    uint32_t close_delay_ms_; // default 0
    std::shared_ptr<InvokeTimer> delay_close_timer_; // The timer to delay close this TcpConn
	
	zrtc::TcpBuffer::Ptr buffer_;
//	SocketState socket_state_;

    ConnectionCallback conn_fn_; // This will be called to the user application layer
    MessageCallback msg_fn_; // This will be called to the user application layer
    WriteCompleteCallback write_complete_fn_; // This will be called to the user application layer
	WriteReadyCallback write_ready_fn_;
    CloseCallback close_fn_; // This will be called to TCPClient or TCPServer
	
private:
	struct PingPacket {
		uint32_t id;
		int64_t time;
		
		PingPacket(uint32_t id, int64_t time)
				: id(id)
				, time(time) { }
	};
	
	bool enable_ping_;
	std::shared_ptr<InvokeTimer> ping_timer_;
	webrtc::Clock *clock_;
	std::atomic<int64_t> rtt_;
//	int64_t last_time_sent_ping_;
	
	void Ping();
	void Pong();
	
	PingPacket CreatePingMessage() const {
		static uint32_t id = 0;
		int64_t now = clock_->TimeInMilliseconds();
		return PingPacket(id++, now);
	}
	
	void SerializePing(uint8_t *buffer, PingPacket ping);
	PingPacket DeserializePing(const uint8_t *buffer);
};
}

#endif /* ZRTC_TCPCONN_H */

