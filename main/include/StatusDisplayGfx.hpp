#pragma once
// Rendering for the optional lock status display.
//
// Everything in here is plain C++ with no ESP-IDF dependency, so the screens
// can be rendered on a host machine for previews and tests. The hardware side
// (SPI panel, backlight, events) lives in StatusDisplay.cpp.
#include <cstddef>
#include <cstdint>

namespace statusgfx {

/// Lock states as defined by LockManager::lockStates.
enum LockState : uint8_t {
  STATE_UNLOCKED = 0,
  STATE_LOCKED = 1,
  STATE_JAMMED = 2,
  STATE_UNKNOWN = 3,
  STATE_UNLOCKING = 4,
  STATE_LOCKING = 5,
};

/// Short full-screen confirmations shown after something was tapped.
enum class Feedback : uint8_t {
  HomeKeyAccepted,
  HomeKeyRejected,
  TagRead,
};

enum class Language : uint8_t { English, Danish };

/// A 16-bit RGB565 canvas over caller-owned memory.
///
/// Pixels are stored byte-swapped (big-endian) when `swapBytes` is set, which
/// is the order SPI panels expect, so the buffer can be sent to the panel as-is.
class Canvas {
public:
  Canvas(uint16_t* pixels, int width, int height, bool swapBytes)
      : m_px(pixels), m_w(width), m_h(height), m_swap(swapBytes) {}

  int width() const { return m_w; }
  int height() const { return m_h; }
  const uint16_t* data() const { return m_px; }
  size_t sizeBytes() const { return size_t(m_w) * size_t(m_h) * 2; }

  static constexpr uint32_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return (uint32_t(r) << 16) | (uint32_t(g) << 8) | b;
  }

  void clear(uint32_t color);
  /// Read a pixel back as RGB888 (for blending and host previews).
  uint32_t get(int x, int y) const;
  void set(int x, int y, uint32_t color);
  void blend(int x, int y, uint32_t color, float alpha);

  // Antialiased filled shapes. Coordinates are in pixels, pixel centres at +0.5.
  void fillCircle(float cx, float cy, float r, uint32_t color);
  void fillRoundRect(float x, float y, float w, float h, float radius, uint32_t color);
  /// Capsule (thick line with round caps) from (x0,y0) to (x1,y1).
  void fillCapsule(float x0, float y0, float x1, float y1, float thickness, uint32_t color);
  /// Upper half of a ring, used for the padlock shackle.
  void fillUpperArc(float cx, float cy, float radius, float thickness, uint32_t color);

  /// Bitmap text, `scale` x the 5x7 font. Accepts UTF-8 Æ, Ø and Å.
  void drawText(int x, int y, const char* utf8, int scale, uint32_t color);
  static int textWidth(const char* utf8, int scale);
  void drawTextCentered(int cy, const char* utf8, int scale, uint32_t color);

private:
  template <typename Sdf>
  void fillSdf(float x0, float y0, float x1, float y1, uint32_t color, Sdf sdf);
  void drawGlyph(int x, int y, const uint8_t* cols, int scale, uint32_t color);

  uint16_t* m_px;
  int m_w;
  int m_h;
  bool m_swap;
};

/// Draws the resting screen for a lock state.
void renderLockState(Canvas& c, uint8_t state, Language lang);
/// Draws a short confirmation after a tap.
void renderFeedback(Canvas& c, Feedback fb, Language lang);

/// Decides which state the screen should show from a LOCK_STATE_CHANGED event.
/// Returns STATE_LOCKING / STATE_UNLOCKING while the lock is moving towards a
/// new target, otherwise the current state.
uint8_t displayStateFor(uint8_t current, uint8_t target);

}  // namespace statusgfx
