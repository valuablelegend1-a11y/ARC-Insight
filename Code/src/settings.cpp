#include "settings.h"

#include <Preferences.h>

#include "arcinsight_config.h"

Settings& settings() {
  static Settings s;
  return s;
}

void Settings::begin() {
  Preferences prefs;
  prefs.begin("arcv", true);
  ssid_ = prefs.getString("ssid", ARCI_WIFI_SSID).c_str();
  pass_ = prefs.getString("pass", ARCI_WIFI_PASS).c_str();
  host_ = prefs.getString("host", ARCI_PC_HOST).c_str();
  uint32_t port = prefs.getUInt("port", ARCI_PC_PORT);
  port_ = (uint16_t)port;
  prefs.end();
  if (ssid_.empty()) ssid_ = ARCI_WIFI_SSID;
  if (host_.empty()) host_ = ARCI_PC_HOST;
}

std::string Settings::wifiSsid() const { return ssid_; }
std::string Settings::wifiPass() const { return pass_; }
std::string Settings::pcHost() const { return host_; }
uint16_t Settings::pcPort() const { return port_; }

void Settings::setWifi(const std::string& ssid, const std::string& pass) {
  Preferences prefs;
  prefs.begin("arcv", false);
  prefs.putString("ssid", ssid.c_str());
  prefs.putString("pass", pass.c_str());
  prefs.end();
  ssid_ = ssid;
  pass_ = pass;
}

void Settings::setPcHost(const std::string& host) {
  Preferences prefs;
  prefs.begin("arcv", false);
  prefs.putString("host", host.c_str());
  prefs.end();
  host_ = host;
}