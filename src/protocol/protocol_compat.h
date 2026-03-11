#pragma once

#include "project_config.h"
#include "protocol_models.h"

#include <functional>

#include "image/image_decoder.h"

namespace zivyobraz::protocol {

class ProtocolCompatService {
 public:
  using BodyHandler = std::function<bool(image::IByteStream& stream, ProtocolResponse& rsp)>;

  void begin(const RuntimeConfig& cfg);
  RequestMetadata buildMetadataPayload() const;
  bool shouldFetchImage(const String& previousTimestamp, const String& incomingTimestamp) const;
  ProtocolResponse performSync(bool timestampCheck, const String& apiKey,
                               const String& previousTimestamp,
                               const BodyHandler& bodyHandler = nullptr);

 private:
  RuntimeConfig cfg_{};
  String buildMetadataJson(const RequestMetadata& md) const;
  bool parseBoolHeaderValue(String value) const;
  bool wireDebugEnabled() const;
  BodyProbeKind probeBodySignature(const uint8_t* data, size_t len, int32_t& offset) const;
  String bodyProbeKindText(BodyProbeKind kind) const;
};

}  // namespace zivyobraz::protocol
