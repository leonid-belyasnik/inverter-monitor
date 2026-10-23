#pragma once

#include <cstdint>

// Template for firmware/esp32-relay/include/secrets.h.
//
// Copy this file to "secrets.h" in the same directory and fill in real
// values for your network. secrets.h is listed in .gitignore and must never
// be committed to the repository.
//
//   cp include/secrets.example.h include/secrets.h
//
namespace secrets {

constexpr const char *WIFI_SSID = "YOUR_WIFI_SSID";
constexpr const char *WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// HomePowerGuard MQTT broker. Port 1883 matches the default
// docker/mosquitto/config/mosquitto.conf (plain TCP, anonymous allowed).
constexpr const char *MQTT_HOST = "192.168.1.10";
constexpr uint16_t MQTT_PORT = 1883;

// Optional MQTT authentication. Leave both empty ("") to connect
// anonymously, which is what the bundled Mosquitto config expects today.
constexpr const char *MQTT_USERNAME = "";
constexpr const char *MQTT_PASSWORD = "";

// Published to v1/devices/<DEVICE_ID>/telemetry. Must be unique per
// physical device if multiple inverters report to the same backend.
constexpr const char *DEVICE_ID = "deye-esp32-001";

} // namespace secrets
