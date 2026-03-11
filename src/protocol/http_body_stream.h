#pragma once

#include <Client.h>

#include "image/image_decoder.h"

namespace zivyobraz::protocol {

class HttpBodyStream final : public image::IByteStream {
 public:
  explicit HttpBodyStream(Client& client);
  int readByte() override;
  size_t bytesRead() const;

 private:
  Client& client_;
  size_t bytesRead_{0};
};

}  // namespace zivyobraz::protocol
