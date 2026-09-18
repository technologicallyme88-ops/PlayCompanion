#pragma once

#include <GfxRenderer.h>

#include "../../MappedInputManager.h"

// Button-only pointer for the Xteink X3/X4 game shelf.
//
// The X3/X4 have no touch panel, but they do have the full six-key C3 input
// layout. Games in this fork were written around touch hit-testing, and some
// of them intentionally register one large hit region then resolve the exact
// cell/card from touch coordinates. Rather than duplicate every game's hit
// testing with a second button-only rules path, this adapter turns the four
// directional keys into a small on-screen pointer and Confirm into the same
// logical tap the touch build already understands.
//
// X4 Pro is untouched: hasTouch() disables both the pointer and its input.
namespace gameinput {

enum class PointerResult : uint8_t { None, Moved, Tap };

struct PointerState {
  int16_t x = -1;
  int16_t y = -1;
};

inline PointerState& pointerState() {
  // Only one Activity is live at a time. Keeping the pointer here avoids
  // adding duplicate cursor members to every game Activity while preserving
  // its last useful position when moving between game screens.
  static PointerState state;
  return state;
}

inline void ensurePointer(const GfxRenderer& renderer) {
  PointerState& state = pointerState();
  const int width = renderer.getScreenWidth();
  const int height = renderer.getScreenHeight();
  if (state.x < 0 || state.x >= width || state.y < 0 || state.y >= height) {
    state.x = static_cast<int16_t>(width / 2);
    state.y = static_cast<int16_t>(height / 2);
  }
}

inline PointerResult readPointer(MappedInputManager& input, const GfxRenderer& renderer, int& tapX, int& tapY) {
  if (input.hasTouch()) return PointerResult::None;

  ensurePointer(renderer);
  PointerState& state = pointerState();
  const int width = renderer.getScreenWidth();
  const int height = renderer.getScreenHeight();

  // 24 px is small enough to land inside the dense Minesweeper/Murdle cells,
  // while still crossing an 800 px screen in a reasonable number of presses.
  constexpr int kStep = 24;
  constexpr int kMargin = 8;

  int dx = 0;
  int dy = 0;
  if (input.wasReleased(MappedInputManager::Button::ScreenLeft)) dx -= kStep;
  if (input.wasReleased(MappedInputManager::Button::ScreenRight)) dx += kStep;
  if (input.wasReleased(MappedInputManager::Button::ScreenUp)) dy -= kStep;
  if (input.wasReleased(MappedInputManager::Button::ScreenDown)) dy += kStep;

  if (dx != 0 || dy != 0) {
    int nx = static_cast<int>(state.x) + dx;
    int ny = static_cast<int>(state.y) + dy;
    if (nx < kMargin) nx = kMargin;
    if (ny < kMargin) ny = kMargin;
    if (nx > width - kMargin - 1) nx = width - kMargin - 1;
    if (ny > height - kMargin - 1) ny = height - kMargin - 1;
    state.x = static_cast<int16_t>(nx);
    state.y = static_cast<int16_t>(ny);
    return PointerResult::Moved;
  }

  if (input.wasReleased(MappedInputManager::Button::Confirm)) {
    tapX = state.x;
    tapY = state.y;
    return PointerResult::Tap;
  }

  return PointerResult::None;
}

inline void drawPointer(GfxRenderer& renderer, const MappedInputManager& input) {
  if (input.hasTouch()) return;
  ensurePointer(renderer);
  const PointerState& state = pointerState();

  // A double outline survives both white and black content underneath without
  // turning the pointer into a large inverted block on e-ink.
  constexpr int kOuter = 18;
  constexpr int kInner = 12;
  renderer.drawRect(state.x - kOuter / 2, state.y - kOuter / 2, kOuter, kOuter, 1, true);
  renderer.drawRect(state.x - kInner / 2, state.y - kInner / 2, kInner, kInner, 1, false);
}

}  // namespace gameinput
