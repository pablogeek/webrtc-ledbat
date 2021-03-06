#include <string>
#include "talk/base/gunit.h"
#include "talk/media/base/fakenetworkinterface.h"
#include "talk/media/ledbat/ledbatdataengine.h"
#include "libutp/utp.h"

class FakeDataReceiver : public sigslot::has_slots<> {
 public:
  FakeDataReceiver() : has_received_data_(false) {}

  void OnDataReceived(const cricket::ReceiveDataParams& params,
                      const char* data, size_t len) {
    has_received_data_ = true;
    last_received_data_ = std::string(data, len);
    last_received_data_len_ = len;
    last_received_data_params_ = params;
  }

  bool LastReceivedEquals(std::string msg) {
  	return (has_received_data_ && last_received_data_ == msg);
  }

  bool has_received_data() const { return has_received_data_; }
  std::string last_received_data() const { return last_received_data_; }
  size_t last_received_data_len() const { return last_received_data_len_; }
  cricket::ReceiveDataParams last_received_data_params() const {
    return last_received_data_params_;
  }

 private:
  bool has_received_data_;
  std::string last_received_data_;
  size_t last_received_data_len_;
  cricket::ReceiveDataParams last_received_data_params_;
};

class SignalReadyToSendObserver : public sigslot::has_slots<> {
 public:
  SignalReadyToSendObserver() : signaled_(false), writable_(false) {}

  void OnSignaled(bool writable) {
    signaled_ = true;
    writable_ = writable;
  }

  bool IsSignaled(bool writable) {
    return signaled_ && (writable_ == writable);
  }

 private:
  bool signaled_;
  bool writable_;
};

class LedbatDataEngineTest : public testing::Test {
  protected:
	virtual void SetUp() {
    engine_.reset(new cricket::LedbatDataEngine());
  }

  virtual void TearDown() {
	}

	cricket::LedbatDataMediaChannel* CreateChannel(
      cricket::FakeNetworkInterface* iface, FakeDataReceiver* receiver) {
    
    cricket::LedbatDataMediaChannel* channel =
      static_cast<cricket::LedbatDataMediaChannel*>(
        engine_.get()->CreateChannel(cricket::DCT_RTP));

    channel->SetInterface(iface);
    channel->SignalDataReceived.connect(
      receiver, &FakeDataReceiver::OnDataReceived);
    return channel;    
	}

	void SetupConnectedChannels(bool chan1_set_send, bool chan2_set_send) {
    net1_.reset(new cricket::FakeNetworkInterface());
    net2_.reset(new cricket::FakeNetworkInterface());
    recv1_.reset(new FakeDataReceiver());
    recv2_.reset(new FakeDataReceiver());
    chan1_.reset(CreateChannel(net1_.get(), recv1_.get()));
    chan1_.get()->SetDebugName("client");
    chan2_.reset(CreateChannel(net2_.get(), recv2_.get()));
    chan2_.get()->SetDebugName("host");
    
    chan1_.get()->SetReceive(true);
    chan2_.get()->SetReceive(true);
    chan1_.get()->SetSend(chan1_set_send);
    chan2_.get()->SetSend(chan2_set_send);

    net1_->SetDestination(chan2_.get());
    net2_->SetDestination(chan1_.get());
	}

	bool SendData(cricket::LedbatDataMediaChannel* chan, const std::string& msg,
    cricket::SendDataResult* result) {
    cricket::SendDataParams params;
    return chan->SendData(params, talk_base::Buffer(
      msg.data(), msg.length()), result);
	}	

	cricket::LedbatDataMediaChannel* chan1() { return chan1_.get(); }
	cricket::LedbatDataMediaChannel* chan2() { return chan2_.get(); }
	FakeDataReceiver* recv1() { return recv1_.get(); }
	FakeDataReceiver* recv2() { return recv2_.get(); }

 private:
	talk_base::scoped_ptr<cricket::LedbatDataEngine> engine_;
	talk_base::scoped_ptr<cricket::FakeNetworkInterface> net1_;
	talk_base::scoped_ptr<cricket::FakeNetworkInterface> net2_;
	talk_base::scoped_ptr<FakeDataReceiver> recv1_;
	talk_base::scoped_ptr<FakeDataReceiver> recv2_;
	talk_base::scoped_ptr<cricket::LedbatDataMediaChannel> chan1_;
	talk_base::scoped_ptr<cricket::LedbatDataMediaChannel> chan2_;
};

TEST_F(LedbatDataEngineTest, CreateMediaChannel) {
  cricket::FakeNetworkInterface iface;
  FakeDataReceiver recv;
  talk_base::scoped_ptr<cricket::LedbatDataMediaChannel> channel(
  	CreateChannel(&iface, &recv));
  EXPECT_TRUE(channel);
}	

TEST_F(LedbatDataEngineTest, SendDataOneWay) {
	SetupConnectedChannels(true, false);

	EXPECT_EQ_WAIT(UTP_STATE_CONNECT, chan1()->utp_state(), 1000);
	cricket::SendDataResult result;
	EXPECT_TRUE(SendData(chan1(), "chan1 says hi", &result));
	EXPECT_EQ(cricket::SDR_SUCCESS, result);
	EXPECT_TRUE_WAIT(recv2()->has_received_data(), 1000);
	EXPECT_EQ("chan1 says hi", recv2()->last_received_data());
}

TEST_F(LedbatDataEngineTest, SendDataMultipleMessages) {
  SetupConnectedChannels(true, false);

  EXPECT_EQ_WAIT(UTP_STATE_CONNECT, chan1()->utp_state(), 1000);
  cricket::SendDataResult result;
  EXPECT_TRUE(SendData(chan1(), "chan1 says hi", &result));
  EXPECT_EQ(cricket::SDR_SUCCESS, result);
  EXPECT_TRUE_WAIT(recv2()->has_received_data(), 1000);
  EXPECT_EQ("chan1 says hi", recv2()->last_received_data());

  
  chan2()->OnReadyToSend(true);

  EXPECT_TRUE(SendData(chan1(), "chan1 says hi again!", &result));
  EXPECT_EQ(cricket::SDR_SUCCESS, result);
  EXPECT_EQ_WAIT("chan1 says hi again!", recv2()->last_received_data(), 1000);
}

TEST_F(LedbatDataEngineTest, SendDataTwoWays) {
	SetupConnectedChannels(true, true);

	EXPECT_EQ_WAIT(UTP_STATE_CONNECT, chan1()->utp_state(), 1000);
	EXPECT_EQ_WAIT(UTP_STATE_CONNECT, chan2()->utp_state(), 1000);
	cricket::SendDataResult result;
	EXPECT_TRUE(SendData(chan1(), "chan1 says hi", &result));
	EXPECT_EQ(cricket::SDR_SUCCESS, result);

	EXPECT_TRUE(SendData(chan2(), "chan2 says hello", &result));
	EXPECT_EQ(cricket::SDR_SUCCESS, result);

	EXPECT_TRUE_WAIT(recv1()->has_received_data(), 1000);
	EXPECT_EQ("chan2 says hello", recv1()->last_received_data());
}