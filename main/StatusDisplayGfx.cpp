#include "StatusDisplayGfx.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace statusgfx {
namespace {

// ---------------------------------------------------------------------------
// 5x7 font, ASCII 0x20-0x5F plus Æ, Ø, Å. One byte per column, bit 0 = top row.
//
// ASCII glyphs are the "classic" Adafruit_GFX font:
// Copyright (c) 2012 Adafruit Industries. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// - Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
// - Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES ARE DISCLAIMED.
// ---------------------------------------------------------------------------
constexpr uint8_t kFontFirst = 0x20;
constexpr uint8_t kFontLast = 0x5F;
constexpr uint8_t kFont[] = {
  0x00, 0x00, 0x00, 0x00, 0x00,  // 0x20 ' '
  0x00, 0x00, 0x5F, 0x00, 0x00,  // 0x21 '!'
  0x00, 0x07, 0x00, 0x07, 0x00,  // 0x22 '"'
  0x14, 0x7F, 0x14, 0x7F, 0x14,  // 0x23 '#'
  0x24, 0x2A, 0x7F, 0x2A, 0x12,  // 0x24 '$'
  0x23, 0x13, 0x08, 0x64, 0x62,  // 0x25 '%'
  0x36, 0x49, 0x56, 0x20, 0x50,  // 0x26 '&'
  0x00, 0x08, 0x07, 0x03, 0x00,  // 0x27 '''
  0x00, 0x1C, 0x22, 0x41, 0x00,  // 0x28 '('
  0x00, 0x41, 0x22, 0x1C, 0x00,  // 0x29 ')'
  0x2A, 0x1C, 0x7F, 0x1C, 0x2A,  // 0x2A '*'
  0x08, 0x08, 0x3E, 0x08, 0x08,  // 0x2B '+'
  0x00, 0x80, 0x70, 0x30, 0x00,  // 0x2C ','
  0x08, 0x08, 0x08, 0x08, 0x08,  // 0x2D '-'
  0x00, 0x00, 0x60, 0x60, 0x00,  // 0x2E '.'
  0x20, 0x10, 0x08, 0x04, 0x02,  // 0x2F '/'
  0x3E, 0x51, 0x49, 0x45, 0x3E,  // 0x30 '0'
  0x00, 0x42, 0x7F, 0x40, 0x00,  // 0x31 '1'
  0x72, 0x49, 0x49, 0x49, 0x46,  // 0x32 '2'
  0x21, 0x41, 0x49, 0x4D, 0x33,  // 0x33 '3'
  0x18, 0x14, 0x12, 0x7F, 0x10,  // 0x34 '4'
  0x27, 0x45, 0x45, 0x45, 0x39,  // 0x35 '5'
  0x3C, 0x4A, 0x49, 0x49, 0x31,  // 0x36 '6'
  0x41, 0x21, 0x11, 0x09, 0x07,  // 0x37 '7'
  0x36, 0x49, 0x49, 0x49, 0x36,  // 0x38 '8'
  0x46, 0x49, 0x49, 0x29, 0x1E,  // 0x39 '9'
  0x00, 0x00, 0x14, 0x00, 0x00,  // 0x3A ':'
  0x00, 0x40, 0x34, 0x00, 0x00,  // 0x3B ';'
  0x00, 0x08, 0x14, 0x22, 0x41,  // 0x3C '<'
  0x14, 0x14, 0x14, 0x14, 0x14,  // 0x3D '='
  0x00, 0x41, 0x22, 0x14, 0x08,  // 0x3E '>'
  0x02, 0x01, 0x59, 0x09, 0x06,  // 0x3F '?'
  0x3E, 0x41, 0x5D, 0x59, 0x4E,  // 0x40 '@'
  0x7C, 0x12, 0x11, 0x12, 0x7C,  // 0x41 'A'
  0x7F, 0x49, 0x49, 0x49, 0x36,  // 0x42 'B'
  0x3E, 0x41, 0x41, 0x41, 0x22,  // 0x43 'C'
  0x7F, 0x41, 0x41, 0x41, 0x3E,  // 0x44 'D'
  0x7F, 0x49, 0x49, 0x49, 0x41,  // 0x45 'E'
  0x7F, 0x09, 0x09, 0x09, 0x01,  // 0x46 'F'
  0x3E, 0x41, 0x41, 0x51, 0x73,  // 0x47 'G'
  0x7F, 0x08, 0x08, 0x08, 0x7F,  // 0x48 'H'
  0x00, 0x41, 0x7F, 0x41, 0x00,  // 0x49 'I'
  0x20, 0x40, 0x41, 0x3F, 0x01,  // 0x4A 'J'
  0x7F, 0x08, 0x14, 0x22, 0x41,  // 0x4B 'K'
  0x7F, 0x40, 0x40, 0x40, 0x40,  // 0x4C 'L'
  0x7F, 0x02, 0x1C, 0x02, 0x7F,  // 0x4D 'M'
  0x7F, 0x04, 0x08, 0x10, 0x7F,  // 0x4E 'N'
  0x3E, 0x41, 0x41, 0x41, 0x3E,  // 0x4F 'O'
  0x7F, 0x09, 0x09, 0x09, 0x06,  // 0x50 'P'
  0x3E, 0x41, 0x51, 0x21, 0x5E,  // 0x51 'Q'
  0x7F, 0x09, 0x19, 0x29, 0x46,  // 0x52 'R'
  0x26, 0x49, 0x49, 0x49, 0x32,  // 0x53 'S'
  0x03, 0x01, 0x7F, 0x01, 0x03,  // 0x54 'T'
  0x3F, 0x40, 0x40, 0x40, 0x3F,  // 0x55 'U'
  0x1F, 0x20, 0x40, 0x20, 0x1F,  // 0x56 'V'
  0x3F, 0x40, 0x38, 0x40, 0x3F,  // 0x57 'W'
  0x63, 0x14, 0x08, 0x14, 0x63,  // 0x58 'X'
  0x03, 0x04, 0x78, 0x04, 0x03,  // 0x59 'Y'
  0x61, 0x59, 0x49, 0x4D, 0x43,  // 0x5A 'Z'
  0x00, 0x7F, 0x41, 0x41, 0x41,  // 0x5B '['
  0x02, 0x04, 0x08, 0x10, 0x20,  // 0x5C backslash
  0x00, 0x41, 0x41, 0x41, 0x7F,  // 0x5D ']'
  0x04, 0x02, 0x01, 0x02, 0x04,  // 0x5E '^'
  0x40, 0x40, 0x40, 0x40, 0x40,  // 0x5F '_'
};
constexpr uint8_t kGlyphAE[5] = {0x7E, 0x09, 0x7F, 0x49, 0x49};
constexpr uint8_t kGlyphOE[5] = {0x3E, 0x61, 0x5D, 0x43, 0x3E};
// Å is a regular A; drawGlyph() adds the ring above the cap height.
constexpr uint8_t kGlyphAA[5] = {0x7C, 0x12, 0x11, 0x12, 0x7C};
constexpr uint8_t kGlyphUnknown[5] = {0x02, 0x01, 0x51, 0x09, 0x06};  // '?'

/// Decodes the next character of a UTF-8 string into a glyph. Lowercase ASCII
/// and æøå are folded to uppercase; anything else becomes '?'.
const uint8_t* nextGlyph(const char*& s) {
  const auto c = static_cast<uint8_t>(*s++);
  if (c < 0x80) {
    uint8_t up = (c >= 'a' && c <= 'z') ? uint8_t(c - 32) : c;
    if (up >= kFontFirst && up <= kFontLast) return &kFont[(up - kFontFirst) * 5];
    return kGlyphUnknown;
  }
  if (c == 0xC3 && *s) {
    const auto c2 = static_cast<uint8_t>(*s++);
    switch (c2) {
      case 0x86: case 0xA6: return kGlyphAE;  // Æ æ
      case 0x98: case 0xB8: return kGlyphOE;  // Ø ø
      case 0x85: case 0xA5: return kGlyphAA;  // Å å
      default: return kGlyphUnknown;
    }
  }
  // Skip the continuation bytes of any other multi-byte sequence.
  while ((static_cast<uint8_t>(*s) & 0xC0) == 0x80) s++;
  return kGlyphUnknown;
}

inline uint16_t to565(uint32_t rgb) {
  return uint16_t(((rgb >> 8) & 0xF800) | ((rgb >> 5) & 0x07E0) | ((rgb >> 3) & 0x001F));
}

inline uint32_t from565(uint16_t c) {
  uint32_t r = (c >> 11) & 0x1F, g = (c >> 5) & 0x3F, b = c & 0x1F;
  return ((r << 3 | r >> 2) << 16) | ((g << 2 | g >> 4) << 8) | (b << 3 | b >> 2);
}

inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

// ---------------------------------------------------------------------------
// Palette
// ---------------------------------------------------------------------------
constexpr uint32_t kBackground = Canvas::rgb(0x0B, 0x0F, 0x14);
constexpr uint32_t kWhite = Canvas::rgb(0xFF, 0xFF, 0xFF);
constexpr uint32_t kGreen = Canvas::rgb(0x2E, 0xCC, 0x71);
constexpr uint32_t kOrange = Canvas::rgb(0xF5, 0xA6, 0x23);
constexpr uint32_t kBlue = Canvas::rgb(0x34, 0x98, 0xDB);
constexpr uint32_t kRed = Canvas::rgb(0xE7, 0x4C, 0x3C);
constexpr uint32_t kGrey = Canvas::rgb(0x8A, 0x94, 0xA0);


constexpr Labels kEnglish{"LOCKED", "UNLOCKED", "LOCKING", "UNLOCKING", "JAMMED",
                          "UNKNOWN", "OK", "DENIED", "TAG", "TAP PHONE"};
constexpr Labels kDanish{"LÅST", "ÅBEN", "LÅSER", "LÅSER OP", "FEJL",
                         "UKENDT", "GODKENDT", "AFVIST", "NFC-TAG", "HOLD HER"};


/// Draws a padlock centred horizontally. `open` raises the shackle and frees
/// its left leg from the body.
void drawPadlock(Canvas& c, bool open, uint32_t color) {
  const float cx = c.width() / 2.0f;
  const float bodyX = cx - 28, bodyY = 50, bodyW = 56, bodyH = 40;
  const float radius = 17, thick = 8;
  const float lift = open ? 10.0f : 0.0f;
  const float arcY = 40 - lift;
  const float legBottom = bodyY + 4;

  c.fillUpperArc(cx, arcY, radius, thick, color);
  // Right leg always reaches into the body.
  c.fillCapsule(cx + radius, arcY, cx + radius, legBottom, thick, color);
  // Left leg: full length when locked, a short stub when open.
  c.fillCapsule(cx - radius, arcY, cx - radius, open ? arcY + 5 : legBottom, thick, color);

  c.fillRoundRect(bodyX, bodyY, bodyW, bodyH, 7, color);
  // Keyhole in the background colour.
  c.fillCircle(cx, bodyY + 15, 5.5f, kBackground);
  c.fillCapsule(cx, bodyY + 17, cx, bodyY + 28, 5, kBackground);
}

}  // namespace

// ---------------------------------------------------------------------------
// Canvas
// ---------------------------------------------------------------------------
void Canvas::clear(uint32_t color) {
  uint16_t v = to565(color);
  if (m_swap) v = uint16_t(v << 8 | v >> 8);
  std::fill(m_px, m_px + size_t(m_w) * m_h, v);
}

uint32_t Canvas::get(int x, int y) const {
  if (x < 0 || y < 0 || x >= m_w || y >= m_h) return 0;
  uint16_t v = m_px[y * m_w + x];
  if (m_swap) v = uint16_t(v << 8 | v >> 8);
  return from565(v);
}

void Canvas::set(int x, int y, uint32_t color) {
  if (x < 0 || y < 0 || x >= m_w || y >= m_h) return;
  uint16_t v = to565(color);
  if (m_swap) v = uint16_t(v << 8 | v >> 8);
  m_px[y * m_w + x] = v;
}

void Canvas::blend(int x, int y, uint32_t color, float alpha) {
  if (alpha <= 0.0f) return;
  if (alpha >= 1.0f) { set(x, y, color); return; }
  const uint32_t bg = get(x, y);
  auto mix = [alpha](uint32_t a, uint32_t b, int shift) {
    float fa = float((a >> shift) & 0xFF), fb = float((b >> shift) & 0xFF);
    return uint32_t(fb + (fa - fb) * alpha + 0.5f) << shift;
  };
  set(x, y, mix(color, bg, 16) | mix(color, bg, 8) | mix(color, bg, 0));
}

template <typename Sdf>
void Canvas::fillSdf(float x0, float y0, float x1, float y1, uint32_t color, Sdf sdf) {
  const int ix0 = std::max(0, int(std::floor(x0)) - 1);
  const int iy0 = std::max(0, int(std::floor(y0)) - 1);
  const int ix1 = std::min(m_w - 1, int(std::ceil(x1)) + 1);
  const int iy1 = std::min(m_h - 1, int(std::ceil(y1)) + 1);
  for (int y = iy0; y <= iy1; y++) {
    for (int x = ix0; x <= ix1; x++) {
      // Signed distance at the pixel centre gives roughly one pixel of
      // antialiasing along every edge.
      const float d = sdf(x + 0.5f, y + 0.5f);
      blend(x, y, color, clampf(0.5f - d, 0.0f, 1.0f));
    }
  }
}

void Canvas::fillCircle(float cx, float cy, float r, uint32_t color) {
  fillSdf(cx - r, cy - r, cx + r, cy + r, color, [=](float px, float py) {
    return std::hypot(px - cx, py - cy) - r;
  });
}

void Canvas::fillRoundRect(float x, float y, float w, float h, float radius, uint32_t color) {
  const float hx = w / 2, hy = h / 2, cx = x + hx, cy = y + hy;
  fillSdf(x, y, x + w, y + h, color, [=](float px, float py) {
    const float qx = std::fabs(px - cx) - (hx - radius);
    const float qy = std::fabs(py - cy) - (hy - radius);
    const float outside = std::hypot(std::max(qx, 0.0f), std::max(qy, 0.0f));
    const float inside = std::min(std::max(qx, qy), 0.0f);
    return outside + inside - radius;
  });
}

void Canvas::fillCapsule(float x0, float y0, float x1, float y1, float thickness, uint32_t color) {
  const float r = thickness / 2;
  const float dx = x1 - x0, dy = y1 - y0;
  const float len2 = dx * dx + dy * dy;
  fillSdf(std::min(x0, x1) - r, std::min(y0, y1) - r, std::max(x0, x1) + r,
          std::max(y0, y1) + r, color, [=](float px, float py) {
            float t = len2 > 0 ? ((px - x0) * dx + (py - y0) * dy) / len2 : 0.0f;
            t = clampf(t, 0.0f, 1.0f);
            return std::hypot(px - (x0 + t * dx), py - (y0 + t * dy)) - r;
          });
}

void Canvas::fillUpperArc(float cx, float cy, float radius, float thickness, uint32_t color) {
  const float outer = radius + thickness / 2;
  fillSdf(cx - outer, cy - outer, cx + outer, cy, color, [=](float px, float py) {
    const float ring = std::fabs(std::hypot(px - cx, py - cy) - radius) - thickness / 2;
    return std::max(ring, py - cy);  // keep only the half above the centre
  });
}

void Canvas::fillArcRight(float cx, float cy, float radius, float thickness, float halfAngleDeg,
                          uint32_t color) {
  const float outer = radius + thickness / 2;
  const float half = halfAngleDeg * 3.14159265f / 180.0f;
  fillSdf(cx, cy - outer, cx + outer, cy + outer, color, [=](float px, float py) {
    const float dx = px - cx, dy = py - cy;
    const float dist = std::hypot(dx, dy);
    const float ring = std::fabs(dist - radius) - thickness / 2;
    // Angular cut expressed in pixels at this radius, so the ends antialias too.
    const float cut = (std::fabs(std::atan2(dy, dx)) - half) * dist;
    return std::max(ring, cut);
  });
}

void Canvas::drawGlyph(int x, int y, const uint8_t* cols, int scale, uint32_t color) {
  if (cols == kGlyphAA) {
    // Small ring centred over the A, in the three rows above the cap height.
    static constexpr int8_t ring[][2] = {{2, -4}, {1, -3}, {3, -3}, {2, -2}};
    for (const auto& p : ring)
      for (int sy = 0; sy < scale; sy++)
        for (int sx = 0; sx < scale; sx++)
          set(x + p[0] * scale + sx, y + p[1] * scale + sy, color);
  }
  for (int col = 0; col < 5; col++) {
    for (int row = 0; row < 7; row++) {
      if (!(cols[col] & (1 << row))) continue;
      for (int sy = 0; sy < scale; sy++)
        for (int sx = 0; sx < scale; sx++) set(x + col * scale + sx, y + row * scale + sy, color);
    }
  }
}

int Canvas::textWidth(const char* utf8, int scale) {
  int n = 0;
  for (const char* s = utf8; *s;) {
    nextGlyph(s);
    n++;
  }
  return n == 0 ? 0 : n * 6 * scale - scale;
}

void Canvas::drawText(int x, int y, const char* utf8, int scale, uint32_t color) {
  for (const char* s = utf8; *s;) {
    drawGlyph(x, y, nextGlyph(s), scale, color);
    x += 6 * scale;
  }
}

void Canvas::drawTextCentered(int cy, const char* utf8, int scale, uint32_t color) {
  int s = scale;
  // Shrink rather than clip when a label is wider than the screen.
  while (s > 1 && textWidth(utf8, s) > m_w - 4) s--;
  drawText((m_w - textWidth(utf8, s)) / 2, cy - (7 * s) / 2, utf8, s, color);
}

// ---------------------------------------------------------------------------
// Screens
// ---------------------------------------------------------------------------
uint8_t displayStateFor(uint8_t current, uint8_t target) {
  if (current == STATE_LOCKING || current == STATE_UNLOCKING) return current;
  const bool settled = current == STATE_LOCKED || current == STATE_UNLOCKED;
  const bool aimed = target == STATE_LOCKED || target == STATE_UNLOCKED;
  if (settled && aimed && target != current) {
    return target == STATE_LOCKED ? STATE_LOCKING : STATE_UNLOCKING;
  }
  return current <= STATE_LOCKING ? current : uint8_t(STATE_UNKNOWN);
}

void rotateSquare(uint16_t* px, int n, uint8_t quarterTurnsCw) {
  const uint8_t turns = quarterTurnsCw & 3;
  if (turns == 0) return;
  if (turns == 2) {
    std::reverse(px, px + size_t(n) * n);
    return;
  }
  auto at = [px, n](int r, int c) -> uint16_t& { return px[r * n + c]; };
  for (int i = 0; i < n / 2; i++) {
    for (int j = i; j < n - 1 - i; j++) {
      uint16_t& a = at(i, j);                  // top
      uint16_t& b = at(j, n - 1 - i);          // right
      uint16_t& c = at(n - 1 - i, n - 1 - j);  // bottom
      uint16_t& d = at(n - 1 - j, i);          // left
      const uint16_t t = a;
      if (turns == 1) {  // clockwise: left -> top -> right -> bottom -> left
        a = d; d = c; c = b; b = t;
      } else {           // counter-clockwise
        a = b; b = c; c = d; d = t;
      }
    }
  }
}

const Labels& defaultLabels(Language lang) { return lang == Language::Danish ? kDanish : kEnglish; }

void renderLockState(Canvas& c, uint8_t state, const Labels& l) {
  c.clear(kBackground);
  uint32_t color;
  bool open;
  const char* label;
  switch (state) {
    case STATE_LOCKED:    color = kGreen;  open = false; label = l.locked;    break;
    case STATE_UNLOCKED:  color = kOrange; open = true;  label = l.unlocked;  break;
    case STATE_LOCKING:   color = kBlue;   open = false; label = l.locking;   break;
    case STATE_UNLOCKING: color = kBlue;   open = true;  label = l.unlocking; break;
    case STATE_JAMMED:    color = kRed;    open = true;  label = l.jammed;    break;
    default:              color = kGrey;   open = false; label = l.unknown;   break;
  }
  drawPadlock(c, open, color);
  c.drawTextCentered(110, label, 2, color);
}

void renderIdle(Canvas& c, const Labels& l) {
  c.clear(kBackground);
  // Phone outline with NFC waves next to it.
  const float px = 30, py = 20, pw = 34, ph = 58;
  c.fillRoundRect(px, py, pw, ph, 7, kGrey);
  c.fillRoundRect(px + 4, py + 6, pw - 8, ph - 14, 3, kBackground);
  c.fillCapsule(px + pw / 2 - 5, py + ph - 4, px + pw / 2 + 5, py + ph - 4, 2.5f, kBackground);
  const float wx = px + pw + 2, wy = py + ph / 2;
  for (int i = 0; i < 3; i++) c.fillArcRight(wx, wy, 9.0f + i * 9.0f, 4.0f, 42.0f, kBlue);
  c.drawTextCentered(110, l.idle, 2, kGrey);
}

void renderFeedback(Canvas& c, Feedback fb, const Labels& l) {
  c.clear(kBackground);
  const float cx = c.width() / 2.0f, cy = 54;
  switch (fb) {
    case Feedback::HomeKeyAccepted:
      c.fillCircle(cx, cy, 42, kGreen);
      c.fillCapsule(cx - 20, cy + 2, cx - 6, cy + 16, 10, kWhite);
      c.fillCapsule(cx - 6, cy + 16, cx + 20, cy - 12, 10, kWhite);
      c.drawTextCentered(112, l.accepted, 2, kGreen);
      break;
    case Feedback::HomeKeyRejected:
      c.fillCircle(cx, cy, 42, kRed);
      c.fillCapsule(cx - 15, cy - 15, cx + 15, cy + 15, 10, kWhite);
      c.fillCapsule(cx + 15, cy - 15, cx - 15, cy + 15, 10, kWhite);
      c.drawTextCentered(112, l.rejected, 2, kRed);
      break;
    case Feedback::TagRead:
      c.fillCircle(cx, cy, 42, kBlue);
      c.drawTextCentered(int(cy), "NFC", 3, kWhite);
      c.drawTextCentered(112, l.tag, 2, kBlue);
      break;
  }
}

}  // namespace statusgfx
