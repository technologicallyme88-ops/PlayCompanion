#pragma once

namespace farm {

// Fetches IP-based current conditions when Wi-Fi is already connected. The
// persisted four-hour gate lives in FarmState, so repeated callers are cheap.
bool refreshWeatherIfDue();

}  // namespace farm
