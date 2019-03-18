/* 
 * File:   LoopbackIOModule.h
 * Author: lap11894
 *
 * Created on February 19, 2019, 1:50 PM
 */

#ifndef ZRTC_LOOPBACKIOMODULE_H
#define ZRTC_LOOPBACKIOMODULE_H

#include "zrtc/common/Common.h"
#include "zrtc/network/IOModuleInterface.h"
#include "zrtc/event_loop/event_loop.h"
#include "zrtc/base/Thread.h"
#include "zrtc/common/ZBufferWrapper.h"

BEG_NSP_ZRTC();

namespace loopbackinternal {

struct MessageParser {
public:
	int8_t msg_type_;
	int8_t version_;
	int32_t checksum_;
	int32_t sequence_id_;
	int32_t src_id_;
	int32_t token_;
	int16_t cmd_;
	int8_t sub_cmd_;
	const uint8_t* params_;
	uint32_t param_size_;

public:
	MessageParser()
	: msg_type_(ZRTPPacketType::UNKNOW)
	, version_(1)
	, checksum_(0)
	, sequence_id_(0)
	, src_id_(0)
	, token_(0)
	, cmd_(0)
	, sub_cmd_(0)
	, params_(NULL)
	, param_size_(0) {
	}

	~MessageParser() {
	}

public:

	uint32_t headerSize() const {
		return 21;
	}

	uint32_t dataSize() const {
		return headerSize() + param_size_;
	}
	
	int32_t buildCS() {
		return 0;
	}

public:

	bool Deserialize(const uint8_t* data, uint32_t len) {
		if (data == NULL) return false;
		if (!DeserializeInternal(data, len)) return false;
		// verify checkSum
		if (checksum_ != buildCS()) return false;
		return true;
	}

	std::string ToString() const {
		return std::string();
	}

private: // deserialize

	bool DeserializeInternal(const uint8_t* data, uint32_t len) {
		ZBufferWrapper buf(const_cast<uint8_t*> (data), len);
		// udpMsgType: 1
		if (!buf.readI8(msg_type_)) {
			return false;
		}
		// version: 1
		if (!buf.readI8(version_)) {
			return false;
		}
		// checkSum: 4
		if (!buf.readI32(checksum_)) {
			return false;
		}
		// seqId: 4
		if (!buf.readI32(sequence_id_)) {
			return false;
		}
		// srcId: 4
		if (!buf.readI32(src_id_)) {
			return false;
		}
		// token: 4
		if (!buf.readI32(token_)) {
			return false;
		}
		// cmd: 2
		if (!buf.readI16(cmd_)) {
			return false;
		}
		// subCmd: 1
		if (!buf.readI8(sub_cmd_)) {
			return false;
		}
		// pParams: ...
		param_size_ = buf.sizeRemain();
		params_ = buf.readRawBuf(param_size_);
		return buf.sizeRemain() == 0;
	}
};

}

class LoopbackIOModule: public IOModuleInterface, public Runnable {
public:
	LoopbackIOModule();
	~LoopbackIOModule();
	
	virtual void Start() override;
	virtual void Stop() override;

	virtual void Send(const uint8_t *data, uint32_t len) override;
	virtual void RegisterOnReceiveCallback(OnReceiveCallback cb) override;

protected:
	virtual void run() override;


	
private:
	struct LoopbackPacket {
		explicit LoopbackPacket(const uint8_t *data, size_t len)
			: data_(new uint8_t[len]), len_(len) {
			memcpy(data_.get(), data, len);
		}

		~LoopbackPacket() {
			
		}
		
		uint8_t * data() const {
			return data_.get();
		}
		
		size_t length() const {
			return len_;
		}

	private:
		std::unique_ptr<uint8_t []> data_;
		size_t len_;
	};

	OnReceiveCallback receive_fn_;
	
	evloop::EventLoop loop_;
	PocoThread worker_thread_;
	
	std::atomic<bool> running_;

private:
	typedef std::shared_ptr<LoopbackPacket> LoopbackPacketPtr;
	void Process(const LoopbackPacketPtr &packet);
};

END_NSP_ZRTC();

#endif /* ZRTC_LOOPBACKIOMODULE_H */

