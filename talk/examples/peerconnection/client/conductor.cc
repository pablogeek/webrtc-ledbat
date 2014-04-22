/*
 * libjingle
 * Copyright 2012, Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "talk/examples/peerconnection/client/conductor.h"

#include <utility>
#include <fstream>
#include "talk/app/webrtc/videosourceinterface.h"
#include "talk/base/common.h"
#include "talk/base/json.h"
#include "talk/base/logging.h"
#include "talk/examples/peerconnection/client/defaults.h"
#include "talk/media/devices/devicemanager.h"
#include "talk/app/webrtc/datachannel.h"
#include "talk/app/webrtc/test/fakeconstraints.h"
#include "talk/base/messagehandler.h"
#include "talk/base/messagequeue.h"

// Names used for a IceCandidate JSON object.
const char kCandidateSdpMidName[] = "sdpMid";
const char kCandidateSdpMlineIndexName[] = "sdpMLineIndex";
const char kCandidateSdpName[] = "candidate";

// Names used for a SessionDescription JSON object.
const char kSessionDescriptionTypeName[] = "type";
const char kSessionDescriptionSdpName[] = "sdp";

class DummySetSessionDescriptionObserver
    : public webrtc::SetSessionDescriptionObserver {
 public:
  static DummySetSessionDescriptionObserver* Create() {
    return
        new talk_base::RefCountedObject<DummySetSessionDescriptionObserver>();
  }
  virtual void OnSuccess() {
    LOG(INFO) << __FUNCTION__;
  }
  virtual void OnFailure(const std::string& error) {
    LOG(INFO) << __FUNCTION__ << " " << error;
  }

 protected:
  DummySetSessionDescriptionObserver() {}
  ~DummySetSessionDescriptionObserver() {}
};

Conductor::Conductor(PeerConnectionClient* client, talk_base::Thread *t, 
  ChannelType channel_type, bool connect, char* sendfile)
  : peer_id_(-1),
    client_(client), t_(t), channel_type_(channel_type), connect_(connect), 
    sendfile_(sendfile), BUFLEN(64), instream_(NULL), ready_to_send_(false) {
  inbuffer_ = new char [BUFLEN];
  client_->RegisterObserver(this);
  thread_message_handler_ = new ThreadMessageHandler(this);
}

Conductor::~Conductor() {
  ASSERT(peer_connection_.get() == NULL);
}

bool Conductor::connection_active() const {
  return peer_connection_.get() != NULL;
}

void Conductor::Close() {
  client_->SignOut();
  DeletePeerConnection();
}

void Conductor::ReadFile() {
  ASSERT(t_ == talk_base::Thread::Current());
  if(sendfile_ != NULL && instream_ == NULL && ready_to_send_) {
    LOG(LS_INFO) << "Reading from file: " << sendfile_;
    instream_ = new std::ifstream(sendfile_, std::ifstream::binary);
  }

  if (instream_ && ready_to_send_) {
    // Bytes to be sent after a possible closing of instream_.
    // SendData release control over the thread and must be called after
    // nulling instream_ to avoid race condition.
    int send_bytes = -1;

    if (instream_->read(inbuffer_, BUFLEN)) {
      send_bytes = instream_->gcount();
    }

    if (instream_->eof()) {
      if (instream_->gcount() > 0) {
        send_bytes = instream_->gcount();
      }

      instream_->close();
      delete instream_;
      sendfile_ = NULL;
      instream_ = NULL;
    } else if (instream_->bad()) {
      LOG(LS_ERROR) << "Bad instream!";
      instream_->close();
      delete instream_;
      instream_ = NULL;
      sendfile_ = NULL;
    }

    if(send_bytes > 0) {
      SendData(inbuffer_, send_bytes);
    }
  }
}

bool Conductor::InitializePeerConnection() {
  ASSERT(peer_connection_factory_.get() == NULL);
  ASSERT(peer_connection_.get() == NULL);

  peer_connection_factory_  = webrtc::CreatePeerConnectionFactory();

  if (!peer_connection_factory_.get()) {
    DeletePeerConnection();
    return false;
  }

  webrtc::PeerConnectionInterface::IceServers servers;
  webrtc::PeerConnectionInterface::IceServer server;
  server.uri = GetPeerConnectionString();
  servers.push_back(server);
  switch (channel_type_) {
    case RTP: {
      global_constraints.SetAllowRtpDataChannels();
      break;
    }
    case SCTP: {
      global_constraints.SetAllowDtlsSctpDataChannels();
      break;
    }
    case LEDBAT: {
      global_constraints.SetAllowDtlsSctpDataChannels();
      global_constraints.SetAllowLedbatDataChannels();
    }
  }
  
  constraints.SetMandatoryReceiveAudio(false);
  constraints.SetMandatoryReceiveVideo(false);
  
  peer_connection_ = peer_connection_factory_->CreatePeerConnection(servers,
                                                                    &global_constraints,
                                                                    NULL,
                                                                    this);
  if (!peer_connection_.get()) {
    DeletePeerConnection();
  }

  if (connect_) {
    LOG(LS_INFO) << "Creating data channel on connecting client! ";
    data_channel_ = peer_connection_.get()->CreateDataChannel("TEST_DC_LABEL", NULL);
    data_channel_->RegisterObserver(this);
  }  
  return peer_connection_.get() != NULL;
}

void Conductor::DeletePeerConnection() {
  peer_connection_ = NULL;
  active_streams_.clear();
  peer_connection_factory_ = NULL;
  peer_id_ = -1;
}

void Conductor::OnDataChannel(webrtc::DataChannelInterface* data_channel) {
  remote_data_channel_ = data_channel;
  remote_data_channel_->RegisterObserver(this);
}

void Conductor::EnsureStreamingUI() {
  ASSERT(peer_connection_.get() != NULL);
}

//
// PeerConnectionObserver implementation.
//

void Conductor::OnError() {
  LOG(LS_ERROR) << __FUNCTION__;
  SendUIThreadCallback(PEER_CONNECTION_ERROR, NULL);
}

// Called when a remote stream is added
void Conductor::OnAddStream(webrtc::MediaStreamInterface* stream) {
  LOG(INFO) << __FUNCTION__ << " " << stream->label();

  stream->AddRef();
  SendUIThreadCallback(NEW_STREAM_ADDED, stream);
}

void Conductor::OnMessage(const webrtc::DataBuffer& buffer) {
    LOG(INFO) << "\n\n### Message received ###:\n " << std::string(buffer.data.data(), buffer.size()) << "\n";
};

void Conductor::OnRemoveStream(webrtc::MediaStreamInterface* stream) {
  LOG(INFO) << __FUNCTION__ << " " << stream->label();
  stream->AddRef();
  SendUIThreadCallback(STREAM_REMOVED, stream);
}

void Conductor::OnIceCandidate(const webrtc::IceCandidateInterface* candidate) {
  LOG(INFO) << __FUNCTION__ << " " << candidate->sdp_mline_index();
  Json::StyledWriter writer;
  Json::Value jmessage;

  jmessage[kCandidateSdpMidName] = candidate->sdp_mid();
  jmessage[kCandidateSdpMlineIndexName] = candidate->sdp_mline_index();
  std::string sdp;
  if (!candidate->ToString(&sdp)) {
    LOG(LS_ERROR) << "Failed to serialize candidate";
    return;
  }
  jmessage[kCandidateSdpName] = sdp;
  SendMessage(writer.write(jmessage));
}

//
// PeerConnectionClientObserver implementation.
//

void Conductor::OnSignedIn() {
  LOG(INFO) << __FUNCTION__;
}

void Conductor::OnDisconnected() {
  LOG(INFO) << __FUNCTION__;

  DeletePeerConnection();

  if(quit_) {
    DisconnectFromServer();
  }
}

void Conductor::OnPeerConnected(int id, const std::string& name) {
  LOG(INFO) << __FUNCTION__;
  // Refresh the list if we're showing it.
  fprintf(stderr, "Peer connected %d:%s!\n", id, name.c_str());
  
  
  if (connect_) {
    fprintf(stderr, "Connecting!\n");
    ConnectToPeer(id);
  } else {
    fprintf(stderr, "Listening!\n");
  }
}

void Conductor::OnPeerDisconnected(int id) {
  LOG(INFO) << __FUNCTION__;
  if (id == peer_id_) {
    LOG(INFO) << "Our peer disconnected";
    SendUIThreadCallback(PEER_CONNECTION_CLOSED, NULL);
  }
}

void Conductor::OnMessageFromPeer(int peer_id, const std::string& message) {
  ASSERT(peer_id_ == peer_id || peer_id_ == -1);
  ASSERT(!message.empty());

  if (!peer_connection_.get()) {
    ASSERT(peer_id_ == -1);
    peer_id_ = peer_id;

    if (!InitializePeerConnection()) {
      LOG(LS_ERROR) << "Failed to initialize our PeerConnection instance";
      client_->SignOut();
      return;
    }
  } else if (peer_id != peer_id_) {
    ASSERT(peer_id_ != -1);
    LOG(WARNING) << "Received a message from unknown peer while already in a "
                    "conversation with a different peer.";
    return;
  }

  Json::Reader reader;
  Json::Value jmessage;
  if (!reader.parse(message, jmessage)) {
    LOG(WARNING) << "Received unknown message. " << message;
    return;
  }
  std::string type;
  std::string json_object;

  GetStringFromJsonObject(jmessage, kSessionDescriptionTypeName, &type);
  if (!type.empty()) {
    std::string sdp;
    if (!GetStringFromJsonObject(jmessage, kSessionDescriptionSdpName, &sdp)) {
      LOG(WARNING) << "Can't parse received session description message.";
      return;
    }
    webrtc::SessionDescriptionInterface* session_description(
        webrtc::CreateSessionDescription(type, sdp));
    if (!session_description) {
      LOG(WARNING) << "Can't parse received session description message.";
      return;
    }
    LOG(INFO) << " Received session description :" << message;
    peer_connection_->SetRemoteDescription(
        DummySetSessionDescriptionObserver::Create(), session_description);
    if (session_description->type() ==
        webrtc::SessionDescriptionInterface::kOffer) {
      peer_connection_->CreateAnswer(this, &constraints);
    }
    return;
  } else {
    std::string sdp_mid;
    int sdp_mlineindex = 0;
    std::string sdp;
    if (!GetStringFromJsonObject(jmessage, kCandidateSdpMidName, &sdp_mid) ||
        !GetIntFromJsonObject(jmessage, kCandidateSdpMlineIndexName,
                              &sdp_mlineindex) ||
        !GetStringFromJsonObject(jmessage, kCandidateSdpName, &sdp)) {
      LOG(WARNING) << "Can't parse received message.";
      return;
    }
    talk_base::scoped_ptr<webrtc::IceCandidateInterface> candidate(
        webrtc::CreateIceCandidate(sdp_mid, sdp_mlineindex, sdp));
    if (!candidate.get()) {
      LOG(WARNING) << "Can't parse received candidate message.";
      return;
    }
    if (!peer_connection_->AddIceCandidate(candidate.get())) {
      LOG(WARNING) << "Failed to apply the received candidate";
      return;
    }
    LOG(INFO) << " Received candidate :" << message;
    return;
  }
}

void Conductor::OnMessageSent(int err) {
  // Process the next pending message if any.
  SendUIThreadCallback(SEND_MESSAGE_TO_PEER, NULL);
}

void Conductor::OnServerConnectionFailure() {
}

//
// MainWndCallback implementation.
//

void Conductor::StartLogin(const std::string& server, int port) {
  if (client_->is_connected())
    return;
  server_ = server;
  client_->Connect(server, port, GetPeerName());
}

void Conductor::DisconnectFromServer() {
  if (client_->is_connected())
    client_->SignOut();
}

void Conductor::ConnectToPeer(int peer_id) {
  ASSERT(peer_id_ == -1);
  ASSERT(peer_id != -1);

  if (peer_connection_.get()) {
    return;
  }

  if (InitializePeerConnection()) {
    peer_id_ = peer_id;
    peer_connection_->CreateOffer(this, &constraints);
  }
}

cricket::VideoCapturer* Conductor::OpenVideoCaptureDevice() {
  talk_base::scoped_ptr<cricket::DeviceManagerInterface> dev_manager(
      cricket::DeviceManagerFactory::Create());
  if (!dev_manager->Init()) {
    LOG(LS_ERROR) << "Can't create device manager";
    return NULL;
  }
  std::vector<cricket::Device> devs;
  if (!dev_manager->GetVideoCaptureDevices(&devs)) {
    LOG(LS_ERROR) << "Can't enumerate video devices";
    return NULL;
  }
  std::vector<cricket::Device>::iterator dev_it = devs.begin();
  cricket::VideoCapturer* capturer = NULL;
  for (; dev_it != devs.end(); ++dev_it) {
    capturer = dev_manager->CreateVideoCapturer(*dev_it);
    if (capturer != NULL)
      break;
  }
  return capturer;
}

void Conductor::DisconnectFromCurrentPeer() {
  LOG(INFO) << __FUNCTION__;
  if (peer_connection_.get()) {
    client_->SendHangUp(peer_id_);
    DeletePeerConnection();
  }
}

void Conductor::SendUIThreadCallback(int msg_id, void* data) {
    t_->Send(
      thread_message_handler_, msg_id, (talk_base::MessageData *)data);
}

void Conductor::UIThreadCallback(int msg_id, void* data) {
  switch (msg_id) {
    case PEER_CONNECTION_CLOSED:
      LOG(INFO) << "PEER_CONNECTION_CLOSED";
      DeletePeerConnection();

      ASSERT(active_streams_.empty());
      break;

    case SEND_DATA: {
      const webrtc::DataBuffer* buffer = (webrtc::DataBuffer*)data;
      data_channel_->Send(*buffer);
      break;
    }

    case READ_FILE: {
      ReadFile();
      break;
    }

    case SEND_MESSAGE_TO_PEER: {
      LOG(INFO) << "SEND_MESSAGE_TO_PEER";
      std::string* msg = reinterpret_cast<std::string*>(data);
      if (msg) {
        // For convenience, we always run the message through the queue.
        // This way we can be sure that messages are sent to the server
        // in the same order they were signaled without much hassle.
        pending_messages_.push_back(msg);
      }

      if (!pending_messages_.empty() && !client_->IsSendingMessage()) {
        msg = pending_messages_.front();
        pending_messages_.pop_front();

        if (!client_->SendToPeer(peer_id_, *msg) && peer_id_ != -1) {
          LOG(LS_ERROR) << "SendToPeer failed to peer_id " << peer_id_;
          DisconnectFromServer();
        }
        delete msg;
      }

      if (!peer_connection_.get())
        peer_id_ = -1;

      break;
    }

    case PEER_CONNECTION_ERROR:
      break;

    case NEW_STREAM_ADDED: {
      webrtc::MediaStreamInterface* stream =
          reinterpret_cast<webrtc::MediaStreamInterface*>(
          data);
      webrtc::VideoTrackVector tracks = stream->GetVideoTracks();
      // Only render the first track.
      if (!tracks.empty()) {
        webrtc::VideoTrackInterface* track = tracks[0];
      }
      stream->Release();
      break;
    }

    case STREAM_REMOVED: {
      // Remote peer stopped sending a stream.
      webrtc::MediaStreamInterface* stream =
          reinterpret_cast<webrtc::MediaStreamInterface*>(
          data);
      stream->Release();
      break;
    }

    default:
      ASSERT(false);
      break;
  }
}

void Conductor::OnSuccess(webrtc::SessionDescriptionInterface* desc) {
  Json::StyledWriter writer;
  Json::Value jmessage;
  jmessage[kSessionDescriptionTypeName] = desc->type();
  std::string sdp;
  desc->ToString(&sdp);
  jmessage[kSessionDescriptionSdpName] = sdp;
  peer_connection_->SetLocalDescription(
      DummySetSessionDescriptionObserver::Create(), webrtc::CreateSessionDescription(desc->type(), sdp));  
  SendMessage(writer.write(jmessage));
}

void Conductor::OnSignedOut() {
}

void Conductor::OnFailure(const std::string& error) {
    LOG(LERROR) << error;
}

void Conductor::SendMessage(const std::string& json_object) {
  std::string* msg = new std::string(json_object);
  SendUIThreadCallback(SEND_MESSAGE_TO_PEER, msg);
}
