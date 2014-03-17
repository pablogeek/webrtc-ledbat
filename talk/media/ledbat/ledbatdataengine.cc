#include "talk/media/ledbat/ledbatdataengine.h"

namespace cricket {

LedbatDataEngine::LedbatDataEngine() {

}

DataMediaChannel* LedbatDataEngine::CreateChannel(DataChannelType data_channel_type) {
	return new LedbatDataMediaChannel();
}

LedbatDataMediaChannel::LedbatDataMediaChannel() { 
}

LedbatDataMediaChannel::~LedbatDataMediaChannel() {
}

void LedbatDataMediaChannel::OnPacketReceived(talk_base::Buffer* packet, const talk_base::PacketTime& packet_time) {
	ReceiveDataParams params;
	SignalDataReceived(params, packet->data(), packet->length());
}


bool LedbatDataMediaChannel::SetMaxSendBandwidth(int bps) { 	
	max_bps_ = bps;
    return true;
}

bool LedbatDataMediaChannel::SetRecvCodecs(const std::vector<DataCodec>& codecs) {
    recv_codecs_ = codecs;
    return true;
  }
bool LedbatDataMediaChannel::SetSendCodecs(const std::vector<DataCodec>& codecs) {
    send_codecs_ = codecs;
    return true;
}


bool LedbatDataMediaChannel::AddSendStream(const StreamParams& stream) {
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

bool LedbatDataMediaChannel::RemoveSendStream(uint32 ssrc) {
  RemoveStreamBySsrc(&send_streams_, ssrc);
  return true;
}

bool LedbatDataMediaChannel::AddRecvStream(const StreamParams& stream) {
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

bool LedbatDataMediaChannel::RemoveRecvStream(uint32 ssrc) {
  RemoveStreamBySsrc(&recv_streams_, ssrc);
  return true;
}

bool LedbatDataMediaChannel::SendData(const SendDataParams& params,
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