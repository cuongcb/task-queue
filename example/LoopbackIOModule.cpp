#include "zrtc/network/LoopbackIOModule.h"
#include "zrtc/common/ZBufferWrapper.h"

BEG_NSP_ZRTC();

LoopbackIOModule::LoopbackIOModule(): receive_fn_(nullptr), running_(false) {

}

LoopbackIOModule::~LoopbackIOModule() {
	
}

void LoopbackIOModule::Start() {
	if (running_) {
		return;
	}
	
	running_ = true;
	worker_thread_.start(*this);
}

void LoopbackIOModule::Stop() {
	if (!running_) {
		return;
	}

	running_ = false;
	
	loop_.Stop();
	
	try {
		worker_thread_.tryJoin(500);
	} catch (...) {
		
	}
}

void LoopbackIOModule::Send(const uint8_t* data, uint32_t len) {
	LoopbackPacketPtr packet = LoopbackPacketPtr(new LoopbackPacket(data, len));
	loop_.QueueInLoop(std::bind(&LoopbackIOModule::Process, this, std::move(packet)));
}

void LoopbackIOModule::RegisterOnReceiveCallback(OnReceiveCallback cb) {
	receive_fn_ = cb;
}

void LoopbackIOModule::run() {
	ZRTC_DEBUG("[CuongCB] Started loopback module.");
	loop_.Run();
}

void LoopbackIOModule::Process(const LoopbackPacketPtr& packet) {
//	if (packet_parser.Deserialize(packet->data(), packet->length()) == false) {
//		LOG(LS_VERBOSE) << "Invalid RTP packet.";
//		return;
//	}
	
	int8_t packet_type = (int8_t)(*(packet->data()));
	
	uint8_t *data = nullptr;
	uint32_t len = 0;
	switch (packet_type) {
		case ZRTPPacketType::VOICE_SEND:
		case ZRTPPacketType::VIDEO_SEND:
		case ZRTPPacketType::VOICE_FEC:
		case ZRTPPacketType::VOICE_NEW_FEC:
		{
			// get list rtp connection in call
			assert(packet->len_ >= 5);
			len = packet->length() - 4;
			data = packet->data() + 4;
			if (packet_type == ZRTPPacketType::VOICE_SEND) {
				data[0] = ZRTPPacketType::VOICE_RECEIVE;
			} else if (packet_type == ZRTPPacketType::VOICE_FEC) {
				data[0] = ZRTPPacketType::VOICE_FEC;
			} else if (packet_type == ZRTPPacketType::VOICE_NEW_FEC) {
				data[0] = ZRTPPacketType::VOICE_NEW_FEC;
			} else {
				data[0] = ZRTPPacketType::VIDEO_RECEIVE;
			}

			break;
		}
		case ZRTPPacketType::REQUEST:
		{
			loopbackinternal::MessageParser packet_parser;
			auto done = packet_parser.Deserialize(packet->data(), packet->length());
			if (done == false) {
				ZRTC_DEBUG("[CuongCB] Invalid packet.");
				return;
			}

			switch (packet_parser.cmd_) {
				case ZRTPRequestCommandType::CMD_CREATECALL:
				{
//					std::string addr = "120.138.69.88:3015";
					uint32_t kCreateCallResponseLength = 33;// + addr.length();
					uint8_t create_call_response_data[kCreateCallResponseLength];
					memset(create_call_response_data, 0, kCreateCallResponseLength);
					ZBufferWrapper wrapper(create_call_response_data, kCreateCallResponseLength);
					// msgType: 1
					wrapper.writeI8(ZRTPPacketType::RESPONSE);
					// version: 1
					wrapper.writeI8(packet_parser.version_);
					// checkSum: 4
					wrapper.writeI32(packet_parser.checksum_);
					// seqId: 4
					wrapper.writeI32(packet_parser.sequence_id_);
					// srcId: 4
					wrapper.writeI32(packet_parser.src_id_);
					// token: 4
					wrapper.writeI32(packet_parser.token_);
					// cmd: 2
					wrapper.writeI16(ZRTPRequestCommandType::CMD_CREATECALL);
					// subCmd: 1
					wrapper.writeI8(2);
					// retCode: 4
					wrapper.writeI32(0);
					// pParams: ...
					// zaloCallId: 4
					wrapper.writeUI32(10);
					// serverToken:4
					wrapper.writeUI32(300492);
					
					
					//wrapper.writeStringS2(addr);
					
					if (receive_fn_) {
						receive_fn_(create_call_response_data, kCreateCallResponseLength);
					}
					break;
				}
				case ZRTPRequestCommandType::CMD_JOINCALL:
				{
					break;
				}
				case ZRTPRequestCommandType::CMD_CHANGE_ADDRESS:
				{
					// get allocation

					break;
				}
				case ZRTPRequestCommandType::CMD_PING:
				{
					// get allocation

					break;
				}
				case ZRTPRequestCommandType::CMD_CLOSE:
				{
					// get allocation
					// TODO: Do nothing or stop?

					break;
				}
				case ZRTPRequestCommandType::CMD_FORWARD:
				{
					// response ack


					break;
				}
				case ZRTPRequestCommandType::CMD_ECHO_INCALL:
				{
					// get allocation

					break;
				}
				case ZRTPRequestCommandType::CMD_ECHO_ANONYMOUS:
				{

					break;
				}
				default:
				{

					break;
				}
			}
			break;
		}

			// Process RTCP 
		case ZRTPPacketType::VOICE_RTCP:
		case ZRTPPacketType::VIDEO_RTCP:
		{
			data = packet->data();
			len = packet->length();
			break;
		}
		default:
		{
			return;
		}
	}
	
	if (receive_fn_ && data) {
		receive_fn_(data, len);
	}
}

END_NSP_ZRTC();
