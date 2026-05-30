#pragma once
//
// Template for WiFi credentials. COPY this to secrets.h (which is gitignored,
// so your real password never gets committed):
//
//     cp include/secrets.example.h include/secrets.h
//
// then edit include/secrets.h with your real network.
// NOTE: the StickS3 (ESP32-S3) is 2.4 GHz only — use a 2.4 GHz SSID.

#define WIFI_SSID "your-network-name"
#define WIFI_PASSWORD "your-password"
