#include "talk/media/base/loopbackdataengine.h"

namespace cricket {

LoopbackDataEngine::LoopbackDataEngine() {

}

DataMediaChannel* LoopbackDataEngine::CreateChannel(DataChannelType data_channel_type) {
	return new LoopbackDataMediaChannel();
}

LoopbackDataMediaChannel::LoopbackDataMediaChannel() { 
}

LoopbackDataMediaChannel::~LoopbackDataMediaChannel() {
}

void LoopbackDataMediaChannel::OnPacketReceived(talk_base::Buffer* packet, const talk_base::PacketTime& packet_time) {
	ReceiveDataParams params;
	SignalDataReceived(params, packet->data(), packet->length());
}


bool LoopbackDataMediaChannel::SetMaxSendBandwidth(int bps) { 	
	max_bps_ = bps;
    return true;
}

bool LoopbackDataMediaChannel::SetRecvCodecs(const std::vector<DataCodec>& codecs) {
    recv_codecs_ = codecs;
    return true;
  }
bool LoopbackDataMediaChannel::SetSendCodecs(const std::vector<DataCodec>& codecs) {
    send_codecs_ = codecs;
    return true;
}


bool LoopbackDataMediaChannel::AddSendStream(const StreamParams& stream) {
  if (!stream.has_ssrcs()) {
    return false;
  }

  StreamParams found_stream;
  if (GetStreamBySsrc(send_streams_, stream.first_ssrc(), &found_stream)) {
    return false;
  }

  send_streams_.push_back(stream);
  return true;
}

bool LoopbackDataMediaChannel::RemoveSendStream(uint32 ssrc) {
  RemoveStreamBySsrc(&send_streams_, ssrc);
  return true;
}

bool LoopbackDataMediaChannel::AddRecvStream(const StreamParams& stream) {
  if (!stream.has_ssrcs()) {
    return false;
  }

  StreamParams found_stream;
  if (GetStreamBySsrc(recv_streams_, stream.first_ssrc(), &found_stream)) {
    return false;
  }

  recv_streams_.push_back(stream);
  return true;
}

bool LoopbackDataMediaChannel::RemoveRecvStream(uint32 ssrc) {
  RemoveStreamBySsrc(&recv_streams_, ssrc);
  return true;
}

bool LoopbackDataMediaChannel::SendData(const SendDataParams& params,
              const talk_base::Buffer& payload,
              SendDataResult* result) {
	talk_base::Buffer payload_copy = payload;
	if(MediaChannel::SendPacket(&payload_copy)) {
		*result = cricket::SDR_SUCCESS;
		return true;
	} else {
		return false;
	}
}

} // namespace cricket