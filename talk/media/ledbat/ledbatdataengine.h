#include "talk/media/base/mediaengine.h"
#include "talk/media/base/mediachannel.h"
#include "talk/base/messagehandler.h"
#include "talk/base/messagequeue.h"
#include "libutp/utp.h"

namespace cricket {

class LedbatDataEngine : public DataEngineInterface {
 public:
  LedbatDataEngine();

  virtual DataMediaChannel* CreateChannel(DataChannelType data_channel_type);
  virtual const std::vector<DataCodec>& data_codecs() {
  	  return data_codecs_;
  }

 private:
  std::vector<DataCodec> data_codecs_;
};

class LedbatDataMediaChannel : public DataMediaChannel, public talk_base::MessageHandler {
  public:
  enum {
      MSG_CHECK_TIMEOUTS,
      MSG_DEFERRED_ACKS
    };

  	LedbatDataMediaChannel();
  	virtual ~LedbatDataMediaChannel();
  
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

	virtual bool SetSend(bool send);

	virtual bool SetReceive(bool receive) {
    receiving_ = receive;
    return true;
	}

	virtual bool NeedsChannelSetup() { return true; }

	int utp_state();

	virtual void OnPacketReceived(talk_base::Buffer* packet,
	                            const talk_base::PacketTime& packet_time);
	virtual void OnRtcpReceived(talk_base::Buffer* packet,
	                          const talk_base::PacketTime& packet_time) {}	

  virtual void OnMessage(talk_base::Message* message);

 	virtual void OnReadyToSend(bool ready);

	virtual bool SendData(const SendDataParams& params, 
						  const talk_base::Buffer& payload, 
						  SendDataResult* result);

	void SetDebugName(std::string name);

	uint64 OnUTPSendTo(utp_callback_arguments *a);

	uint64 OnUTPLog(utp_callback_arguments *a);

	uint64 OnUTPStateChange(utp_callback_arguments *a);

	uint64 OnUTPAccept(utp_callback_arguments *a);

	uint64 OnUTPRead(utp_callback_arguments *a);
	
  private:
  	void Log(const char *fmt, ...);
    bool SendOutstandingBuffer(SendDataResult* result);

	bool sending_;
	bool receiving_;
	int max_bps_;
	std::vector<DataCodec> recv_codecs_;
    std::vector<DataCodec> send_codecs_;
    std::vector<StreamParams> send_streams_;
    std::vector<StreamParams> recv_streams_;

    utp_context *ctx_;
    utp_socket *utp_sock_;
    std::string debug_name_;
    bool debug_log_; 
    struct sockaddr_in ip4addr_; 
    int utp_state_;  
    char *send_buffer_;
    char *send_buffer_index_;
    size_t send_buffer_length_;
};

} // namespace cricket