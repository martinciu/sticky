#pragma once
// Non-secret, tunable settings for the weather-clock widget (committed to git;
// WiFi credentials live separately in the gitignored secrets.h).

// --- Location for Open-Meteo. Default: Warsaw. Find yours at latlong.net. ---
#define WIDGET_LAT 52.23
#define WIDGET_LON 21.01

// --- Timezone as a POSIX TZ string. Poland = CET/CEST with EU DST rules. ---
#define WIDGET_TZ "CET-1CEST,M3.5.0,M10.5.0/3"

// --- Timing ---
#define WEATHER_REFRESH_MS (15UL * 60 * 1000) // refresh weather every 15 min
#define NTP_RESYNC_MS (60UL * 60 * 1000)       // re-sync the clock hourly
