#ifndef MP3_INFO_H
#define MP3_INFO_H

// Duration of an MP3 file from its first bytes, without decoding. The first frame header gives
// bitrate and sample rate. A Xing/Info header (LAME, VBR and CBR) or a VBRI header (Fraunhofer)
// inside that frame gives the number of frames, which makes the duration exact; without one the
// duration follows from the size and the bitrate, as for every CBR file.
//
// Pure C++ (no Arduino), so the host tests check it. Everything is inline: see CLAUDE.md on
// headers that more than one translation unit may include.

#include <cstddef>
#include <cstdint>
#include <cstring>

struct Mp3Frame {
  int version = 0;          // 1 = MPEG 1, 2 = MPEG 2, 25 = MPEG 2.5
  int layer = 0;            // 1, 2 or 3
  uint32_t bitrate = 0;     // bit/s
  uint32_t sampleRate = 0;  // Hz
  bool mono = false;
  uint32_t length = 0;   // bytes of this frame, header included
  uint32_t samples = 0;  // samples per frame
};

inline uint32_t mp3Be32(const uint8_t *p) {
  return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

// Decodes the four header bytes at p. False for anything that is not a frame this can time:
// reserved values, the "free" bitrate.
inline bool mp3ParseHeader(const uint8_t *p, Mp3Frame &f) {
  if (p[0] != 0xFF || (p[1] & 0xE0) != 0xE0) return false;
  int v = (p[1] >> 3) & 3, l = (p[1] >> 1) & 3, b = p[2] >> 4, s = (p[2] >> 2) & 3, pad = (p[2] >> 1) & 1;
  if (v == 1 || l == 0 || b == 0 || b == 15 || s == 3) return false;
  static const uint16_t rates[3][3] = {{44100, 48000, 32000}, {22050, 24000, 16000}, {11025, 12000, 8000}};
  static const uint16_t kbps[2][3][15] = {
      {// MPEG 1: layer I, II, III
       {0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448},
       {0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384},
       {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320}},
      {// MPEG 2 and 2.5
       {0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256},
       {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160},
       {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160}}};
  f.version = v == 3 ? 1 : v == 2 ? 2 : 25;
  f.layer = 4 - l;
  f.bitrate = kbps[f.version == 1 ? 0 : 1][f.layer - 1][b] * 1000u;
  f.sampleRate = rates[f.version == 1 ? 0 : f.version == 2 ? 1 : 2][s];
  f.mono = ((p[3] >> 6) & 3) == 3;
  if (f.layer == 1) {
    f.samples = 384;
    f.length = (12 * f.bitrate / f.sampleRate + pad) * 4;
  } else {
    f.samples = f.layer == 3 && f.version != 1 ? 576 : 1152;
    f.length = f.samples / 8 * f.bitrate / f.sampleRate + pad;  // 144 (or 72) x bitrate / rate
  }
  return f.length > 4;
}

struct Mp3Info {
  uint32_t ms = 0;       // duration
  uint32_t bitrate = 0;  // average, bit/s
  bool frameCount = false;  // the duration comes from a Xing/Info or VBRI frame count
};

// head: the first bytes of the audio, after any ID3v2 tag (id3TagSize() in podcast-feed.h).
// audioBytes: from there to the end of the file. A frame header only counts if the next frame
// starts right behind it, so junk before the audio is skipped; when the head is too short to
// show the next one, the first header has to do.
inline bool mp3Info(const uint8_t *head, size_t len, uint64_t audioBytes, Mp3Info &info) {
  info = Mp3Info();
  for (size_t i = 0; i + 4 <= len; i++) {
    Mp3Frame f;
    if (!mp3ParseHeader(head + i, f)) continue;
    if (i + f.length + 4 <= len) {
      Mp3Frame g;
      if (!mp3ParseHeader(head + i + f.length, g) || g.version != f.version || g.layer != f.layer ||
          g.sampleRate != f.sampleRate)
        continue;
    }
    uint32_t frames = 0;
    size_t side = f.version == 1 ? (f.mono ? 17 : 32) : (f.mono ? 9 : 17);  // side information
    const uint8_t *x = head + i + 4 + side;
    if (f.layer == 3 && i + 4 + side + 12 <= len && (memcmp(x, "Xing", 4) == 0 || memcmp(x, "Info", 4) == 0)) {
      if (mp3Be32(x + 4) & 1) frames = mp3Be32(x + 8);  // flag 1: the frame count is there
    } else if (i + 36 + 18 <= len && memcmp(head + i + 36, "VBRI", 4) == 0) {
      frames = mp3Be32(head + i + 36 + 14);
    }
    uint64_t bytes = audioBytes > i ? audioBytes - i : 0;
    if (frames) {
      info.ms = (uint32_t)((uint64_t)frames * f.samples * 1000 / f.sampleRate);
      info.bitrate = info.ms ? (uint32_t)(bytes * 8000 / info.ms) : f.bitrate;
      info.frameCount = true;
    } else {
      info.bitrate = f.bitrate;
      info.ms = (uint32_t)(bytes * 8000 / f.bitrate);
    }
    return true;
  }
  return false;
}

#endif
