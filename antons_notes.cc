/* 
To checkout a new copy of the gclient+git-repos this might work:

# Follow the guide at http://www.webrtc.org/reference/getting-started for installing prerequisites
gclient sync --force --revision 5844
cd trunk/
# Guide says to run ./build/install-build-deps.sh but it breaks for me half way through. Works anyway
git init
git remote add --track master origin https://github.com/Peerialism/browser-tranport.git
git fetch
git pull # Gives an error, but that's ok
git reset --hard
mv .git .git.bak # gclient tries to use our git instead of svn when running hooks, hide!
gclient runhooks
mv .git.bak .git
ninja -C out/Debug # Install a million packages that are not specified in the install instructions
cd measure/
mkdir d/
cat /dev/urandom > d/garbage # Create a garbage file for testing send. Ctrl+C when big enough
./server

# Terminal 2:
./recv

# Terminal 3:
./send

# Check that it worked
md5sum d/garbage out

# Hack away!


The flow through webrtc:
Creating a DataEngine and sending some data
1. Create a DataEngine of a specific type or of a hybrid type
2. Ask the DataEngine to create a new DataMediaChannel of a specific type, one of DataChannelType
   (DataEngine will return NULL if the DataChannelType is not supported by the engine)
3. Create a MediaChannel::NetworkInterface and set its destination
4. Create a DataReceiver which implements void OnDataReceived(const cricket::ReceiveDataParams& params, const char* data, size_t len)
5. Set the channels interface to point to the NetworkInterface
6. Set the channels SignalDataReceived to the DataReceiver::OnDataReceived 
7. Let the channel prepare for receiving and sending by calling SetSend(true) and SetReceive(true)
8. Send some data through the channel->SendData and expect the DataReceiver to receive it

Reaching the code that actually creates a DataEngine:
1. WebRtcSession::SetLocalDescription ->
2. WebRtcSession::CreateChannels ->
3. WebRtcSession::CreateDataChannel ->
4. ChannelManager::CreateDataChannel ->
5. ChannelManager::CreateDataChannel_w ->
6. DataChannel::SendData ->
8. DataMediaChannel::SendData == our code
*/

// Interface for a data engine
mediaengine.h:
  class DataEngineInterface {
   public:
    virtual ~DataEngineInterface() {}
    virtual DataMediaChannel* CreateChannel(DataChannelType type) = 0; // implemented by SctpDataEngine and RtpDataEngine

    virtual const std::vector<DataCodec>& data_codecs() = 0;
  }; 

  enum DataChannelType {
    DCT_NONE = 0,
    DCT_RTP = 1,
    DCT_SCTP = 2
  };                                                                   

// Construction and selection of choices of data engines
channelmanager.cc:
  static DataEngineInterface* ConstructDataEngine() {
  #ifdef HAVE_SCTP
    return new HybridDataEngine(new RtpDataEngine(), new SctpDataEngine());
  #else
    return new RtpDataEngine();
  #endif
  }

// Data engine that selects sub-dataengine depending on DataChannelType
// One more sub-dataengine, a ledbat-one could be added
hybriddataengine.h:
  virtual DataMediaChannel* CreateChannel(DataChannelType data_channel_type) {
      DataMediaChannel* channel = NULL;
      if (first_) {
        channel = first_->CreateChannel(data_channel_type);
      }
      if (!channel && second_) {
        channel = second_->CreateChannel(data_channel_type);
      }
      return channel;
  }


// Interface for the DataMediaChannel, what actually handles the transfer
mediachannel.h:
  class DataMediaChannel : public MediaChannel {
    public:
      // Abstract methods
      virtual bool SetSendCodecs(const std::vector<DataCodec>& codecs) = 0;
      virtual bool SetRecvCodecs(const std::vector<DataCodec>& codecs) = 0;
      virtual bool SetSend(bool send) = 0;
      virtual bool SetReceive(bool receive) = 0;
      virtual bool SendData(
          const SendDataParams& params,
          const talk_base::Buffer& payload,
          SendDataResult* result = NULL) = 0;
      // Called when a RTP packet is received.
      virtual void OnPacketReceived(talk_base::Buffer* packet,
                                    const talk_base::PacketTime& packet_time) = 0;
      // Called when a RTCP packet is received.
      virtual void OnRtcpReceived(talk_base::Buffer* packet,
                                  const talk_base::PacketTime& packet_time) = 0;
      // Called when the socket's ability to send has changed.
      virtual void OnReadyToSend(bool ready) = 0;
      // Creates a new outgoing media stream with SSRCs and CNAME as described
      // by sp.
      virtual bool AddSendStream(const StreamParams& sp) = 0;
      // Removes an outgoing media stream.
      // ssrc must be the first SSRC of the media stream if the stream uses
      // multiple SSRCs.
      virtual bool RemoveSendStream(uint32 ssrc) = 0;
      // Creates a new incoming media stream with SSRCs and CNAME as described
      // by sp.
      virtual bool AddRecvStream(const StreamParams& sp) = 0;
      // Removes an incoming media stream.
      // ssrc must be the first SSRC of the media stream if the stream uses
      // multiple SSRCs.
      virtual bool RemoveRecvStream(uint32 ssrc) = 0;

      // Mutes the channel.
      virtual bool MuteStream(uint32 ssrc, bool on) = 0;

      // Sets the RTP extension headers and IDs to use when sending RTP.
      virtual bool SetRecvRtpHeaderExtensions(
          const std::vector<RtpHeaderExtension>& extensions) = 0;
      virtual bool SetSendRtpHeaderExtensions(
          const std::vector<RtpHeaderExtension>& extensions) = 0;
      // Returns the absoulte sendtime extension id value from media channel.
      virtual int GetRtpSendTimeExtnId() const {
        return -1;
      }
      // Sets the initial bandwidth to use when sending starts.
      virtual bool SetStartSendBandwidth(int bps) = 0;
      // Sets the maximum allowed bandwidth to use when sending data.
      virtual bool SetMaxSendBandwidth(int bps) = 0;


      // Signal that should be sent with data when data is received
      sigslot::signal3<const ReceiveDataParams&,
                         const char*,
                         size_t> SignalDataReceived;

      // Not sure what this means
      sigslot::signal1<bool> SignalReadyToSend;  
  }.

// Example implementation of a DataEngine and DataMediaChannel that
// only passes data forward without changing or controlling it
loopbackdataengine_unittest.cc:
  // Create a data engine, create two data channels and send data between them
  // using fake network interfaces
  TEST_F(LoopbackDataMediaChannelTest, SendData)




// Where the magic actually happens:
// SetLocalDescription is also in the JavaScript API. My guess is that this is the
// entry point and above this there should only be setup and JS wrapping
webrtcsession.cc:
  // Create voice, video and data channels
  bool WebRtcSession::SetLocalDescription(SessionDescriptionInterface* desc,
                                          std::string* err_desc) {
    
    // .. validation and setup code ..

    // Transport and Media channels will be created only when offer is set.
    if (action == kOffer && !CreateChannels(local_desc_->description())) {
      // TODO(mallinath) - Handle CreateChannel failure, as new local description
      // is applied. Restore back to old description.
      return BadLocalSdp(desc->type(), kCreateChannelFailed, err_desc);
    }

  // Create voice, video and data channels
  bool WebRtcSession::CreateChannels(const SessionDescription* desc) {
    
    // .. create voice and video channels ..
    
    const cricket::ContentInfo* data = cricket::GetFirstDataContent(desc);
    if (data_channel_type_ != cricket::DCT_NONE &&
        data && !data->rejected && !data_channel_.get()) {
      if (!CreateDataChannel(data)) {
        LOG(LS_ERROR) << "Failed to create data channel.";
        return false;
      }
    }
  }

  // Create a DataChannel, with a DataMediaChannel of a specific type 
  // and hook up a receiver.
  // The DataMediaChannel is created by the DataEngine that the ChannelManager
  // owns.
  bool WebRtcSession::CreateDataChannel(const cricket::ContentInfo* content) {
    bool sctp = (data_channel_type_ == cricket::DCT_SCTP);
    data_channel_.reset(channel_manager_->CreateDataChannel(
        this, content->name, !sctp, data_channel_type_));
    if (!data_channel_.get()) {
      return false;
    }
    if (sctp) {
      mediastream_signaling_->OnDataTransportCreatedForSctp();
      data_channel_->SignalDataReceived.connect(
          this, &WebRtcSession::OnDataChannelMessageReceived);
    }
    return true;
  }

// Channelmanager creates a DataEngine and creates 
// new DataChannels (and DataMediaChannels). 
// A new DataChannel wraps a new DataMediaChannel in a layer of
// threading stuff and is created by ChannelManager::CreateDataChannel
channelmanager.cc:
  // Calls ChannelManager::CreateDataChannel_w through some threading maneuver
  DataChannel* ChannelManager::CreateDataChannel(
    BaseSession* session, const std::string& content_name,
    bool rtcp, DataChannelType channel_type) {
    return worker_thread_->Invoke<DataChannel*>(
        Bind(&ChannelManager::CreateDataChannel_w, this, session, content_name,
             rtcp, channel_type));
  }

  // Create a DataMediaChannel through data_media_engine_ and
  // wrap it in a DataChannel that handles threading stuff
  DataChannel* ChannelManager::CreateDataChannel_w(
    BaseSession* session, const std::string& content_name,
    bool rtcp, DataChannelType data_channel_type) {
  DataMediaChannel* media_channel = data_media_engine_->CreateChannel(
      data_channel_type);

  DataChannel* data_channel = new DataChannel(
      worker_thread_, media_channel,
      session, content_name, rtcp);
  return data_channel;
}

// When sending data DataChannel asks to DataMediaChannel::SendData
// to to it through some threading maneuver
channel.cc:
  bool DataChannel::SendData(const SendDataParams& params,
                             const talk_base::Buffer& payload,
                             SendDataResult* result) {
    return InvokeOnWorker(Bind(&DataMediaChannel::SendData,
                               media_channel(), params, payload, result));
  }
