#include "talk/media/base/mediaengine.h"
#include "talk/media/base/mediachannel.h"

namespace cricket {

class LoopbackDataEngine : public DataEngineInterface {
 public:
  LoopbackDataEngine();

  virtual DataMediaChannel* CreateChannel(DataChannelType data_channel_type);
  virtual const std::vector<DataCodec>& data_codecs() {
  	  return data_codecs_;
  }

 private:
  std::vector<DataCodec> data_codecs_;
};

class LoopbackDataMediaChannel : public DataMediaChannel {
  public:
  	LoopbackDataMediaChannel();
  	virtual ~LoopbackDataMediaChannel();

	virtual bool SetStartSendBandwidth(int bps) { return true; }
	virtual bool SetMaxSendBandwidth(int bps);
	virtual bool SetRecvRtpHeaderExtensions(
	  const std::vector<RtpHeaderExtension>& extensions) { return true; }
	virtual bool SetSendRtpHeaderExtensions(
	  const std::vector<RtpHeaderExtension>& extensions) { return true; }

	virtual bool SetSendCodecs(const std::vector<DataCodec>& codecs);
	virtual bool SetRecvCodecs(const std::vector<DataCodec>& codecs);

	virtual bool AddSendStream(const StreamParams& sp);
	virtual bool RemoveSendStream(uint32 ssrc);
	virtual bool AddRecvStream(const StreamParams& sp);
	virtual bool RemoveRecvStream(uint32 ssrc);


	virtual bool SetSend(bool send) {
		sending_ = send;
		return true;
	}
	virtual bool SetReceive(bool receive) {
		receiving_ = receive;
		return true;
	}

	virtual void OnPacketReceived(talk_base::Buffer* packet,
	                            const talk_base::PacketTime& packet_time);
	virtual void OnRtcpReceived(talk_base::Buffer* packet,
	                          const talk_base::PacketTime& packet_time) {}	

 	virtual void OnReadyToSend(bool ready) {}

	virtual bool SendData(const SendDataParams& params, 
						  const talk_base::Buffer& payload, 
						  SendDataResult* result);
  private:
	bool sending_;
	bool receiving_;
	int max_bps_;
	std::vector<DataCodec> recv_codecs_;
    std::vector<DataCodec> send_codecs_;
    std::vector<StreamParams> send_streams_;
    std::vector<StreamParams> recv_streams_;
};

} // namespace cricket