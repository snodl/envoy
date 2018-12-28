#pragma once

#include <memory>
#include <string>

#include "envoy/common/pure.h"

namespace Envoy {
namespace Extensions {
namespace NetworkFilters {
namespace DubboProxy {

// Supported serialization type
enum class SerializationType : uint8_t {
  Hessian = 2,
  Json = 6,
};

// Message Type
enum class MessageType : uint8_t {
  Response = 0,
  Request = 1,
};

/**
 * Dubbo protocol response status types.
 * See org.apache.dubbo.remoting.exchange
 */
enum class ResponseStatus : uint8_t {
  Ok = 20,
  ClientTimeout = 30,
  ServerTimeout = 31,
  BadRequest = 40,
  BadResponse = 50,
  ServiceNotFound = 60,
  ServiceError = 70,
  ServerError = 80,
  ClientError = 90,
  ServerThreadpoolExhaustedError = 100,
};

class Message {
public:
  virtual ~Message() {}
  virtual MessageType messageType() const PURE;
  virtual int32_t bodySize() const PURE;
  virtual bool isEvent() const PURE;
  virtual int64_t requestId() const PURE;
  virtual std::string toString() const PURE;
};

class RequestMessage : public virtual Message {
public:
  virtual ~RequestMessage() {}
  virtual SerializationType serializationType() const PURE;
  virtual bool isTwoWay() const PURE;
};

typedef std::unique_ptr<RequestMessage> RequestMessagePtr;

class ResponseMessage : public virtual Message {
public:
  virtual ~ResponseMessage() {}
  virtual ResponseStatus responseStatus() const PURE;
};

typedef std::unique_ptr<ResponseMessage> ResponseMessagePtr;

} // namespace DubboProxy
} // namespace NetworkFilters
} // namespace Extensions
} // namespace Envoy
