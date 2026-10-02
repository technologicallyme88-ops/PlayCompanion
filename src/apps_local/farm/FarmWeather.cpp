#include "FarmWeather.h"

#include <ArduinoJson.h>
#include <CompanionMood.h>
#include <HalClock.h>
#include <Logging.h>
#include <WiFi.h>

#include <cstdio>
#include <string>

#include "FarmState.h"
#include "network/HttpDownloader.h"

namespace farm {
namespace {
constexpr char LOCATION_URL[] = "https://ipwho.is/?fields=success,latitude,longitude";

int32_t utcMinuteNow() {
  uint16_t year = 0;
  uint8_t month = 0;
  uint8_t day = 0;
  uint8_t hour = 0;
  uint8_t minute = 0;
  if (!halClock.getUtcDateTime(year, month, day, hour, minute)) return 0;
  return companion::localDayNumber(year, month, day, hour, minute, 0) * 1440 + hour * 60 + minute;
}

WeatherEffect effectForCode(const int code) {
  if (code >= 51 && code <= 82) return WeatherEffect::Rain;
  if (code >= 85 && code <= 86) return WeatherEffect::Snow;
  if (code == 95 || code == 96 || code == 99) return WeatherEffect::Rain;
  if (code == 0 || code == 1) return WeatherEffect::Clear;
  return WeatherEffect::Cloudy;
}
}  // namespace

bool refreshWeatherIfDue() {
  if (WiFi.status() != WL_CONNECTED) return false;
  const int32_t utcMinute = utcMinuteNow();
  if (!FARM_STATE.weatherCheckDue(utcMinute)) return false;

  // These two small, infrequent response buffers are released before returning;
  // no allocation is retained by the farm or repeated in a render loop.
  std::string response;
  response.reserve(256);
  if (!HttpDownloader::fetchUrl(LOCATION_URL, response)) {
    LOG_ERR("FARM", "Weather location lookup failed");
    return false;
  }
  JsonDocument location;
  if (deserializeJson(location, response) || !(location["success"] | false)) {
    LOG_ERR("FARM", "Weather location response invalid");
    return false;
  }
  const double latitude = location["latitude"] | 1000.0;
  const double longitude = location["longitude"] | 1000.0;
  if (latitude < -90.0 || latitude > 90.0 || longitude < -180.0 || longitude > 180.0) {
    LOG_ERR("FARM", "Weather location out of range");
    return false;
  }

  char weatherUrl[192];
  snprintf(weatherUrl, sizeof(weatherUrl),
           "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f&current=weather_code",
           latitude, longitude);
  response.clear();
  if (!HttpDownloader::fetchUrl(weatherUrl, response)) {
    LOG_ERR("FARM", "Current weather lookup failed");
    return false;
  }
  JsonDocument weather;
  if (deserializeJson(weather, response) || weather["current"]["weather_code"].isNull()) {
    LOG_ERR("FARM", "Current weather response invalid");
    return false;
  }
  const int code = weather["current"]["weather_code"].as<int>();
  if (!FARM_STATE.applyWeather(effectForCode(code), utcMinute)) return false;
  if (!FARM_STATE.saveToFile()) {
    LOG_ERR("FARM", "Failed to save weather update");
    return false;
  }
  LOG_INF("FARM", "Applied local weather code %d", code);
  return true;
}

}  // namespace farm
