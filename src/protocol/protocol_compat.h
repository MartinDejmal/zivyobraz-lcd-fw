#pragma once

#include "project_config.h"
#include "protocol_models.h"

namespace zivyobraz::protocol {

class ProtocolCompatService {
 public:
  void begin(const RuntimeConfig& cfg);
  RequestMetadata buildMetadataPayload() const;
  bool shouldFetchImage(const String& previousTimestamp, const String& incomingTimestamp) const;
  ProtocolResponse performSync(bool timestampCheck, const String& apiKey);

 private:
  RuntimeConfig cfg_{};
};

}  // namespace zivyobraz::protocol
