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

#ifndef PEERCONNECTION_SAMPLES_CLIENT_CONDUCTOR_H_
#define PEERCONNECTION_SAMPLES_CLIENT_CONDUCTOR_H_
#pragma once

#include <deque>
#include <map>
#include <set>
#include <string>

#include "talk/examples/peerconnection/client/main_wnd.h"
#include "talk/examples/peerconnection/client/peer_connection_client.h"
#include "talk/app/webrtc/mediastreaminterface.h"
#include "talk/app/webrtc/peerconnectioninterface.h"
#include "talk/base/scoped_ptr.h"
#include "talk/app/webrtc/peerconnection.h"
#include "talk/app/webrtc/datachannel.h"
#include "talk/app/webrtc/datachannelinterface.h"
#include "talk/app/webrtc/test/fakeconstraints.h"
#include "talk/base/messagehandler.h"

namespace webrtc {
class VideoCaptureModule;
}  // namespace webrtc

namespace cricket {
class VideoRenderer;
}  // namespace cricket

class ThreadMessageHandler;

class Conductor
  : public webrtc::PeerConnectionObserver,
    public webrtc::CreateSessionDescriptionObserver,
    public PeerConnectionClientObserver,
    public webrtc::DataChannelObserver,
    public MainWndCallback {
 public:
  enum CallbackID {
    MEDIA_CHANNELS_INITIALIZED = 1,
    PEER_CONNECTION_CLOSED,
    SEND_MESSAGE_TO_PEER,
    PEER_CONNECTION_ERROR,
    NEW_STREAM_ADDED,
    STREAM_REMOVED,
    CLOSE,
    READ_FILE,
  };

  enum ChannelType {
    RTP,
    SCTP,
    LEDBAT
  };

  Conductor(PeerConnectionClient* client, talk_base::Thread *t, 
    ChannelType channel_type, bool connect, char* sendfile, char* receivefile);

  bool connection_active() const;

  virtual void Close();
  virtual void StartLogin(const std::string& server, int port);
  virtual void UIThreadCallback(int msg_id, void* data);

  void ReadFile();


  bool SendData(char* data, size_t len) {
    return SendData(data, len, data_channel_);
  }
  bool SendData(char* data, size_t len, 
    talk_base::scoped_refptr<webrtc::DataChannelInterface> data_channel);

  void OnReadyToSend(bool writable) {
    LOG(LS_INFO) << "OnReadyToSend: " << writable;
  }

  bool SendData(std::string message) {
    return SendData(message, data_channel_);
  }

  bool SendData(std::string message, 
    talk_base::scoped_refptr<webrtc::DataChannelInterface> data_channel) {
    return SendData((char *)message.c_str(), message.length(), data_channel);
  } 

  void SendUIThreadCallback(int msg_id, void* data);

 protected:
  ~Conductor();
  bool InitializePeerConnection();
  void DeletePeerConnection();
  void EnsureStreamingUI();
  
  cricket::VideoCapturer* OpenVideoCaptureDevice();

  //
  // PeerConnectionObserver implementation.
  //
  virtual void OnError();
  virtual void OnStateChange();

  virtual void OnMessage(const webrtc::DataBuffer& buffer);
  virtual void OnAddStream(webrtc::MediaStreamInterface* stream);
  virtual void OnRemoveStream(webrtc::MediaStreamInterface* stream);
  virtual void OnRenegotiationNeeded() {}
  virtual void OnIceChange() {}
  virtual void OnIceCandidate(const webrtc::IceCandidateInterface* candidate);
  virtual void OnDataChannel(webrtc::DataChannelInterface* data_channel);
  //
  // PeerConnectionClientObserver implementation.
  //

  virtual void OnSignedIn();

  virtual void OnSignedOut();

  virtual void OnDisconnected();

  virtual void OnPeerConnected(int id, const std::string& name);

  virtual void OnPeerDisconnected(int id);

  virtual void OnMessageFromPeer(int peer_id, const std::string& message);

  virtual void OnMessageSent(int err);

  virtual void OnServerConnectionFailure();

  virtual void DisconnectFromServer();

  virtual void ConnectToPeer(int peer_id);

  virtual void DisconnectFromCurrentPeer();  

  // CreateSessionDescriptionObserver implementation.
  virtual void OnSuccess(webrtc::SessionDescriptionInterface* desc);
  virtual void OnFailure(const std::string& error);
  bool quit() { return quit_; }

 protected:
  // Send a message to the remote peer.
  void SendMessage(const std::string& json_object);

  int peer_id_;
  talk_base::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;
  talk_base::scoped_refptr<webrtc::PeerConnectionFactoryInterface>
      peer_connection_factory_;
  PeerConnectionClient* client_;
  MainWindow* main_wnd_;
  std::deque<std::string*> pending_messages_;
  std::map<std::string, talk_base::scoped_refptr<webrtc::MediaStreamInterface> >
      active_streams_;
  std::string server_;
  talk_base::scoped_refptr<webrtc::DataChannelInterface> datachannel_;
  ThreadMessageHandler* thread_message_handler_;
  talk_base::Thread *t_;
  bool quit_;
  talk_base::scoped_refptr<webrtc::DataChannelInterface> data_channel_;
  talk_base::scoped_refptr<webrtc::DataChannelInterface> remote_data_channel_;
  webrtc::FakeConstraints constraints;
  webrtc::FakeConstraints global_constraints;
  ChannelType channel_type_;
  bool connect_;
  char* sendfile_;
  char* receivefile_;
  bool ready_to_send_;

  std::ifstream* instream_;
  std::ofstream* outstream_;
  const size_t BUFLEN;
  char* inbuffer_;
  int receive_length_;
  int outstanding_buffer_;
};

class ThreadMessageHandler : public talk_base::MessageHandler {
 public:
  ThreadMessageHandler(Conductor* c) : c_(c) {
  }
  virtual void OnMessage(talk_base::Message* msg) {
    c_->UIThreadCallback(msg->message_id, (void *)msg->pdata);
  };
  Conductor *c_;
};

#endif  // PEERCONNECTION_SAMPLES_CLIENT_CONDUCTOR_H_
