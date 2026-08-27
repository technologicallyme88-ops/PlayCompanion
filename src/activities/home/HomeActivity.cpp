#include "HomeActivity.h"

#include <Bitmap.h>
#include <Epub.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Utf8.h>
#include <Xtc.h>
#include <esp_random.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "../../apps_local/Shelf.h"  // CrossPlay game/app shelf

#include "CrossPointSettings.h"
#include "CrossPointState.h"
#include "MappedInputManager.h"
#include "OpdsServerStore.h"
#include "RecentBooksStore.h"
#include "companion/CompanionRenderer.h"
#include "companion/CompanionState.h"
#include "companion/CompanionTracker.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
// One-shot: set by the boot path, consumed by the first home paint.
bool panelHoldsRetainedFrame = false;

// Companion dialogue must never use GfxRenderer::wrappedText() with a hard
// line limit: that helper intentionally ellipsises the final line.  Companion
// quotes are short prose, so wrap every word and let the caller size the
// speech bubble (or choose a smaller font) around the complete message.
std::vector<std::string> wrapCompanionText(const GfxRenderer& renderer, const int fontId, const char* text,
                                           const int maxWidth) {
  std::vector<std::string> lines;
  if (!text || !*text || maxWidth <= 0) return lines;

  std::string remaining(text);
  std::string current;
  while (!remaining.empty()) {
    const size_t space = remaining.find(' ');
    const std::string word = space == std::string::npos ? remaining : remaining.substr(0, space);
    if (space == std::string::npos)
      remaining.clear();
    else
      remaining.erase(0, space + 1);

    const std::string candidate = current.empty() ? word : current + " " + word;
    if (renderer.getTextWidth(fontId, candidate.c_str()) <= maxWidth) {
      current = candidate;
      continue;
    }

    if (!current.empty()) {
      lines.push_back(current);
      current = word;
    } else {
      // Companion source strings do not currently contain words this wide, but
      // keeping the whole word is still preferable to silently replacing part
      // of the message with an ellipsis.
      current = word;
    }
  }
  if (!current.empty()) lines.push_back(current);
  return lines;
}

struct CompanionPageSlice {
  int page = 0;
  int pages = 1;
  int start = 0;
  int count = 0;
};

CompanionPageSlice companionPageSlice(const std::vector<std::string>& lines, const int maxLines,
                                      const int requestedPage) {
  CompanionPageSlice result;
  if (lines.empty() || maxLines <= 0) return result;
  result.pages = std::max(1, (static_cast<int>(lines.size()) + maxLines - 1) / maxLines);
  result.page = std::clamp(requestedPage, 0, result.pages - 1);
  result.start = result.page * maxLines;
  result.count = std::min(maxLines, static_cast<int>(lines.size()) - result.start);
  return result;
}

void drawCompanionPager(const GfxRenderer& renderer, const Rect& bubble, const int page, const int pages,
                        const int pad) {
  if (pages <= 1) return;
  char indicator[16];
  snprintf(indicator, sizeof(indicator), "< %d / %d >", page + 1, pages);
  const int w = renderer.getTextWidth(SMALL_FONT_ID, indicator, EpdFontFamily::BOLD);
  const int h = renderer.getLineHeight(SMALL_FONT_ID);
  renderer.drawText(SMALL_FONT_ID, bubble.x + bubble.width - pad - w, bubble.y + bubble.height - pad - h, indicator,
                    true, EpdFontFamily::BOLD);
}
}  // namespace

void HomeActivity::notePanelHoldsRetainedFrame() { panelHoldsRetainedFrame = true; }

int HomeActivity::upstreamMenuRows() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const bool continueRow = metrics.homeContinueReadingInMenu && !recentBooks.empty();
  return 4 + (hasOpdsServers ? 1 : 0) + (continueRow ? 1 : 0);
}

int HomeActivity::getMenuItemCount() const {
  int count = 4 + shelf::folderCount();  // stock rows + CrossPlay Games/Apps folders
  if (!recentBooks.empty()) {
    count += recentBooks.size();
  }
  if (hasOpdsServers) {
    count++;
  }
  return count;
}

void HomeActivity::loadRecentBooks(int maxBooks) {
  recentBooks.clear();
  const auto& books = RECENT_BOOKS.getBooks();
  recentBooks.reserve(std::min(static_cast<int>(books.size()), maxBooks));

  for (const RecentBook& book : books) {
    // Limit to maximum number of recent books
    if (recentBooks.size() >= maxBooks) {
      break;
    }

    // Skip if file no longer exists
    if (RecentBooksStore::isMissing(book)) {
      continue;
    }

    recentBooks.push_back(book);
  }
}

void HomeActivity::loadRecentCovers(int coverHeight) {
  recentsLoading = true;
  bool showingLoading = false;
  Rect popupRect;

  int progress = 0;
  for (RecentBook& book : recentBooks) {
    if (!book.coverBmpPath.empty()) {
      std::string coverPath = UITheme::getCoverThumbPath(book.coverBmpPath, coverHeight);
      if (!Storage.exists(coverPath.c_str())) {
        // If epub, try to load the metadata for title/author and cover
        if (FsHelpers::hasEpubExtension(book.path)) {
          Epub epub(book.path, "/.crosspoint");
          // Skip loading css since we only need metadata here
          epub.load(false, true);

          // Try to generate thumbnail image for Continue Reading card
          if (!showingLoading) {
            showingLoading = true;
            popupRect = GUI.drawPopup(renderer, tr(STR_LOADING_POPUP));
          }
          GUI.fillPopupProgress(renderer, popupRect, 10 + progress * (90 / recentBooks.size()));
          bool success = epub.generateThumbBmp(coverHeight);
          if (!success) {
            RECENT_BOOKS.updateBook(book.path, book.title, book.author, "");
            book.coverBmpPath = "";
          }
          coverRendered = false;
          requestUpdate();
        } else if (FsHelpers::hasXtcExtension(book.path)) {
          // Handle XTC file
          Xtc xtc(book.path, "/.crosspoint");
          if (xtc.load()) {
            // Try to generate thumbnail image for Continue Reading card
            if (!showingLoading) {
              showingLoading = true;
              popupRect = GUI.drawPopup(renderer, tr(STR_LOADING_POPUP));
            }
            GUI.fillPopupProgress(renderer, popupRect, 10 + progress * (90 / recentBooks.size()));
            bool success = xtc.generateThumbBmp(coverHeight);
            if (!success) {
              RECENT_BOOKS.updateBook(book.path, book.title, book.author, "");
              book.coverBmpPath = "";
            }
            coverRendered = false;
            requestUpdate();
          }
        }
      }
    }
    progress++;
  }

  recentsLoaded = true;
  recentsLoading = false;
}

void HomeActivity::onEnter() {
  Activity::onEnter();

  hasOpdsServers = OPDS_STORE.hasServers();
  // Resolve the calendar day once here rather than per render: the companion's
  // mood has to reflect days elapsed since the last reading session, and this
  // is the only screen that shows it outside one.
  COMPANION.refreshForDisplay();
  companionMessagePage = 0;
  companionMessagePages = 1;
  // Pick the line once per visit, not per render, so it stays put while the
  // menu cursor moves but is fresh every time you come back. Drawn at random
  // rather than in rotation, and never the same one twice running -- a repeat
  // reads as a bug even when it is chance. Not persisted: an SD write is not
  // worth it for flavour text, and a reshuffle after a reboot is harmless.
  static uint32_t lastQuote = UINT32_MAX;
  const uint8_t quoteCount = companion::quoteCountFor(CompanionTracker::activeId(), COMPANION.currentMood());
  if (quoteCount > 1) {
    uint32_t pick = lastQuote;
    while (pick == lastQuote) pick = esp_random() % quoteCount;
    lastQuote = pick;
    companionQuoteIndex = pick;
  } else {
    companionQuoteIndex = 0;
  }

  const auto& metrics = UITheme::getInstance().getMetrics();
  loadRecentBooks(metrics.homeRecentBooksCount);

  const auto base = static_cast<int>(recentBooks.size());
  selectorIndex = initialMenuItem == HomeMenuItem::NONE ? 0 : base + menuItemToIndex(initialMenuItem, hasOpdsServers);

  // Restore the CrossPlay shelf folder selection when returning Home.
  if (const int shelfRow = shelf::lastFolderOnHome(); shelfRow >= 0) {
    selectorIndex = base + upstreamMenuRows() + shelfRow;
  }

  // Simulator/site helper; a no-op on normal hardware unless the environment is set.
  shelf::autostartFromEnv(renderer, mappedInput);

  // Trigger first update
  requestUpdate();
}

void HomeActivity::onExit() {
  Activity::onExit();

  // Consume the milestone once the user has actually been on the screen that
  // shows it. Clearing at render time instead would lose it to the very first
  // repaint, before it had been read.
  if (SETTINGS.companionEnabled && COMPANION_STATE.milestonePending) {
    COMPANION_STATE.milestonePending = false;
    COMPANION_STATE.saveToFile();
  }

  // Free the stored cover buffer if any
  freeCoverBuffer();
}

bool HomeActivity::storeCoverBuffer() {
  // render() must have already set the cover rect; without it we'd be back to
  // cloning the whole framebuffer.
  if (coverRectW <= 0 || coverRectH <= 0) return false;
  freeCoverBuffer();
  const size_t needed = renderer.getRegionByteSize(coverRectX, coverRectY, coverRectW, coverRectH);
  if (needed == 0) return false;
  coverBuffer = static_cast<uint8_t*>(malloc(needed));
  if (!coverBuffer) {
    LOG_ERR("HOME", "OOM: cover buffer (%u bytes)", (unsigned)needed);
    return false;
  }
  coverBufferSize = needed;
  if (!renderer.copyRegionToBuffer(coverRectX, coverRectY, coverRectW, coverRectH, coverBuffer, coverBufferSize)) {
    free(coverBuffer);
    coverBuffer = nullptr;
    coverBufferSize = 0;
    return false;
  }
  return true;
}

void HomeActivity::drawCompanion(const Rect region) const {
  if (!SETTINGS.companionEnabled || !SETTINGS.companionOnHome) return;
  if (region.width <= 0 || region.height <= 0) return;

  const int stripTop = region.y;
  const int stripBottom = region.y + region.height;
  const int pageWidth = region.x + region.width;
  const int leftEdge = region.x;

  constexpr int SIDE_MARGIN = 10;
  constexpr int GAP = 4;          // between the tail tip and the character
  constexpr int BUBBLE_PAD = 10;  // inside the bubble, clear of the rounded corners
  constexpr int TAIL_LENGTH = 12;
  constexpr int MIN_BUBBLE_W = 90;
  constexpr int LABEL_GAP = 2;            // between the character's feet and its status label
  constexpr int SUBLABEL_GAP = 0;         // between the mood label and the streak/progress line
  constexpr int DESCENDER_ALLOWANCE = 3;  // getTextHeight() omits descenders
  constexpr int BOTTOM_MARGIN = 2;        // keeps the last line clear of the button hints
  // Walk cycle. Advanced by redraws rather than a timer: a timer would keep the
  // panel refreshing, block the low-power idle, and accumulate e-ink ghosting.
  // Driving it from input means the character only moves when the screen was
  // going to be repainted anyway, so the motion is free.
  constexpr int WALK_STEPS = 6;
  // Kept to roughly the tail's reach: the bubble has to sit clear of the
  // character's furthest step, so every pixel of pacing pushes the bubble right.
  constexpr int WALK_TRAVEL = 14;
  constexpr int BOB_HEIGHT = 3;
  constexpr int MAX_SCALE = 4;

  const int available = stripBottom - stripTop - BOTTOM_MARGIN;

  // getTextHeight() reports the ascender only, but drawText() takes y as the
  // top and descenders hang below it. Without this allowance the bottom line
  // overruns into the button hints.
  const int labelH = renderer.getTextHeight(UI_10_FONT_ID) + DESCENDER_ALLOWANCE;
  const int subH = renderer.getTextHeight(SMALL_FONT_ID) + DESCENDER_ALLOWANCE;

  // The mood label, and under it the line answering "why?" and "what next?".
  // A reachable target beats a tally, so progress toward Thriving wins when
  // there is progress to report.
  const auto id = CompanionTracker::activeId();
  const auto mood = COMPANION.currentMood();
  const char* label = companion::moodLabel(mood);
  char sub[40] = "";
  const uint16_t minutes = COMPANION.minutesToday();
  const companion::MoodThresholds thresholds;
  if (minutes >= thresholds.contentMinutes && minutes < thresholds.thrivingMinutes) {
    snprintf(sub, sizeof(sub), tr(STR_COMPANION_TO_THRIVING_FORMAT), thresholds.thrivingMinutes - minutes);
  } else if (COMPANION.hasValidClock() && COMPANION_STATE.ledger.streakDays > 0) {
    snprintf(sub, sizeof(sub), tr(STR_COMPANION_STREAK_FORMAT), COMPANION_STATE.ledger.streakDays);
  }

  // A beaten personal best takes over the bubble once, then reverts to the
  // normal mood lines. The flag is cleared by the caller after the render so a
  // repaint mid-visit does not swallow it before it has been seen.
  const char* quote = COMPANION_STATE.milestonePending ? companion::milestoneQuoteFor(id, companionQuoteIndex)
                                                       : companion::quoteFor(id, mood, companionQuoteIndex);

  // Side by side only works if the widest character this could pick still leaves
  // a bubble wide enough to wrap. A column beside the menu does not, so it
  // stacks the bubble above the character instead. Tested against the largest
  // scale rather than the chosen one because the scale is picked on height, and
  // a tall narrow region would otherwise choose a character that squeezes the
  // bubble out entirely.
  constexpr int SIDE_BY_SIDE_MIN_W =
      SIDE_MARGIN * 2 + WALK_TRAVEL + companion::poseWidth(MAX_SCALE) + GAP + TAIL_LENGTH + MIN_BUBBLE_W;
  if (region.width < SIDE_BY_SIDE_MIN_W) {
    drawCompanionColumn(region, label, sub, quote);
    return;
  }

  // Themes leave very different amounts of room under their menus: Lyra spares
  // ~148px, Lyra Extended 90, Classic 78. Stacking the status under the
  // character only works in the first case. Below the height that fits a
  // scale-3 character over both status lines, the stack starves everything at
  // once -- a 30px character beside a bubble too short to hold a single line --
  // so the layout turns on its side instead: the status moves into a column at
  // the right edge and the character and bubble each take the full strip height.
  const int roomyNeeded = companion::poseHeight(3) + BOB_HEIGHT + LABEL_GAP + labelH + SUBLABEL_GAP + subH;
  if (available < roomyNeeded) {
    drawCompanionCompact(stripTop, available, leftEdge, pageWidth, label, sub, quote);
    return;
  }

  // Whole-pixel scales only: fractional scaling would smear the baked dither.
  const int labelBlock = LABEL_GAP + labelH + SUBLABEL_GAP + subH;
  int scale = MAX_SCALE;
  while (scale > 1 && companion::poseHeight(scale) + BOB_HEIGHT + labelBlock > available) scale--;

  const int spriteW = companion::poseWidth(scale);
  const int spriteH = companion::poseHeight(scale);

  // Character and labels are centred as one block, so the status sits directly
  // under the feet instead of drifting to the bottom of the strip.
  const int blockH = spriteH + BOB_HEIGHT + labelBlock;
  const int blockTop = stripTop + std::max(0, (available - blockH) / 2);
  const int artHeight = spriteH + BOB_HEIGHT;

  // Ping-pong across WALK_TRAVEL, facing the direction of travel.
  const uint32_t phase = companionFrame % (WALK_STEPS * 2);
  const bool walkingBack = phase >= WALK_STEPS;
  const uint32_t step = walkingBack ? (WALK_STEPS * 2 - 1 - phase) : phase;
  const int walkX = static_cast<int>(step) * WALK_TRAVEL / (WALK_STEPS - 1);
  const int bob = (companionFrame % 2) ? BOB_HEIGHT : 0;

  const int spriteX = leftEdge + SIDE_MARGIN + walkX;
  const int spriteY = blockTop + bob;

  // A neglected companion is curled up or powered down: pacing about would
  // undercut the pose, so it stays put and only the quote rotates.
  const bool restless = mood != companion::Mood::Neglected;
  const int drawY = restless ? spriteY : spriteY - bob;
  companion::drawPose(renderer, id, mood, restless ? spriteX : leftEdge + SIDE_MARGIN, drawY, scale,
                      restless && walkingBack);

  // Status lines sit centred under the character's whole pacing range, not
  // under the sprite itself, so they stay put while the character moves.
  const int lane = WALK_TRAVEL + spriteW;
  {
    const int labelW = renderer.getTextWidth(UI_10_FONT_ID, label, EpdFontFamily::BOLD);
    const int subW = sub[0] != '\0' ? renderer.getTextWidth(SMALL_FONT_ID, sub) : 0;

    // Both lines share one centre so they read as a block. That centre starts
    // under the character but is pushed right far enough that the widest line
    // still clears the screen edge -- "27 min to Thriving" is wider than the
    // character it sits under, so centring on the sprite alone runs it off.
    const int widest = std::max(labelW, subW);
    int centreX = leftEdge + SIDE_MARGIN + lane / 2;
    centreX = std::max(centreX, leftEdge + SIDE_MARGIN + widest / 2);
    centreX = std::min(centreX, pageWidth - SIDE_MARGIN - widest / 2);

    const int labelY = blockTop + artHeight + LABEL_GAP;
    renderer.drawText(UI_10_FONT_ID, centreX - labelW / 2, labelY, label, true, EpdFontFamily::BOLD);
    if (subW > 0) {
      renderer.drawText(SMALL_FONT_ID, centreX - subW / 2, labelY + labelH + SUBLABEL_GAP, sub);
    }
  }

  if (!quote) return;

  // Bubble body starts past the character's furthest step plus the tail, so the
  // two can never collide mid-stride.
  const int bubbleX = leftEdge + SIDE_MARGIN + WALK_TRAVEL + spriteW + GAP + TAIL_LENGTH;
  const int bubbleW = pageWidth - bubbleX - SIDE_MARGIN;
  if (bubbleW < MIN_BUBBLE_W) return;  // narrow screen: character only

  // Size the bubble to the complete quote. The generic wrapped-text helper
  // ellipsises its last permitted line, so Companion uses its own unlimited
  // wrapper and only drops to the small font when the normal UI font would be
  // too tall for this strip.
  const int textW = bubbleW - BUBBLE_PAD * 2;
  int quoteFont = UI_10_FONT_ID;
  auto lines = wrapCompanionText(renderer, quoteFont, quote, textW);
  int lineH = renderer.getLineHeight(quoteFont);
  const int maxBubbleH = artHeight - 2;
  int maxLines = std::max(1, (maxBubbleH - BUBBLE_PAD * 2) / lineH);
  if (static_cast<int>(lines.size()) > maxLines) {
    quoteFont = SMALL_FONT_ID;
    lines = wrapCompanionText(renderer, quoteFont, quote, textW);
    lineH = renderer.getLineHeight(quoteFont);
    maxLines = std::max(1, (maxBubbleH - BUBBLE_PAD * 2) / lineH);
  }
  const int pagerH = static_cast<int>(lines.size()) > maxLines ? renderer.getLineHeight(SMALL_FONT_ID) + 2 : 0;
  maxLines = std::max(1, (maxBubbleH - BUBBLE_PAD * 2 - pagerH) / lineH);
  const auto page = companionPageSlice(lines, maxLines, companionMessagePage);
  companionMessagePages = page.pages;
  const int bubbleH = page.count * lineH + BUBBLE_PAD * 2 + (page.pages > 1 ? renderer.getLineHeight(SMALL_FONT_ID) + 2 : 0);
  const int bubbleY = blockTop + (artHeight - bubbleH) / 2;

  companion::drawSpeechBubble(renderer, bubbleX, bubbleY, bubbleW, bubbleH, TAIL_LENGTH);
  const Rect textBounds{bubbleX + BUBBLE_PAD, bubbleY, textW, bubbleH};
  int textY = bubbleY + BUBBLE_PAD;
  for (int i = 0; i < page.count; ++i) {
    UITheme::drawCenteredText(renderer, textBounds, quoteFont, textY, lines[page.start + i].c_str());
    textY += lineH;
  }
  drawCompanionPager(renderer, Rect{bubbleX, bubbleY, bubbleW, bubbleH}, page.page, page.pages, BUBBLE_PAD);
}

void HomeActivity::drawCompanionCompact(const int stripTop, const int available, const int leftEdge,
                                        const int pageWidth, const char* label, const char* sub,
                                        const char* quote) const {
  constexpr int SIDE_MARGIN = 10;
  constexpr int GAP = 4;
  constexpr int TAIL_LENGTH = 12;
  constexpr int PAD = 6;             // bubble inner padding, tighter than the roomy layout's 10
  constexpr int COL_GAP = 8;         // between the bubble and the status column
  constexpr int MIN_BUBBLE_W = 200;  // below this the quote wraps to nonsense, so the column goes instead
  constexpr int STRIP_INSET = 4;     // keeps the bubble off the menu row above and the hints below
  constexpr int SUBLABEL_GAP = 0;
  constexpr int DESCENDER_ALLOWANCE = 3;
  constexpr int WALK_STEPS = 6;
  constexpr int WALK_TRAVEL = 14;
  constexpr int BOB_HEIGHT = 3;
  constexpr int MAX_SCALE = 4;

  // Nothing is stacked here, so the character is free to take the whole strip.
  int scale = 0;
  for (int candidate = MAX_SCALE; candidate >= 1; candidate--) {
    if (companion::poseHeight(candidate) + BOB_HEIGHT <= available) {
      scale = candidate;
      break;
    }
  }
  if (scale == 0) return;  // theme leaves no usable room

  const int spriteW = companion::poseWidth(scale);
  const int spriteH = companion::poseHeight(scale);
  const int spriteTop = stripTop + (available - spriteH - BOB_HEIGHT) / 2;

  const uint32_t phase = companionFrame % (WALK_STEPS * 2);
  const bool walkingBack = phase >= WALK_STEPS;
  const uint32_t step = walkingBack ? (WALK_STEPS * 2 - 1 - phase) : phase;
  const int walkX = static_cast<int>(step) * WALK_TRAVEL / (WALK_STEPS - 1);
  const int bob = (companionFrame % 2) ? BOB_HEIGHT : 0;

  const auto id = CompanionTracker::activeId();
  const auto mood = COMPANION.currentMood();
  const bool restless = mood != companion::Mood::Neglected;
  companion::drawPose(renderer, id, mood, leftEdge + SIDE_MARGIN + (restless ? walkX : 0),
                      restless ? spriteTop + bob : spriteTop, scale, restless && walkingBack);

  if (!quote) return;

  // The bubble never spans the whole strip: sitting flush under the last menu
  // row reads as a collision even when the arithmetic is right.
  const int usableH = available - STRIP_INSET * 2;
  const int bigLineH = renderer.getLineHeight(UI_10_FONT_ID);

  // On the shortest strips a UI_10 bubble holds one line, which truncates the
  // longer quotes, and a full-size mood label towers over a scale-1 character.
  // Both drop to the small font together, so the text stays subordinate to the
  // companion and every quote survives intact.
  const bool tight = usableH < bigLineH * 2 + PAD * 2;
  const int quoteFont = tight ? SMALL_FONT_ID : UI_10_FONT_ID;
  const int labelFont = tight ? SMALL_FONT_ID : UI_10_FONT_ID;

  const int labelH = renderer.getTextHeight(labelFont) + DESCENDER_ALLOWANCE;
  const int subH = renderer.getTextHeight(SMALL_FONT_ID) + DESCENDER_ALLOWANCE;
  const int labelW = renderer.getTextWidth(labelFont, label, EpdFontFamily::BOLD);
  const int subW = sub[0] != '\0' ? renderer.getTextWidth(SMALL_FONT_ID, sub) : 0;

  const int bubbleX = leftEdge + SIDE_MARGIN + WALK_TRAVEL + spriteW + GAP + TAIL_LENGTH;
  int bubbleRight = pageWidth - SIDE_MARGIN;

  // Status column at the right edge, dropped whole rather than clipped: it only
  // earns its width if the bubble still has room to wrap sensibly.
  const int statusRows = (available >= labelH + SUBLABEL_GAP + subH && subW > 0) ? 2 : (available >= labelH ? 1 : 0);
  const int statusW = statusRows >= 2 ? std::max(labelW, subW) : labelW;
  const bool showStatus = statusRows > 0 && bubbleRight - statusW - COL_GAP - bubbleX >= MIN_BUBBLE_W;
  if (showStatus) bubbleRight -= statusW + COL_GAP;

  const int bubbleW = bubbleRight - bubbleX;
  if (bubbleW < MIN_BUBBLE_W) return;  // narrow screen: character only

  // Wrapped here rather than inside drawCenteredWrappedText so the bubble can be
  // sized to the lines the quote actually needs. Stretching it to the full strip
  // instead just crowds the menu row above.
  int lineH = renderer.getLineHeight(quoteFont);
  const int textW = bubbleW - PAD * 2;
  std::vector<std::string> lines = wrapCompanionText(renderer, quoteFont, quote, textW);

  // If the normal font needs more vertical room than the strip provides, retry
  // in the small font. Unlike wrappedText(maxLines), neither path truncates.
  int effectiveQuoteFont = quoteFont;
  if (static_cast<int>(lines.size()) * lineH + PAD * 2 > usableH && quoteFont != SMALL_FONT_ID) {
    effectiveQuoteFont = SMALL_FONT_ID;
    lineH = renderer.getLineHeight(effectiveQuoteFont);
    lines = wrapCompanionText(renderer, effectiveQuoteFont, quote, textW);
  }
  if (lines.empty()) return;

  int maxLines = std::max(1, (usableH - PAD * 2) / lineH);
  const int pagerH = static_cast<int>(lines.size()) > maxLines ? renderer.getLineHeight(SMALL_FONT_ID) + 1 : 0;
  maxLines = std::max(1, (usableH - PAD * 2 - pagerH) / lineH);
  const auto page = companionPageSlice(lines, maxLines, companionMessagePage);
  companionMessagePages = page.pages;
  const int bubbleH = std::min(usableH, page.count * lineH + PAD * 2 +
                                          (page.pages > 1 ? renderer.getLineHeight(SMALL_FONT_ID) + 1 : 0));
  const int bubbleY = stripTop + (available - bubbleH) / 2;
  companion::drawSpeechBubble(renderer, bubbleX, bubbleY, bubbleW, bubbleH, TAIL_LENGTH);

  const Rect textBounds{bubbleX + PAD, bubbleY, textW, bubbleH};
  int textY = bubbleY + PAD;
  for (int i = 0; i < page.count; ++i) {
    UITheme::drawCenteredText(renderer, textBounds, effectiveQuoteFont, textY, lines[page.start + i].c_str());
    textY += lineH;
  }
  drawCompanionPager(renderer, Rect{bubbleX, bubbleY, bubbleW, bubbleH}, page.page, page.pages, PAD);

  if (!showStatus) return;

  const int statusH = statusRows >= 2 ? labelH + SUBLABEL_GAP + subH : labelH;
  const int statusX = pageWidth - SIDE_MARGIN - statusW;
  const int statusTop = stripTop + (available - statusH) / 2;
  renderer.drawText(labelFont, statusX + (statusW - labelW) / 2, statusTop, label, true, EpdFontFamily::BOLD);
  if (statusRows >= 2) {
    renderer.drawText(SMALL_FONT_ID, statusX + (statusW - subW) / 2, statusTop + labelH + SUBLABEL_GAP, sub);
  }
}

bool HomeActivity::restoreCoverBuffer() {
  if (!coverBuffer || coverRectW <= 0 || coverRectH <= 0) return false;
  return renderer.copyBufferToRegion(coverRectX, coverRectY, coverRectW, coverRectH, coverBuffer, coverBufferSize);
}

void HomeActivity::freeCoverBuffer() {
  if (coverBuffer) {
    free(coverBuffer);
    coverBuffer = nullptr;
  }
  coverBufferSize = 0;
  coverBufferStored = false;
}

void HomeActivity::loop() {
  const int menuCount = getMenuItemCount();
  const auto& metrics = UITheme::getInstance().getMetrics();

  auto activateSelection = [this] {
    if (selectorIndex < recentBooks.size()) {
      onSelectBook(recentBooks[selectorIndex].path);
      return;
    }
    const int menuIndex = selectorIndex - static_cast<int>(recentBooks.size());
    switch (indexToMenuItem(menuIndex, hasOpdsServers)) {
      case HomeMenuItem::FILE_BROWSER:
        onFileBrowserOpen();
        break;
      case HomeMenuItem::RECENTS:
        onRecentsOpen();
        break;
      case HomeMenuItem::OPDS_BROWSER:
        onOpdsBrowserOpen();
        break;
      case HomeMenuItem::FILE_TRANSFER:
        onFileTransferOpen();
        break;
      case HomeMenuItem::SETTINGS_MENU:
        onSettingsOpen();
        break;
      default: {
        const int shelfRow = menuIndex - upstreamMenuRows();
        if (shelfRow >= 0 && shelfRow < shelf::folderCount()) {
          shelf::openFolder(shelfRow, renderer, mappedInput);
        }
        break;
      }
    }
  };

#if !FREEINK_CAP_TOUCH
  // Original X4 has no touch panel. When a Companion response spans multiple
  // pages, reserve the front left/right buttons for message paging; the side
  // up/down buttons continue to navigate the Home menu through ButtonNavigator.
  if (companionMessagePages > 1) {
    if (mappedInput.wasReleased(MappedInputManager::Button::ScreenRight)) {
      companionMessagePage = (companionMessagePage + 1) % companionMessagePages;
      requestUpdate();
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::ScreenLeft)) {
      companionMessagePage = (companionMessagePage + companionMessagePages - 1) % companionMessagePages;
      requestUpdate();
      return;
    }
  }
#endif

  buttonNavigator.onNext([this, menuCount] {
    selectorIndex = ButtonNavigator::nextIndex(selectorIndex, menuCount);
    requestUpdate();
  });

  buttonNavigator.onPrevious([this, menuCount] {
    selectorIndex = ButtonNavigator::previousIndex(selectorIndex, menuCount);
    requestUpdate();
  });

  const auto swipe = mappedInput.wasSwipe();
  if (swipe == MappedInputManager::SwipeDir::Left && companionMessagePages > 1) {
    companionMessagePage = (companionMessagePage + 1) % companionMessagePages;
    requestUpdate();
    return;
  }
  if (swipe == MappedInputManager::SwipeDir::Right && companionMessagePages > 1) {
    companionMessagePage = (companionMessagePage + companionMessagePages - 1) % companionMessagePages;
    requestUpdate();
    return;
  }
  if (swipe == MappedInputManager::SwipeDir::Up) {
    selectorIndex = ButtonNavigator::nextIndex(selectorIndex, menuCount);
    requestUpdate();
    return;
  }
  if (swipe == MappedInputManager::SwipeDir::Down) {
    selectorIndex = ButtonNavigator::previousIndex(selectorIndex, menuCount);
    requestUpdate();
    return;
  }

  // Back is otherwise unused on the home menu: open the most recently read
  // book directly (recentBooks is most-recent-first and already pruned of
  // files missing from the SD card).
  if (mappedInput.wasReleased(MappedInputManager::Button::Back) && !recentBooks.empty()) {
    onSelectBook(recentBooks[0].path);
    return;
  }

  const int coverColumnCount = std::max(1, metrics.homeRecentBooksCount);
  const int recentCount = std::min(static_cast<int>(recentBooks.size()), coverColumnCount);
  const int coverColumnWidth = (renderer.getScreenWidth() - 2 * metrics.contentSidePadding) / coverColumnCount;
  int touchedBook = -1;
  const auto coverTouch = mappedInput.colTouch(touchedBook, metrics.contentSidePadding, coverColumnWidth, recentCount,
                                               metrics.homeTopPadding,
                                               metrics.homeTopPadding + metrics.homeCoverTileHeight, coverColumnWidth);
  if (coverTouch != MappedInputManager::RowTouch::None) {
    if (coverTouch == MappedInputManager::RowTouch::Down) {
      if (selectorIndex != touchedBook) {
        selectorIndex = touchedBook;
        requestUpdate();
      }
    } else {
      selectorIndex = touchedBook;
      activateSelection();
    }
    return;
  }

  const int menuTop = metrics.homeTopPadding + metrics.homeCoverTileHeight + metrics.homeMenuTopOffset;
  const int renderedMenuSelection =
      metrics.homeContinueReadingInMenu ? selectorIndex : selectorIndex - recentBooks.size();
  const int renderedMenuCount =
      menuCount - (metrics.homeContinueReadingInMenu ? 0 : static_cast<int>(recentBooks.size()));
  int menuRow = -1;
  // Row height from the theme, not the metrics table: RoundedRaff draws
  // font-derived rows and the touch grid must match the visuals exactly.
  const int menuRowHeight = GUI.getMenuRowHeight(renderer);
  // Bounded by the drawn menu width, not the screen: where a theme narrows its
  // rows to make room for the companion, a tap beside them must not select a row.
  const int menuTouchRight = companionMenuWidth > 0 ? companionMenuWidth : INT32_MAX;
  const auto menuTouch = mappedInput.rowTouch(menuRow, menuTop, menuRowHeight + metrics.menuSpacing, renderedMenuCount,
                                              0, menuTouchRight, menuRowHeight);
  if (menuTouch != MappedInputManager::RowTouch::None) {
    const int touchedIndex =
        metrics.homeContinueReadingInMenu ? menuRow : menuRow + static_cast<int>(recentBooks.size());
    if (menuTouch == MappedInputManager::RowTouch::Down) {
      if (selectorIndex != touchedIndex) {
        selectorIndex = touchedIndex;
        requestUpdate();
      }
    } else {
      selectorIndex = touchedIndex;
      activateSelection();
    }
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    activateSelection();
  }
}

void HomeActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  bool bufferRestored = coverBufferStored && restoreCoverBuffer();

  // Band spans topPadding..homeTopPadding: the cover tile starts at the fixed
  // homeTopPadding, so the height must shrink by topPadding or the band (and a
  // centered title, e.g. RoundedRaff's book title) sinks into the tile.
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.homeTopPadding - metrics.topPadding},
                 metrics.homeContinueReadingInMenu && !recentBooks.empty() ? recentBooks[0].title.c_str() : nullptr);

  // Record the tile rect so storeCoverBuffer (called from the theme) knows
  // which sub-region of the framebuffer to snapshot. ~16 KB in Portrait
  // instead of the 48 KB full framebuffer the previous bind captured.
  coverRectX = 0;
  coverRectY = metrics.homeTopPadding;
  coverRectH = metrics.homeCoverTileHeight;

  // Build menu items dynamically
  std::vector<const char*> menuItems = {tr(STR_BROWSE_FILES), tr(STR_MENU_RECENT_BOOKS), tr(STR_FILE_TRANSFER),
                                        tr(STR_SETTINGS_TITLE)};
  std::vector<UIIcon> menuIcons = {Folder, Recent, Transfer, Settings};

  if (hasOpdsServers) {
    menuItems.insert(menuItems.begin() + 2, tr(STR_OPDS_BROWSER));
    menuIcons.insert(menuIcons.begin() + 2, Library);
  }

  if (metrics.homeContinueReadingInMenu && !recentBooks.empty()) {
    // Insert Continue Reading at the top if enabled in theme
    menuItems.insert(menuItems.begin(), tr(STR_CONTINUE_READING));
    menuIcons.insert(menuIcons.begin(), Book);
  }

  Rect menuRect{0, metrics.homeTopPadding + metrics.homeCoverTileHeight + metrics.homeMenuTopOffset, pageWidth,
                pageHeight - (metrics.headerHeight + metrics.homeTopPadding + metrics.verticalSpacing +
                              metrics.homeMenuTopOffset + metrics.buttonHintsHeight)};
  const Rect coverRect{0, metrics.homeTopPadding, pageWidth, metrics.homeCoverTileHeight};
  const auto labelAt = [&menuItems](int index) { return std::string(menuItems[index]); };

  // The theme decides where the companion goes and what gives up room for it.
  // Deriving it here from the metrics table is only ever right for one theme:
  // each spaces its rows differently, RoundedRaff sizes them from the font, and
  // a theme whose rows are narrower than the screen has more usable space
  // beside its menu than underneath it.
  const auto companion = GUI.getHomeCompanionLayout(renderer, menuRect, coverRect, static_cast<int>(menuItems.size()),
                                                    labelAt, pageHeight - metrics.buttonHintsHeight);
  if (companion.menuWidth > 0) menuRect.width = companion.menuWidth;
  companionMenuWidth = menuRect.width;

  // Narrowing the rect is all it takes to move the cover: it centres its book
  // inside whatever it is handed.
  const Rect drawnCover =
      companion.coverWidth > 0 ? Rect{coverRect.x, coverRect.y, companion.coverWidth, coverRect.height} : coverRect;
  coverRectW = drawnCover.width;
  GUI.drawRecentBookCover(renderer, drawnCover, recentBooks, selectorIndex, coverRendered, coverBufferStored,
                          bufferRestored, std::bind(&HomeActivity::storeCoverBuffer, this));

  // CrossPlay Games/Apps folders are appended after the stock home rows.
  for (int i = 0; i < shelf::folderCount(); ++i) {
    menuItems.push_back(shelf::folders()[i].title);
    menuIcons.push_back(shelf::folders()[i].icon);
  }

  GUI.drawButtonMenu(renderer, menuRect, static_cast<int>(menuItems.size()),
                     metrics.homeContinueReadingInMenu ? selectorIndex : selectorIndex - recentBooks.size(), labelAt,
                     [&menuIcons](int index) { return menuIcons[index]; });

  companionFrame++;
  drawCompanion(companion.region);

  const auto labels = mappedInput.mapLabels(recentBooks.empty() ? "" : tr(STR_RESUME), tr(STR_SELECT), tr(STR_DIR_UP),
                                            tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  if (panelHoldsRetainedFrame) {
    // A sleep wake leaves the sleep screen on the panel and skips the clearing
    // pass so resume stays fast, on the assumption the reader repaints next.
    // Landing on home instead, the fast waveform paints over the retained frame
    // without clearing it and the sleep screen ghosts through. HALF_REFRESH
    // requests the resync that clears it; after this the panel is ours and
    // every later paint can stay fast.
    panelHoldsRetainedFrame = false;
    renderer.displayBuffer(HalDisplay::HALF_REFRESH);
  } else {
    renderer.displayBuffer();
  }

  if (!firstRenderDone) {
    firstRenderDone = true;
    requestUpdate();
  } else if (!recentsLoaded && !recentsLoading) {
    recentsLoading = true;
    loadRecentCovers(metrics.homeCoverHeight);
  }
}

void HomeActivity::onSelectBook(const std::string& path) { activityManager.goToReader(path); }

void HomeActivity::onFileBrowserOpen() { activityManager.goToFileBrowser(); }

void HomeActivity::onRecentsOpen() { activityManager.goToRecentBooks(); }

void HomeActivity::onSettingsOpen() { activityManager.goToSettings(); }

void HomeActivity::onFileTransferOpen() { activityManager.goToFileTransfer(); }

void HomeActivity::onOpdsBrowserOpen() { activityManager.goToBrowser(); }

void HomeActivity::drawCompanionColumn(const Rect region, const char* label, const char* sub, const char* quote) const {
  constexpr int PAD = 6;  // bubble inner padding
  constexpr int TAIL_LENGTH = 12;
  constexpr int BUBBLE_GAP = 2;  // between the tail tip and the character's head
  constexpr int LABEL_GAP = 2;
  constexpr int SUBLABEL_GAP = 0;
  constexpr int DESCENDER_ALLOWANCE = 3;
  constexpr int MARGIN = 4;  // keeps the block off the menu pills and the hints
  constexpr int WALK_STEPS = 6;
  constexpr int WALK_TRAVEL = 14;
  constexpr int BOB_HEIGHT = 3;
  constexpr int MAX_SCALE = 4;
  constexpr int MAX_NOODLE_SCALE = 6;
  constexpr int MIN_BUBBLE_W = 90;

  const auto id = CompanionTracker::activeId();
  const auto mood = COMPANION.currentMood();
  const bool useInkBounds = id == companion::CompanionId::Noodle || id == companion::CompanionId::Lincoln;

  const int colX = region.x + MARGIN;
  const int colW = region.width - MARGIN * 2;
  const int colTop = region.y + MARGIN;
  const int colH = region.height - MARGIN * 2;
  if (colW < MIN_BUBBLE_W) return;

  const int labelH = renderer.getTextHeight(UI_10_FONT_ID) + DESCENDER_ALLOWANCE;
  const int subH = renderer.getTextHeight(SMALL_FONT_ID) + DESCENDER_ALLOWANCE;
  const int textW = colW - PAD * 2;

  // Measure the entire quote before sizing the character. If three or more
  // normal-font lines would crowd the column, use the small UI font rather than
  // allowing the renderer to turn the last line into "...".
  int quoteFont = UI_10_FONT_ID;
  std::vector<std::string> lines = quote ? wrapCompanionText(renderer, quoteFont, quote, textW)
                                         : std::vector<std::string>{};
  int lineH = renderer.getLineHeight(quoteFont);
  if (lines.size() > 3) {
    quoteFont = SMALL_FONT_ID;
    lines = wrapCompanionText(renderer, quoteFont, quote, textW);
    lineH = renderer.getLineHeight(quoteFont);
  }
  int maxPageLines = quoteFont == SMALL_FONT_ID ? 4 : 3;
  const auto page = companionPageSlice(lines, maxPageLines, companionMessagePage);
  companionMessagePages = page.pages;
  const int pagerH = page.pages > 1 ? renderer.getLineHeight(SMALL_FONT_ID) + 1 : 0;
  const int bubbleH = lines.empty() ? 0 : page.count * lineH + PAD * 2 + pagerH;
  const int bubbleBlock = lines.empty() ? 0 : bubbleH + TAIL_LENGTH + BUBBLE_GAP;
  const int statusBlock = LABEL_GAP + labelH + SUBLABEL_GAP + (sub[0] != '\0' ? subH : 0);

  int scale = 0;
  const int maxScale = useInkBounds ? MAX_NOODLE_SCALE : MAX_SCALE;
  for (int candidate = maxScale; candidate >= 1; candidate--) {
    const int candidateW = useInkBounds ? companion::poseInkWidth(id, mood, candidate) : companion::poseWidth(candidate);
    const int candidateH = useInkBounds ? companion::poseInkHeight(id, mood, candidate) : companion::poseHeight(candidate);
    if (candidateW + WALK_TRAVEL > colW) continue;
    if (bubbleBlock + candidateH + BOB_HEIGHT + statusBlock <= colH) {
      scale = candidate;
      break;
    }
  }
  if (scale == 0) return;

  const int spriteW = useInkBounds ? companion::poseInkWidth(id, mood, scale) : companion::poseWidth(scale);
  const int spriteH = useInkBounds ? companion::poseInkHeight(id, mood, scale) : companion::poseHeight(scale);
  const int blockH = bubbleBlock + spriteH + BOB_HEIGHT + statusBlock;
  const int blockTop = colTop + (colH - blockH) / 2;

  // Bubble spans the column; the character paces underneath it, centred on the
  // range it walks rather than on its own width so it does not appear to drift.
  if (!lines.empty()) {
    companion::drawSpeechBubble(renderer, colX, blockTop, colW, bubbleH, TAIL_LENGTH, companion::TailSide::Bottom);
    const Rect textBounds{colX + PAD, blockTop, textW, bubbleH};
    int textY = blockTop + PAD;
    for (int i = 0; i < page.count; ++i) {
      UITheme::drawCenteredText(renderer, textBounds, quoteFont, textY, lines[page.start + i].c_str());
      textY += lineH;
    }
    drawCompanionPager(renderer, Rect{colX, blockTop, colW, bubbleH}, page.page, page.pages, PAD);
  }

  const uint32_t phase = companionFrame % (WALK_STEPS * 2);
  const bool walkingBack = phase >= WALK_STEPS;
  const uint32_t step = walkingBack ? (WALK_STEPS * 2 - 1 - phase) : phase;
  const int walkX = static_cast<int>(step) * WALK_TRAVEL / (WALK_STEPS - 1);
  const int bob = (companionFrame % 2) ? BOB_HEIGHT : 0;

  const bool restless = mood != companion::Mood::Neglected;
  const int laneX = colX + (colW - spriteW - WALK_TRAVEL) / 2;
  const int spriteTop = blockTop + bubbleBlock;
  const int drawX = laneX + (restless ? walkX : WALK_TRAVEL / 2);
  const int drawY = restless ? spriteTop + bob : spriteTop;
  if (noodle) {
    companion::drawPoseTrimmed(renderer, id, mood, drawX, drawY, scale, restless && walkingBack);
  } else {
    companion::drawPose(renderer, id, mood, drawX, drawY, scale, restless && walkingBack);
  }

  const int labelW = renderer.getTextWidth(UI_10_FONT_ID, label, EpdFontFamily::BOLD);
  const int subW = sub[0] != '\0' ? renderer.getTextWidth(SMALL_FONT_ID, sub) : 0;
  const int labelY = spriteTop + spriteH + BOB_HEIGHT + LABEL_GAP;
  const int centreX = colX + colW / 2;
  renderer.drawText(UI_10_FONT_ID, centreX - labelW / 2, labelY, label, true, EpdFontFamily::BOLD);
  if (subW > 0) {
    renderer.drawText(SMALL_FONT_ID, centreX - subW / 2, labelY + labelH + SUBLABEL_GAP, sub);
  }
}
