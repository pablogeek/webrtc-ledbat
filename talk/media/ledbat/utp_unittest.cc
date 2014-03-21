#include <string>
#include "talk/base/gunit.h"
#include "talk/media/base/fakenetworkinterface.h"
#include "libutp/utp.h"
#include <unistd.h>

// Forward declare callbacks that are set in UTPNode::SetUp() 
uint64 callback_on_state_change(utp_callback_arguments *a);
uint64 callback_sendto(utp_callback_arguments *a);
uint64 callback_log(utp_callback_arguments *a);
uint64 callback_on_accept(utp_callback_arguments *a);
uint64 callback_on_read(utp_callback_arguments *a);

// An endpoint in a UTP connection over UDP. Can act as either a host or client
// with Listen() or Connect() respectively.
class UTPNode {
  public:
	UTPNode() {
		debug_log_ = false; // Enabled by setting SetDebugName()
		SetUp();
	}    

    void SetDebugName(std::string name) {
    	debug_log_ = true;
    	debug_name_ = name;
    }    

    // Create a socket fd_ and bind it to address_:port_. If port_ is an empty string, bind to port NULL to
    // allocate a random port (for client connections)
    void Bind() {
		fd_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);	
		EXPECT_TRUE(fd_ >= 0);
		int error;
		if(error = getaddrinfo(address_.c_str(), port_.length() ? port_.c_str() : NULL, &hints_, &res_)) {
			Log("gai_strerror: %s", gai_strerror(error));
			EXPECT_FALSE(error);
		}
		freeaddrinfo(res_);

		EXPECT_FALSE(bind(fd_, res_->ai_addr, res_->ai_addrlen));

		socklen_t len = sizeof(sin_);
		EXPECT_TRUE(getsockname(fd_, (struct sockaddr *) &sin_, &len) == 0);
		Log("Bound to local %s:%d", inet_ntoa(sin_.sin_addr), ntohs(sin_.sin_port));	    	
    }

    // Create a socket fd_ bound to address:port 
    void Listen(std::string address, std::string port) {
    	address_ = address;
    	port_ = port;
    	Bind();
    }

    // Create a utp_socket and connect it to address:remote_port.
    // Create a socket fd_ bound to address:<random_port> for receiving data.
    void Connect(std::string address, std::string remote_port) {
    	address_ = address;
    	port_ = "";
    	Bind();

		utp_sock_ = utp_create_socket(ctx_);
		EXPECT_TRUE(utp_sock_);

		EXPECT_FALSE(getaddrinfo(address_.c_str(), remote_port.c_str(), &hints_, &res_));
		sinp_ = (struct sockaddr_in *)res_->ai_addr;
		Log("Connecting to %s:%d", inet_ntoa(sinp_->sin_addr), ntohs(sinp_->sin_port));

		EXPECT_FALSE(utp_connect(utp_sock_, res_->ai_addr, res_->ai_addrlen));
		freeaddrinfo(res_);
    }

    // Receive data over UDP and give it to UTP
    void Recv() {
    	unsigned char socket_data[4096];
    	struct sockaddr_in src_addr;
		socklen_t addrlen = sizeof(src_addr);

		ssize_t len = recvfrom(fd_, socket_data, sizeof(socket_data), MSG_DONTWAIT, (struct sockaddr *)&src_addr, &addrlen);
		if (len < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				utp_issue_deferred_acks(ctx_);
				Log("recvfrom: %s (EAGAIN/EWOULDBLOCK)", strerror(errno));
				return;
			} else {
				EXPECT_TRUE(len >= 0);
			}
			Log("recvfrom: %s", strerror(errno));
		}

		Log("Received %d byte UDP packet from %s:%d", len, inet_ntoa(src_addr.sin_addr), ntohs(src_addr.sin_port));

		if (!utp_process_udp(ctx_, socket_data, len, (struct sockaddr *)&src_addr, addrlen)) {
			Log("UDP packet not handled by UTP.  Ignoring.");    	
		}
    }

    void Log(const char *fmt, ...) {
    	if (debug_log_) {
	    	va_list ap;
	    	fprintf(stderr, "%s: ", debug_name_.c_str());
	    	va_start(ap, fmt);
	    	vfprintf(stderr, fmt, ap);
	    	va_end(ap);
	    	fprintf(stderr, "\n");
    	}
    }

    std::string last_message_received() {
    	return last_message_received_;
    }

    // Ask UTP to send a message through utp_sock_. Only supported when acting as a client and
    // Connect() has been called
    void SendMessage(std::string message) {
    	// TODO: With great power comes great responsibility (don't forget to free the duplicate)
    	send_buffer_ = strdup(message.c_str()); 
    	send_buffer_index_ = send_buffer_;
    	buf_len_ = strlen(send_buffer_);
    	WriteBuffer();
    }

    // Send data over the wire when asked by UTP
    uint64 OnUTPSendTo(utp_callback_arguments *a) {
		struct sockaddr_in *sin = (struct sockaddr_in *) a->address;
		Log("sendto: %zd byte packet to %s:%d%s", a->address_len, inet_ntoa(sin->sin_addr), ntohs(sin->sin_port),
					(a->flags & UTP_UDP_DONTFRAG) ? "  (DF bit requested, but not yet implemented)" : "");
		
		sendto(fd_, a->buf, a->len, 0, a->address, a->address_len);
		return 0;
    }

    uint64 OnUTPLog(utp_callback_arguments *a) {
		Log("utp: %s", a->buf);
		return 0;
    }

	uint64 OnUTPStateChange(utp_callback_arguments *a) {
    	Log("State %d: %s", a->state, utp_state_names[a->state]);

		switch (a->state) {
			case UTP_STATE_CONNECT:
			case UTP_STATE_WRITABLE:
				Log("UTP_STATE_CONNECT/UTP_STATE_WRITABLE");
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

    uint64 OnUTPAccept(utp_callback_arguments *a) {
		utp_socket *s = a->socket;
		Log("Accepted inbound socket %p", s);
		return 0;    	
    }

    // Log received data 
    uint64 OnUTPRead(utp_callback_arguments *a) {
		const unsigned char *p;
		p = a->buf;
		last_message_received_ = std::string((const char*)a->buf, a->len);
		Log("Received message: %s", last_message_received_.c_str());
		utp_read_drained(a->socket);
		return 0;
    }

  private:
  	// Create a utp_context, set up callbacks, logging options and initialize the socket options to UDP
	void SetUp() {
		ctx_ = utp_init(2);

		utp_set_callback(ctx_, UTP_LOG, 			&callback_log);
		utp_set_callback(ctx_, UTP_ON_STATE_CHANGE,	&callback_on_state_change);
		utp_set_callback(ctx_, UTP_SENDTO,			&callback_sendto);
		utp_set_callback(ctx_, UTP_ON_ACCEPT,		&callback_on_accept);
		utp_set_callback(ctx_, UTP_ON_READ,			&callback_on_read);

		utp_context_set_userdata(ctx_, (void*)this);

		utp_context_set_option(ctx_, UTP_LOG_NORMAL, 1);
		utp_context_set_option(ctx_, UTP_LOG_MTU,    1);
		utp_context_set_option(ctx_, UTP_LOG_DEBUG,  1);


		memset(&hints_, 0, sizeof(hints_));
		hints_.ai_family = AF_INET;
		hints_.ai_socktype = SOCK_DGRAM;
		hints_.ai_protocol = IPPROTO_UDP;		
    }

    // Write all data in send_buffer_ to UTP using utp_write
    // TODO: Make sure that the memory in send_buffer_ is allocated and unchanged during the time utp_write needs it
	void WriteBuffer() {
		while (send_buffer_index_ < send_buffer_ + buf_len_) {
			size_t sent;

			sent = utp_write(utp_sock_, send_buffer_index_, send_buffer_ + buf_len_ - send_buffer_index_);
			if (sent == 0) {
				Log("Socket no longer writable");
				return;
			}

			send_buffer_index_ += sent;

			if (send_buffer_index_ == send_buffer_ + buf_len_) {
					Log("wrote %zd bytes; buffer now empty", sent);
					send_buffer_index_ = send_buffer_;
					buf_len_ = 0;
			} else {
				Log("wrote %zd bytes; %d bytes left in buffer", sent, send_buffer_ + buf_len_ - send_buffer_index_);
			}
		}
    }    

	std::string address_, port_;
  	struct addrinfo hints_, *res_;
	struct sockaddr_in sin_, *sinp_;
	utp_context *ctx_;
	utp_socket *utp_sock_;
	char *send_buffer_;
	char *send_buffer_index_;
	int buf_len_;
	std::string debug_name_;
    int fd_;
    bool debug_log_;
    std::string last_message_received_;
};

UTPNode* get_callback_utp_node(utp_callback_arguments *a) {
	void *user_data = utp_context_get_userdata(a->context);
	return (UTPNode*)user_data;	
}

uint64 callback_on_read(utp_callback_arguments *a)
{
	UTPNode* self = get_callback_utp_node(a);
	return self->OnUTPRead(a);
}

uint64 callback_on_state_change(utp_callback_arguments *a)
{
	UTPNode* self = get_callback_utp_node(a);
	return self->OnUTPStateChange(a);
}

uint64 callback_log(utp_callback_arguments *a)
{
	UTPNode* self = get_callback_utp_node(a);
	self->OnUTPLog(a);
	return 0;
}

uint64 callback_sendto(utp_callback_arguments *a)
{
	UTPNode* self = get_callback_utp_node(a);
	self->OnUTPSendTo(a);
	return 0;
}

uint64 callback_on_accept(utp_callback_arguments *a)
{
	UTPNode* self = get_callback_utp_node(a);
	return self->OnUTPAccept(a);
}

class UTPTest : public testing::Test {
  protected:
	virtual void SetUp() {
    }

    virtual void TearDown() {
  	}	
};

TEST_F(UTPTest, CreateUTPContext) {
	utp_context *ctx;
	ctx = utp_init(2);
	EXPECT_TRUE(ctx);
}

TEST_F(UTPTest, CreateUTPSocket) {
	utp_context *ctx;
	ctx = utp_init(2);
	EXPECT_TRUE(ctx);

	utp_socket *s;
	s = utp_create_socket(ctx);
	EXPECT_TRUE(s);
}

TEST_F(UTPTest, EndToEndWithUDP) {
	UTPNode utp_host;
	utp_host.SetDebugName("host");
	
	// Bind host socket
	utp_host.Listen("127.0.0.1", "5678");

	UTPNode utp_client;		
	utp_client.SetDebugName("client");

	// Send connect message
	utp_client.Connect("127.0.0.1", "5678");

	// Receive connect message and send connected-message to client
	utp_host.Recv();

	// Receive connected-message
	utp_client.Recv();

	// Send some data
	utp_client.SendMessage("Hello world!");

	// Receive the data
	utp_host.Recv();

	EXPECT_EQ("Hello world!", utp_host.last_message_received());
}