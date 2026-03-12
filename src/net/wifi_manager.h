#pragma once

#include <Arduino.h>
#include <DNSServer.h>

#include "project_config.h"

namespace zivyobraz::net {

enum class WifiState : uint8_t {
  Unconfigured,
  Connecting,
  Connected,
  ApMode,
  PortalActive,
  ConnectFailed,
};

class WifiManager {
 public:
  void begin(const WifiConfig& cfg, const String& deviceName);
  void tick();
  bool connect(uint32_t timeoutMs = 15000);
  bool reconnect(uint32_t timeoutMs = 8000);
  void disconnect();
  bool applyConfigAndConnect(const WifiConfig& cfg, uint32_t timeoutMs = 15000);

  WifiState state() const;
  String statusText() const;
  String ssid() const;
  String ip() const;
  String mac() const;
  int32_t rssi() const;
  uint32_t apRetries() const;
  bool isConnected() const;
  bool isPortalActive() const;
  String apSsid() const;
  String apPassword() const;
  String portalIp() const;

 private:
  bool startApPortal();
  void stopApPortal();
  String buildApSsid() const;

  WifiConfig cfg_{};
  WifiState state_{WifiState::Unconfigured};
  String deviceName_;
  String apSsid_;
  String apPassword_;
  IPAddress apIp_{192, 168, 4, 1};
  IPAddress apGateway_{192, 168, 4, 1};
  IPAddress apSubnet_{255, 255, 255, 0};
  DNSServer dnsServer_;
  bool portalActive_{false};
  uint32_t apRetries_{0};
  uint32_t lastReconnectAttemptMs_{0};
};

}  // namespace zivyobraz::net
