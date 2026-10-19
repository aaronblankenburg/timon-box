#ifndef PODCAST_FEED_H
#define PODCAST_FEED_H

// Podcast helpers that do not touch the hardware: header parsing for the episode download.
// Plain C++ without Arduino types, so the host tests (test/host) can compile this file as is.

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

// Size of a leading ID3v2 tag including its 10-byte header (and footer, if flagged), or 0 if
// `hdr` does not start one. The size bytes are "syncsafe": 7 bits each, high bit always clear.
inline uint32_t id3TagSize(const uint8_t *hdr, size_t len) {
  if (len < 10 || hdr[0] != 'I' || hdr[1] != 'D' || hdr[2] != '3') return 0;
  if (hdr[3] == 0xFF || hdr[4] == 0xFF) return 0;  // version bytes are never 0xFF
  if ((hdr[6] | hdr[7] | hdr[8] | hdr[9]) & 0x80) return 0;
  uint32_t size = ((uint32_t)hdr[6] << 21) | ((uint32_t)hdr[7] << 14) | ((uint32_t)hdr[8] << 7) | hdr[9];
  bool footer = hdr[3] >= 4 && (hdr[5] & 0x10);
  return 10 + size + (footer ? 10 : 0);
}

// First two bytes of an MPEG audio frame: 11 sync bits set.
inline bool isMp3FrameSync(uint8_t a, uint8_t b) { return a == 0xFF && (b & 0xE0) == 0xE0; }

static const uint32_t SIZE_UNKNOWN = 0xFFFFFFFF;

// "bytes 1000-1999/58520663" -> start 1000, end 1999, total 58520663. A total of "*" gives
// SIZE_UNKNOWN. Returns false if the header is missing or malformed.
inline bool parseContentRange(const char *s, uint32_t &start, uint32_t &end, uint32_t &total) {
  if (!s) return false;
  while (*s == ' ') s++;
  if (strncmp(s, "bytes", 5) != 0) return false;
  s += 5;
  while (*s == ' ' || *s == '=') s++;
  char *p;
  unsigned long a = strtoul(s, &p, 10);
  if (p == s || *p != '-') return false;
  s = p + 1;
  unsigned long b = strtoul(s, &p, 10);
  if (p == s || *p != '/') return false;
  s = p + 1;
  if (*s == '*') {
    total = SIZE_UNKNOWN;
  } else {
    unsigned long t = strtoul(s, &p, 10);
    if (p == s) return false;
    total = (uint32_t)t;
  }
  start = (uint32_t)a;
  end = (uint32_t)b;
  return start <= end;
}

// The target of a redirect. `location` may be absolute ("https://cdn/x"), scheme-relative
// ("//cdn/x"), host-relative ("/x") or relative to the current path ("x").
inline std::string resolveUrl(const std::string &base, const std::string &location) {
  if (location.compare(0, 7, "http://") == 0 || location.compare(0, 8, "https://") == 0) return location;
  size_t schemeEnd = base.find("://");
  if (schemeEnd == std::string::npos) return location;
  if (location.compare(0, 2, "//") == 0) return base.substr(0, schemeEnd + 1) + location;
  size_t hostEnd = base.find('/', schemeEnd + 3);
  std::string origin = hostEnd == std::string::npos ? base : base.substr(0, hostEnd);
  if (!location.empty() && location[0] == '/') return origin + location;
  std::string path = hostEnd == std::string::npos ? "/" : base.substr(hostEnd);
  size_t cut = path.find_first_of("?#");
  if (cut != std::string::npos) path.erase(cut);
  path.erase(path.rfind('/') + 1);
  return origin + path + location;
}

// "https://host/path" -> "http://host/path"; anything else unchanged. Episode files are
// public, and with the certificate check off TLS would only protect against passive
// listeners, while it costs ~45 kB of RAM for the whole download.
inline std::string plainHttpVariant(const std::string &url) {
  if (url.compare(0, 8, "https://") == 0) return "http://" + url.substr(8);
  return url;
}

#endif
