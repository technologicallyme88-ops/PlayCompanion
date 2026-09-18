#include "HeaderDate.h"

#include <GfxRenderer.h>
#include <HalClock.h>

#include <algorithm>
#include <cstddef>
#include <cstdio>

#include "CrossPointSettings.h"
#include "components/themes/BaseTheme.h"
#include "fontIds.h"

namespace {
constexpr int kHeaderDateRightInset = 12;
constexpr int kHeaderDateBottomGap = 10;

// r11.4 does not expose the newer CrossInk date-format settings or
// HalClock::formatDate().  Format a compact local date from the RTC using the
// existing clockUtcOffsetQ setting so the Pokemon compact header remains
// compatible without changing the stable clock/settings APIs.
bool formatHeaderDate(char* buf, const size_t len) {
  if (buf == nullptr || len < 11) return false;
  if (!halClock.isAvailable() || !SETTINGS.clockHasBeenSynced) return false;

  uint16_t year = 0;
  uint8_t month = 0, day = 0, hour = 0, minute = 0;
  if (!halClock.getUtcDateTime(year, month, day, hour, minute)) return false;

  // Apply the configured UTC offset to determine the local calendar day.
  int offsetQ = static_cast<int>(SETTINGS.clockUtcOffsetQ) - 48;
  if (offsetQ < -48) offsetQ = -48;
  if (offsetQ > 56) offsetQ = 56;
  int localMinutes = static_cast<int>(hour) * 60 + static_cast<int>(minute) + offsetQ * 15;
  int dayDelta = 0;
  while (localMinutes < 0) { localMinutes += 1440; --dayDelta; }
  while (localMinutes >= 1440) { localMinutes -= 1440; ++dayDelta; }

  auto isLeap = [](int y) { return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0); };
  auto daysInMonth = [&](int y, int m) {
    static constexpr int kDays[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    return m == 2 ? kDays[m - 1] + (isLeap(y) ? 1 : 0) : kDays[m - 1];
  };

  int y = year, m = month, d = day + dayDelta;
  while (d < 1) {
    if (--m < 1) { m = 12; --y; }
    d += daysInMonth(y, m);
  }
  while (d > daysInMonth(y, m)) {
    d -= daysInMonth(y, m);
    if (++m > 12) { m = 1; ++y; }
  }

  // r11.4 has no user-selectable date format, so use the firmware's compact
  // US-style date instead of importing newer settings fields.
  return snprintf(buf, len, "%02d/%02d/%04d", m, d, y) > 0;
}
}  // namespace

int headerDateReservedWidth(const GfxRenderer& renderer) {
  char dateBuf[13];
  if (!formatHeaderDate(dateBuf, sizeof(dateBuf))) return 0;
  return renderer.getTextWidth(UI_10_FONT_ID, dateBuf) + kHeaderDateRightInset;
}

int headerDateLineBottomY(const GfxRenderer&, const ThemeMetrics& metrics, const int headerHeight) {
  const int effectiveHeaderHeight = headerHeight >= 0 ? headerHeight : metrics.headerHeight;
  return metrics.topPadding + effectiveHeaderHeight - kHeaderDateBottomGap;
}

void drawHeaderDate(const GfxRenderer& renderer, const int pageWidth, const ThemeMetrics& metrics,
                    const int headerHeight) {
  drawHeaderDateAtLineBottom(renderer, pageWidth, headerDateLineBottomY(renderer, metrics, headerHeight));
}

void drawHeaderDateAtLineBottom(const GfxRenderer& renderer, const int pageWidth, const int lineBottomY) {
  constexpr int dateFontId = UI_10_FONT_ID;
  drawHeaderDateAtBaseline(renderer, pageWidth,
                           lineBottomY - renderer.getLineHeight(dateFontId) + renderer.getFontAscenderSize(dateFontId));
}

void drawHeaderDateAtBaseline(const GfxRenderer& renderer, const int pageWidth, const int baselineY) {
  char dateBuf[13];
  if (!formatHeaderDate(dateBuf, sizeof(dateBuf))) return;
  constexpr int dateFontId = UI_10_FONT_ID;
  const int textWidth = renderer.getTextWidth(dateFontId, dateBuf);
  const int dateX = pageWidth - kHeaderDateRightInset - textWidth;
  const int dateY = baselineY - renderer.getFontAscenderSize(dateFontId);
  renderer.drawText(dateFontId, std::max(0, dateX), std::max(0, dateY), dateBuf);
}
