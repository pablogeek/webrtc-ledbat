#include "talk/media/ledbat/ledbatdataengine.h"
#include "libutp/utp.h"
#include <unistd.h>

namespace cricket {

// Forward declare callbacks that are set in UTPNode::SetUp() 
uint64 ledbat_callback_on_state_change(utp_callback_arguments *a);
uint64 ledbat_callback_sendto(utp_callback_arguments *a);
uint64 ledbat_callback_log(utp_callback_arguments *a);
uint64 ledbat_callback_on_accept(utp_callback_arguments *a);
uint64 ledbat_callback_on_read(utp_callback_arguments *a);

LedbatDataEngine::LedbatDataEngine() {
}

DataMediaChannel* LedbatDataEngine::CreateChannel(
    DataChannelType data_channel_type) {
	return new LedbatDataMediaChannel();
}

LedbatDataMediaChannel::LedbatDataMediaChannel() {
  sending_ = false;
  debug_log_ = false;
  utp_state_ = 0;

  ctx_ = utp_init(2);

  utp_set_callback(ctx_, UTP_LOG,             &ledbat_callback_log);
  utp_set_callback(ctx_, UTP_ON_STATE_CHANGE, &ledbat_callback_on_state_change);
  utp_set_callback(ctx_, UTP_SENDTO,          &ledbat_callback_sendto);
  utp_set_callback(ctx_, UTP_ON_ACCEPT,       &ledbat_callback_on_accept);
  utp_set_callback(ctx_, UTP_ON_READ,         &ledbat_callback_on_read);
  
  utp_context_set_userdata(ctx_, (void*)this);

  utp_context_set_option(ctx_, UTP_LOG_NORMAL, 1);
  utp_context_set_option(ctx_, UTP_LOG_MTU,    1);
  utp_context_set_option(ctx_, UTP_LOG_DEBUG,  1); 

  utp_sock_ = utp_create_socket(ctx_);

  ip4addr_.sin_family = AF_INET;
  ip4addr_.sin_port = htons(0);
  inet_pton(AF_INET, "0.0.0.0", &ip4addr_.sin_addr);   
}

LedbatDataMediaChannel::~LedbatDataMediaChannel() {
}

bool LedbatDataMediaChannel::SetSend(bool send) {
  if (!sending_) {
    utp_connect(utp_sock_, (sockaddr*)&ip4addr_, sizeof(ip4addr_));
    sending_ = true;
    return true;
  } else {
    return false;
  }
}

int LedbatDataMediaChannel::utp_state() {
  return utp_state_;
}

void LedbatDataMediaChannel::OnPacketReceived(talk_base::Buffer* packet, 
    const talk_base::PacketTime& packet_time) {
	ReceiveDataParams params;

  // TODO: Investigate Buffer operator= to make sure it does a copy on
  // pointer dereference
  talk_base::Buffer packet_copy = *packet; 

	if (!utp_process_udp(ctx_, (unsigned char*)packet_copy.data(), 
      packet->length(), (struct sockaddr *)&ip4addr_, 
      (socklen_t)sizeof(ip4addr_))) {
    Log("UDP packet not handled by UTP.  Ignoring.");     
  }
}

bool LedbatDataMediaChannel::SetMaxSendBandwidth(int bps) { 	
  max_bps_ = bps;
  return true;
}

bool LedbatDataMediaChannel::SetRecvCodecs(
    const std::vector<DataCodec>& codecs) {
  recv_codecs_ = codecs;
  return true;
}

bool LedbatDataMediaChannel::SetSendCodecs(
    const std::vector<DataCodec>& codecs) {
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

void LedbatDataMediaChannel::Log(const char *fmt, ...) {
  if(debug_log_) {
    va_list ap;
    fprintf(stderr, "%s: ", debug_name_.c_str());
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
  }
}

void LedbatDataMediaChannel::SetDebugName(std::string name) {
  debug_log_ = true;
  debug_name_ = name;
}

bool LedbatDataMediaChannel::SendData(const SendDataParams& params,
                                      const talk_base::Buffer& payload,
                                      SendDataResult* result) {
  if (result) {
    // Preset |result| to assume an error.  If SendData succeeds, we'll
    // overwrite |*result| once more at the end.
    *result = SDR_ERROR;
  }

  char *send_buffer_ = strdup(payload.data());
  char *send_buffer_index_ = send_buffer_;
  size_t buf_len_ = payload.length();

	while (send_buffer_index_ < send_buffer_ + buf_len_) {
    size_t sent;

    sent = utp_write(utp_sock_, send_buffer_index_, 
      send_buffer_ + buf_len_ - send_buffer_index_);
    if (sent == 0) {
      Log("Socket no longer writable");
      return false;
    }

    send_buffer_index_ += sent;

    if (send_buffer_index_ == send_buffer_ + buf_len_) {
      Log("wrote %zd bytes; buffer now empty", sent);
      send_buffer_index_ = send_buffer_;
    } else {
      Log("wrote %zd bytes; %d bytes left in buffer", sent, 
        send_buffer_ + buf_len_ - send_buffer_index_);
    }
    *result = cricket::SDR_SUCCESS;
    return true;
  }
}

// Send data over the wire when asked by UTP
// TODO: Do we need to keep the memory/free it or does UTP copy?
uint64 LedbatDataMediaChannel::OnUTPSendTo(utp_callback_arguments *a) {
  talk_base::Buffer buffer = talk_base::Buffer(a->buf, a->len);
  if(MediaChannel::SendPacket(&buffer)) {
    return 0;
  } else {
    return 1;
  }
}

uint64 LedbatDataMediaChannel::OnUTPLog(utp_callback_arguments *a) {
  Log("utp: %s", a->buf);
  return 0;
}

uint64 LedbatDataMediaChannel::OnUTPStateChange(utp_callback_arguments *a) {
  Log("State %d: %s", a->state, utp_state_names[a->state]);

  utp_state_ = a->state;

  switch (a->state) {
    case UTP_STATE_CONNECT:
    case UTP_STATE_WRITABLE:
      break;
    case UTP_STATE_EOF:
      Log("Received EOF from socket; closing");
      utp_close(a->socket);
      break;

    case UTP_STATE_DESTROYING:
      Log("UTP socket is being destroyed; exiting");
      break;
  }
  return 0;
}

uint64 LedbatDataMediaChannel::OnUTPAccept(utp_callback_arguments *a) {
  utp_socket *s = a->socket;
  Log("Accepted inbound socket %p", s);
  return 0;     
}

// Log received data 
uint64 LedbatDataMediaChannel::OnUTPRead(utp_callback_arguments *a) {
  Log("Received message: %s", a->buf);
  utp_read_drained(a->socket);

  ReceiveDataParams params;
  SignalDataReceived(params, (char *)a->buf, a->len);
  return 0;
}

LedbatDataMediaChannel* ledbat_get_callback_media_channel(
    utp_callback_arguments *a) {
  void *user_data = utp_context_get_userdata(a->context);
  return (LedbatDataMediaChannel*)user_data; 
}

uint64 ledbat_callback_on_read(utp_callback_arguments *a)
{
  LedbatDataMediaChannel* self = ledbat_get_callback_media_channel(a);
  return self->OnUTPRead(a);
}

uint64 ledbat_callback_on_state_change(utp_callback_arguments *a)
{
  LedbatDataMediaChannel* self = ledbat_get_callback_media_channel(a);
  return self->OnUTPStateChange(a);
}

uint64 ledbat_callback_log(utp_callback_arguments *a)
{
  LedbatDataMediaChannel* self = ledbat_get_callback_media_channel(a);
  self->OnUTPLog(a);
  return 0;
}

uint64 ledbat_callback_sendto(utp_callback_arguments *a)
{
  LedbatDataMediaChannel* self = ledbat_get_callback_media_channel(a);
  self->OnUTPSendTo(a);
  return 0;
}

uint64 ledbat_callback_on_accept(utp_callback_arguments *a)
{
  LedbatDataMediaChannel* self = ledbat_get_callback_media_channel(a);
  return self->OnUTPAccept(a);
}

} // namespace cricket