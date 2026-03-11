#include "http_body_stream.h"

namespace zivyobraz::protocol {

HttpBodyStream::HttpBodyStream(Client& client) : client_(client) {}

int HttpBodyStream::readByte() {
  while (!client_.available() && client_.connected()) {
    delay(2);
  }

  const int value = client_.read();
  if (value >= 0) {
    bytesRead_++;
  }
  return value;
}

size_t HttpBodyStream::bytesRead() const {
  return bytesRead_;
}

}  // namespace zivyobraz::protocol
