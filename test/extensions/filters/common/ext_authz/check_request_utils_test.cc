#include "common/network/address_impl.h"
#include "common/protobuf/protobuf.h"

#include "extensions/filters/common/ext_authz/check_request_utils.h"

#include "test/mocks/network/mocks.h"
#include "test/mocks/ssl/mocks.h"
#include "test/mocks/stream_info/mocks.h"
#include "test/mocks/upstream/mocks.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using testing::Return;
using testing::ReturnPointee;
using testing::ReturnRef;

namespace Envoy {
namespace Extensions {
namespace Filters {
namespace Common {
namespace ExtAuthz {

class CheckRequestUtilsTest : public testing::Test {
public:
  CheckRequestUtilsTest() {
    addr_ = std::make_shared<Network::Address::Ipv4Instance>("1.2.3.4", 1111);
    protocol_ = Envoy::Http::Protocol::Http10;
  };

  Network::Address::InstanceConstSharedPtr addr_;
  absl::optional<Http::Protocol> protocol_;
  CheckRequestUtils check_request_generator_;
  NiceMock<Envoy::Http::MockStreamDecoderFilterCallbacks> callbacks_;
  NiceMock<Envoy::Network::MockReadFilterCallbacks> net_callbacks_;
  NiceMock<Envoy::Network::MockConnection> connection_;
  NiceMock<Envoy::Ssl::MockConnection> ssl_;
  NiceMock<Envoy::StreamInfo::MockStreamInfo> req_info_;
};

// Verify that createTcpCheck's dependencies are invoked when it's called.
TEST_F(CheckRequestUtilsTest, BasicTcp) {
  envoy::service::auth::v2alpha::CheckRequest request;
  EXPECT_CALL(net_callbacks_, connection()).Times(2).WillRepeatedly(ReturnRef(connection_));
  EXPECT_CALL(connection_, remoteAddress()).WillOnce(ReturnRef(addr_));
  EXPECT_CALL(connection_, localAddress()).WillOnce(ReturnRef(addr_));
  EXPECT_CALL(Const(connection_), ssl()).Times(2).WillRepeatedly(Return(&ssl_));

  CheckRequestUtils::createTcpCheck(&net_callbacks_, request);
}

// Verify that createHttpCheck's dependencies are invoked when it's called.
TEST_F(CheckRequestUtilsTest, BasicHttp) {
  Http::HeaderMapImpl headers;
  envoy::service::auth::v2alpha::CheckRequest request;
  EXPECT_CALL(callbacks_, connection()).Times(2).WillRepeatedly(Return(&connection_));
  EXPECT_CALL(connection_, remoteAddress()).WillOnce(ReturnRef(addr_));
  EXPECT_CALL(connection_, localAddress()).WillOnce(ReturnRef(addr_));
  EXPECT_CALL(Const(connection_), ssl()).Times(2).WillRepeatedly(Return(&ssl_));
  EXPECT_CALL(callbacks_, streamId()).WillOnce(Return(0));
  EXPECT_CALL(callbacks_, streamInfo()).Times(3).WillRepeatedly(ReturnRef(req_info_));
  EXPECT_CALL(req_info_, protocol()).Times(2).WillRepeatedly(ReturnPointee(&protocol_));
  Protobuf::Map<ProtobufTypes::String, ProtobufTypes::String> empty;

  CheckRequestUtils::createHttpCheck(&callbacks_, headers, std::move(empty), request);
}

// Verify that createHttpCheck extract the proper attributes from the http request into CheckRequest
// proto object.
TEST_F(CheckRequestUtilsTest, CheckAttrContextPeer) {
  Http::TestHeaderMapImpl request_headers{{"x-envoy-downstream-service-cluster", "foo"},
                                          {":path", "/bar"}};
  envoy::service::auth::v2alpha::CheckRequest request;
  EXPECT_CALL(callbacks_, connection()).WillRepeatedly(Return(&connection_));
  EXPECT_CALL(connection_, remoteAddress()).WillRepeatedly(ReturnRef(addr_));
  EXPECT_CALL(connection_, localAddress()).WillRepeatedly(ReturnRef(addr_));
  EXPECT_CALL(Const(connection_), ssl()).WillRepeatedly(Return(&ssl_));
  EXPECT_CALL(callbacks_, streamId()).WillRepeatedly(Return(0));
  EXPECT_CALL(callbacks_, streamInfo()).WillRepeatedly(ReturnRef(req_info_));
  EXPECT_CALL(req_info_, protocol()).WillRepeatedly(ReturnPointee(&protocol_));
  EXPECT_CALL(ssl_, uriSanPeerCertificate()).WillOnce(Return("source"));
  EXPECT_CALL(ssl_, uriSanLocalCertificate()).WillOnce(Return("destination"));

  Protobuf::Map<ProtobufTypes::String, ProtobufTypes::String> context_extensions;
  context_extensions["key"] = "value";

  CheckRequestUtils::createHttpCheck(&callbacks_, request_headers, std::move(context_extensions),
                                     request);

  EXPECT_EQ("source", request.attributes().source().principal());
  EXPECT_EQ("destination", request.attributes().destination().principal());
  EXPECT_EQ("foo", request.attributes().source().service());
  EXPECT_EQ("value", request.attributes().context_extensions().at("key"));
}

} // namespace ExtAuthz
} // namespace Common
} // namespace Filters
} // namespace Extensions
} // namespace Envoy
