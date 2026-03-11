#pragma once

#include "project_config.h"
#include "protocol_models.h"

namespace zivyobraz::protocol {

class ProtocolCompatService {
 public:
  void begin(const RuntimeConfig& cfg);
  RequestMetadata buildMetadataPayload() const;
  bool shouldFetchImage(const String& previousTimestamp, const String& incomingTimestamp) const;
  ProtocolResponse performSync(bool timestampCheck, const String& apiKey,
                               const String& previousTimestamp);

 private:
  RuntimeConfig cfg_{};
  String buildMetadataJson(const RequestMetadata& md) const;
  bool parseBoolHeaderValue(String value) const;
};

}  // namespace zivyobraz::protocol
