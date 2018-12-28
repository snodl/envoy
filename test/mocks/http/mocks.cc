#include "mocks.h"

#include "envoy/buffer/buffer.h"
#include "envoy/event/dispatcher.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::MakeMatcher;
using testing::Matcher;
using testing::MatcherInterface;
using testing::MatchResultListener;
using testing::Return;
using testing::ReturnRef;
using testing::SaveArg;

namespace Envoy {
namespace Http {

MockConnectionCallbacks::MockConnectionCallbacks() {}
MockConnectionCallbacks::~MockConnectionCallbacks() {}

MockServerConnectionCallbacks::MockServerConnectionCallbacks() {}
MockServerConnectionCallbacks::~MockServerConnectionCallbacks() {}

MockStreamCallbacks::MockStreamCallbacks() {}
MockStreamCallbacks::~MockStreamCallbacks() {}

MockServerConnection::MockServerConnection() {
  ON_CALL(*this, protocol()).WillByDefault(Return(protocol_));
}

MockServerConnection::~MockServerConnection() {}

MockClientConnection::MockClientConnection() {}
MockClientConnection::~MockClientConnection() {}

MockFilterChainFactory::MockFilterChainFactory() {}
MockFilterChainFactory::~MockFilterChainFactory() {}

template <class T> static void initializeMockStreamFilterCallbacks(T& callbacks) {
  callbacks.cluster_info_.reset(new NiceMock<Upstream::MockClusterInfo>());
  callbacks.route_.reset(new NiceMock<Router::MockRoute>());
  ON_CALL(callbacks, dispatcher()).WillByDefault(ReturnRef(callbacks.dispatcher_));
  ON_CALL(callbacks, streamInfo()).WillByDefault(ReturnRef(callbacks.stream_info_));
  ON_CALL(callbacks, clusterInfo()).WillByDefault(Return(callbacks.cluster_info_));
  ON_CALL(callbacks, route()).WillByDefault(Return(callbacks.route_));
}

MockStreamDecoderFilterCallbacks::MockStreamDecoderFilterCallbacks() {
  initializeMockStreamFilterCallbacks(*this);
  ON_CALL(*this, decodingBuffer()).WillByDefault(Invoke(&buffer_, &Buffer::InstancePtr::get));

  ON_CALL(*this, addDownstreamWatermarkCallbacks(_))
      .WillByDefault(Invoke([this](DownstreamWatermarkCallbacks& callbacks) -> void {
        callbacks_.push_back(&callbacks);
      }));

  ON_CALL(*this, removeDownstreamWatermarkCallbacks(_))
      .WillByDefault(Invoke([this](DownstreamWatermarkCallbacks& callbacks) -> void {
        callbacks_.remove(&callbacks);
      }));

  ON_CALL(*this, activeSpan()).WillByDefault(ReturnRef(active_span_));
  ON_CALL(*this, tracingConfig()).WillByDefault(ReturnRef(tracing_config_));
}

MockStreamDecoderFilterCallbacks::~MockStreamDecoderFilterCallbacks() {}

MockStreamEncoderFilterCallbacks::MockStreamEncoderFilterCallbacks() {
  initializeMockStreamFilterCallbacks(*this);
  ON_CALL(*this, encodingBuffer()).WillByDefault(Invoke(&buffer_, &Buffer::InstancePtr::get));
  ON_CALL(*this, activeSpan()).WillByDefault(ReturnRef(active_span_));
  ON_CALL(*this, tracingConfig()).WillByDefault(ReturnRef(tracing_config_));
}

MockStreamEncoderFilterCallbacks::~MockStreamEncoderFilterCallbacks() {}

MockStreamDecoderFilter::MockStreamDecoderFilter() {
  ON_CALL(*this, setDecoderFilterCallbacks(_))
      .WillByDefault(Invoke(
          [this](StreamDecoderFilterCallbacks& callbacks) -> void { callbacks_ = &callbacks; }));
}

MockStreamDecoderFilter::~MockStreamDecoderFilter() {}

MockStreamEncoderFilter::MockStreamEncoderFilter() {
  ON_CALL(*this, setEncoderFilterCallbacks(_))
      .WillByDefault(Invoke(
          [this](StreamEncoderFilterCallbacks& callbacks) -> void { callbacks_ = &callbacks; }));
}

MockStreamEncoderFilter::~MockStreamEncoderFilter() {}

MockStreamFilter::MockStreamFilter() {
  ON_CALL(*this, setDecoderFilterCallbacks(_))
      .WillByDefault(Invoke([this](StreamDecoderFilterCallbacks& callbacks) -> void {
        decoder_callbacks_ = &callbacks;
      }));
  ON_CALL(*this, setEncoderFilterCallbacks(_))
      .WillByDefault(Invoke([this](StreamEncoderFilterCallbacks& callbacks) -> void {
        encoder_callbacks_ = &callbacks;
      }));
}

MockStreamFilter::~MockStreamFilter() {}

MockAsyncClient::MockAsyncClient() {
  ON_CALL(*this, dispatcher()).WillByDefault(ReturnRef(dispatcher_));
}
MockAsyncClient::~MockAsyncClient() {}

MockAsyncClientCallbacks::MockAsyncClientCallbacks() {}
MockAsyncClientCallbacks::~MockAsyncClientCallbacks() {}

MockAsyncClientStreamCallbacks::MockAsyncClientStreamCallbacks() {}
MockAsyncClientStreamCallbacks::~MockAsyncClientStreamCallbacks() {}

MockAsyncClientRequest::MockAsyncClientRequest(MockAsyncClient* client) : client_(client) {}
MockAsyncClientRequest::~MockAsyncClientRequest() { client_->onRequestDestroy(); }

MockAsyncClientStream::MockAsyncClientStream() {}
MockAsyncClientStream::~MockAsyncClientStream() {}

MockFilterChainFactoryCallbacks::MockFilterChainFactoryCallbacks() {}
MockFilterChainFactoryCallbacks::~MockFilterChainFactoryCallbacks() {}

} // namespace Http

namespace Http {

IsSubsetOfHeadersMatcher IsSubsetOfHeaders(const HeaderMap& expected_headers) {
  return IsSubsetOfHeadersMatcher(expected_headers);
}

IsSupersetOfHeadersMatcher IsSupersetOfHeaders(const HeaderMap& expected_headers) {
  return IsSupersetOfHeadersMatcher(expected_headers);
}

} // namespace Http
} // namespace Envoy
