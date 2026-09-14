#pragma once
#include <string>

class Settings {
 public:
  void begin();

  std::string wifiSsid() const;
  std::string wifiPass() const;
  std::string pcHost() const;
  uint16_t pcPort() const;

  void setWifi(const std::string& ssid, const std::string& pass);
  void setPcHost(const std::string& host);

 private:
  std::string ssid_;
  std::string pass_;
  std::string host_;
  uint16_t port_ = 8765;
};

Settings& settings();