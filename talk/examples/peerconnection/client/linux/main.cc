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
#include "talk/examples/peerconnection/client/flagdefs.h"
#include "talk/examples/peerconnection/client/linux/main_wnd.h"
#include "talk/examples/peerconnection/client/peer_connection_client.h"
#include "talk/base/basictypes.h"
#include "talk/base/ssladapter.h"
#include "talk/base/thread.h"

class CustomSocketServer : public talk_base::PhysicalSocketServer {
 public:
  CustomSocketServer(talk_base::Thread* thread, GtkMainWnd* wnd)
      : thread_(thread), wnd_(wnd), conductor_(NULL), client_(NULL) {}
  virtual ~CustomSocketServer() {}

  void set_client(PeerConnectionClient* client) { client_ = client; }
  void set_conductor(Conductor* conductor) { conductor_ = conductor; }

  // Override so that we can also pump the GTK message loop.
  virtual bool Wait(int cms, bool process_io) {    
    /*conductor_->SendUIThreadCallback(Conductor::READ_FILE, NULL);*/
    return talk_base::PhysicalSocketServer::Wait(0/*cms == -1 ? 1 : cms*/,
                                                 process_io);
  }

 protected:
  talk_base::Thread* thread_;
  GtkMainWnd* wnd_;
  Conductor* conductor_;
  PeerConnectionClient* client_;
};

int main(int argc, char* argv[]) {
  FlagList::SetFlagsFromCommandLine(&argc, argv, true);
  if (FLAG_help) {
    FlagList::Print(NULL, false);
    return 0;
  }

  Conductor::ChannelType channel_type;
  if(strcmp(FLAG_channeltype, "rtp") == 0) {
    channel_type = Conductor::RTP;
  } else if(strcmp(FLAG_channeltype, "sctp") == 0) {
    channel_type = Conductor::SCTP;
  } else if(strcmp(FLAG_channeltype, "ledbat") == 0) {
    channel_type = Conductor::LEDBAT;
  } else {
    printf("Error: %s is not a valid channel type. " \
      "Choose one of: rtp|sctp|ledbat.\n", FLAG_channeltype);
    return -1;
  }
  
  // Abort if the user specifies a port that is outside the allowed
  // range [1, 65535].
  if ((FLAG_port < 1) || (FLAG_port > 65535)) {
    printf("Error: %i is not a valid port.\n", FLAG_port);
    return -1;
  }

  talk_base::AutoThread auto_thread;
  talk_base::Thread* thread = talk_base::Thread::Current();
  CustomSocketServer socket_server(thread, NULL);
  thread->set_socketserver(&socket_server);

  talk_base::InitializeSSL();
  // Must be constructed after we set the socketserver.
  PeerConnectionClient client;
  talk_base::scoped_refptr<Conductor> conductor(
      new talk_base::RefCountedObject<Conductor>(&client, thread, channel_type,
        FLAG_connect, (char *)FLAG_sendfile, (char *)FLAG_receivefile));
  //Conductor(PeerConnectionClient* client, talk_base::Thread *t, ChannelType channel_type, bool connect, char* sendfile, char* receivefile);
  socket_server.set_client(&client);
  socket_server.set_conductor(conductor);
  conductor.get()->StartLogin("localhost", 8888);

  thread->ProcessMessages(talk_base::kForever);
  fprintf(stderr, "%s\n", "Stopping!");
  conductor.release();
  thread->set_socketserver(NULL);
  talk_base::CleanupSSL();
  return 0;
}

