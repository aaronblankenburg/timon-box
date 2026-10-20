#ifndef PODCAST_FEED_H
#define PODCAST_FEED_H

// Podcast helpers that do not touch the hardware: header parsing for the episode download.
// Plain C++ without Arduino types, so the host tests (test/host) can compile this file as is.

#include <strings.h>

#include <cctype>
#include <cstdint>
#include <cstdio>
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

// ---------------------------------------------------------------------------
// Dates
// ---------------------------------------------------------------------------

struct FeedDate {
  int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;  // as written in the feed
  int64_t utc = 0;  // seconds since 1970, for ordering
};

// Days since 1970-01-01 (H. Hinnant's days_from_civil).
inline int64_t daysFromCivil(int y, int m, int d) {
  y -= m <= 2;
  int64_t era = (y >= 0 ? y : y - 399) / 400;
  int64_t yoe = y - era * 400;
  int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

inline int feedMonth(const char *s) {
  // English, plus the German abbreviations some German feeds write by mistake.
  static const char *names[] = {"jan", "feb", "mar", "apr", "may", "jun", "jul", "aug", "sep", "oct", "nov", "dec",
                                "jan", "feb", "mrz", "apr", "mai", "jun", "jul", "aug", "sep", "okt", "nov", "dez"};
  char low[4] = {0};
  for (int i = 0; i < 3 && s[i]; i++) low[i] = (char)tolower((unsigned char)s[i]);
  if ((unsigned char)s[0] == 'M' && (unsigned char)s[1] == 0xC3 && (unsigned char)s[2] == 0xA4) return 3;  // "Mär"
  for (int i = 0; i < 24; i++) {
    if (strncmp(low, names[i], 3) == 0) return i % 12 + 1;
  }
  return 0;
}

// Minutes east of UTC for "+0200", "-07:00", "GMT", "CEST", ...; 0 if unknown.
inline int feedZoneOffset(const char *s) {
  while (*s == ' ') s++;
  if (*s == '+' || *s == '-') {
    int sign = *s == '-' ? -1 : 1;
    s++;
    int digits[4], n = 0;
    while (n < 4 && *s) {
      if (isdigit((unsigned char)*s)) digits[n++] = *s - '0';
      else if (*s != ':') break;
      s++;
    }
    if (n < 4) return 0;
    return sign * ((digits[0] * 10 + digits[1]) * 60 + digits[2] * 10 + digits[3]);
  }
  struct Zone {
    const char *name;
    int offset;
  };
  static const Zone zones[] = {{"GMT", 0}, {"UTC", 0}, {"UT", 0}, {"Z", 0}, {"EST", -300}, {"EDT", -240},
                               {"CST", -360}, {"CDT", -300}, {"MST", -420}, {"MDT", -360}, {"PST", -480},
                               {"PDT", -420}, {"CEST", 120}, {"CET", 60}, {"MESZ", 120}, {"MEZ", 60}, {"BST", 60}};
  for (const Zone &z : zones) {
    size_t len = strlen(z.name);
    if (strncasecmp(s, z.name, len) == 0 && !isalpha((unsigned char)s[len])) return z.offset;
  }
  return 0;
}

static inline bool readInt(const char *&s, int &out, int maxDigits) {
  int v = 0, n = 0;
  while (n < maxDigits && isdigit((unsigned char)*s)) {
    v = v * 10 + (*s - '0');
    s++;
    n++;
  }
  if (!n) return false;
  out = v;
  return true;
}

// RFC 822 dates as feeds write them ("Fri, 9 Oct 2026 14:00:00 +0000") and ISO 8601
// ("2026-10-17T10:00:00+02:00"), which some feeds use instead.
inline bool parseFeedDate(const char *s, FeedDate &d) {
  d = FeedDate();
  while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
  const char *p = s;
  int tz = 0;
  if (isdigit((unsigned char)p[0]) && isdigit((unsigned char)p[1]) && isdigit((unsigned char)p[2]) &&
      isdigit((unsigned char)p[3]) && p[4] == '-') {
    if (!readInt(p, d.year, 4) || *p++ != '-' || !readInt(p, d.month, 2) || *p++ != '-' || !readInt(p, d.day, 2)) return false;
    if (*p == 'T' || *p == ' ') {
      p++;
      if (readInt(p, d.hour, 2) && *p == ':') {
        p++;
        readInt(p, d.minute, 2);
        if (*p == ':') {
          p++;
          readInt(p, d.second, 2);
          if (*p == '.')
            while (isdigit((unsigned char)*++p)) {
            }
        }
      }
      tz = feedZoneOffset(p);
    }
  } else {
    while (isalpha((unsigned char)*p)) p++;  // weekday
    while (*p == ',' || *p == ' ' || *p == '.') p++;
    if (!readInt(p, d.day, 2)) return false;
    while (*p == ' ' || *p == '.' || *p == '-') p++;
    d.month = feedMonth(p);
    if (!d.month) return false;
    while (*p && *p != ' ' && *p != '-') p++;
    while (*p == ' ' || *p == '-') p++;
    if (!readInt(p, d.year, 4)) return false;
    if (d.year < 100) d.year += d.year < 70 ? 2000 : 1900;
    while (*p == ' ') p++;
    if (readInt(p, d.hour, 2) && *p == ':') {
      p++;
      readInt(p, d.minute, 2);
      if (*p == ':') {
        p++;
        readInt(p, d.second, 2);
      }
      tz = feedZoneOffset(p);
    }
  }
  if (d.month < 1 || d.month > 12 || d.day < 1 || d.day > 31 || d.hour > 23 || d.minute > 59 || d.second > 60) return false;
  d.utc = daysFromCivil(d.year, d.month, d.day) * 86400 + d.hour * 3600 + d.minute * 60 + d.second - tz * 60;
  return true;
}

// ---------------------------------------------------------------------------
// Feed parsing
// ---------------------------------------------------------------------------

struct PodcastEpisode {
  std::string title, guid, url, type, episodeType, pubDate;
  uint32_t length = 0;  // as the feed says; often wrong, only for display
  bool dated = false;
  FeedDate date;

  bool valid() const { return !url.empty(); }

  // MP3 by MIME type; by file extension only when the feed names no type or a generic one.
  bool isMp3() const {
    if (type == "audio/mpeg" || type == "audio/mp3" || type == "audio/mpeg3" || type == "audio/x-mpeg" ||
        type == "audio/x-mp3")
      return true;
    bool generic = type.empty() || type == "application/octet-stream" || type == "binary/octet-stream" ||
                   type == "audio/unknown";
    std::string path = url.substr(0, url.find_first_of("?#"));
    return generic && path.size() > 4 && strcasecmp(path.c_str() + path.size() - 4, ".mp3") == 0;
  }
};

inline void appendUtf8(std::string &out, uint32_t cp) {
  if (cp < 0x80) {
    out += (char)cp;
  } else if (cp < 0x800) {
    out += (char)(0xC0 | (cp >> 6));
    out += (char)(0x80 | (cp & 0x3F));
  } else if (cp < 0x10000) {
    out += (char)(0xE0 | (cp >> 12));
    out += (char)(0x80 | ((cp >> 6) & 0x3F));
    out += (char)(0x80 | (cp & 0x3F));
  } else if (cp < 0x110000) {
    out += (char)(0xF0 | (cp >> 18));
    out += (char)(0x80 | ((cp >> 12) & 0x3F));
    out += (char)(0x80 | ((cp >> 6) & 0x3F));
    out += (char)(0x80 | (cp & 0x3F));
  }
}

// Decodes one entity name ("amp", "#38", "#x26"); false if unknown.
inline bool decodeEntity(const std::string &name, std::string &out) {
  if (name == "amp") out += '&';
  else if (name == "lt") out += '<';
  else if (name == "gt") out += '>';
  else if (name == "quot") out += '"';
  else if (name == "apos") out += '\'';
  else if (name.size() > 1 && name[0] == '#') {
    char *end;
    unsigned long cp = name[1] == 'x' || name[1] == 'X' ? strtoul(name.c_str() + 2, &end, 16) : strtoul(name.c_str() + 1, &end, 10);
    if (*end || cp == 0) return false;
    appendUtf8(out, (uint32_t)cp);
  } else {
    return false;
  }
  return true;
}

inline std::string decodeEntities(const std::string &in) {
  std::string out;
  for (size_t i = 0; i < in.size(); i++) {
    if (in[i] == '&') {
      size_t semi = in.find(';', i);
      if (semi != std::string::npos && semi - i <= 10 && decodeEntity(in.substr(i + 1, semi - i - 1), out)) {
        i = semi;
        continue;
      }
    }
    out += in[i];
  }
  return out;
}

// Streaming RSS 2.0 reader. push() the feed in chunks of any size; stop downloading once done().
// Memory stays bounded whatever the feed size: only the fields of the item being read and of
// the newest one so far are kept.
//
// The newest episode is the one with the latest pubDate, not the first in the feed: feeds may
// pin an older item on top, and some list oldest first. Trailers are skipped. After maxItems
// items it stops, unless the dates so far ascend - then the newest is at the end and it reads on.
class FeedParser {
 public:
  std::string channelTitle;
  std::string newFeedUrl;  // itunes:new-feed-url: the podcast moved
  int itemsSeen = 0;
  uint32_t bytes = 0;

  explicit FeedParser(int maxItems = 5, uint32_t maxBytes = 8u << 20) : _maxItems(maxItems), _maxBytes(maxBytes) {}

  void push(const char *data, size_t len) {
    for (size_t i = 0; i < len && !done(); i++) feed(data[i]);
    bytes += len;
  }

  bool done() const {
    bool ascending = _sawIncrease && !_sawDecrease;  // oldest first: the newest is still to come
    return _channelClosed || bytes >= _maxBytes || (itemsSeen >= _maxItems && !ascending);
  }

  // The newest episode, whatever its format (check isMp3()); !valid() if the feed had none.
  const PodcastEpisode &newest() const { return _best; }

 private:
  enum Mode { TEXT, ENTITY, OPEN, NAME, ATTRS, CLOSE, BANG, COMMENT, CDATA, SKIP };
  enum Field { NONE, TITLE, PUBDATE, DCDATE, GUID, EPISODETYPE, CHANNEL_TITLE, NEW_FEED_URL };
  static const size_t MAX_NAME = 64, MAX_ATTRS = 4096, MAX_TEXT = 1024;

  int _maxItems;
  uint32_t _maxBytes;
  Mode _mode = TEXT;
  std::string _name, _attrs, _text, _entity, _bang;
  char _quote = 0;
  int _dashes = 0, _brackets = 0;
  int _depth = 0, _itemLevel = 0, _channelLevel = 0, _captureLevel = 0;
  Field _capture = NONE;
  bool _channelClosed = false, _sawIncrease = false, _sawDecrease = false, _seenItem = false, _haveLast = false;
  int64_t _lastUtc = 0;
  PodcastEpisode _item, _best;
  std::string _dcDate;

  static std::string localName(const std::string &name) {
    size_t colon = name.find(':');
    return colon == std::string::npos ? name : name.substr(colon + 1);
  }

  void text(char c) {
    if (_capture != NONE && _depth == _captureLevel && _text.size() < MAX_TEXT) _text += c;
  }

  void feed(char c) {
    switch (_mode) {
      case TEXT:
        if (c == '<') _mode = OPEN;
        else if (c == '&') {
          _entity.clear();
          _mode = ENTITY;
        } else text(c);
        break;
      case ENTITY:
        if (c == ';') {
          std::string out;
          if (decodeEntity(_entity, out)) {
            for (char o : out) text(o);
          } else {
            text('&');
            for (char o : _entity) text(o);
            text(';');
          }
          _mode = TEXT;
        } else if (_entity.size() < 10 && c != '<' && c != '&') {
          _entity += c;
        } else {  // not an entity after all
          text('&');
          for (char o : _entity) text(o);
          _mode = TEXT;
          feed(c);
        }
        break;
      case OPEN:
        _name.clear();
        _attrs.clear();
        if (c == '/') _mode = CLOSE;
        else if (c == '!') {
          _bang.clear();
          _mode = BANG;
        } else if (c == '?') _mode = SKIP;
        else {
          _name += c;
          _mode = NAME;
        }
        break;
      case NAME:
        if (c == '>' || c == '/' || c == ' ' || c == '\t' || c == '\n' || c == '\r') {
          _mode = ATTRS;
          _quote = 0;
          feed(c);
        } else if (_name.size() < MAX_NAME) {
          _name += c;
        }
        break;
      case ATTRS:
        if (_quote) {
          if (c == _quote) _quote = 0;
        } else if (c == '"' || c == '\'') {
          _quote = c;
        } else if (c == '>') {
          size_t end = _attrs.find_last_not_of(" \t\r\n");
          bool selfClosing = end != std::string::npos && _attrs[end] == '/';
          openTag(selfClosing);
          _mode = TEXT;
          break;
        }
        if (_attrs.size() < MAX_ATTRS) _attrs += c;
        break;
      case CLOSE:
        if (c == '>') {
          closeTag();
          _mode = TEXT;
        } else if (_name.size() < MAX_NAME && c != ' ' && c != '\t' && c != '\n' && c != '\r') {
          _name += c;
        }
        break;
      case BANG:
        _bang += c;
        if (_bang == "--") {
          _dashes = 0;
          _mode = COMMENT;
        } else if (_bang == "[CDATA[") {
          _brackets = 0;
          _mode = CDATA;
        } else if (_bang.size() >= 7 || (_bang != std::string("[CDATA[").substr(0, _bang.size()) &&
                                         _bang != std::string("--").substr(0, _bang.size()))) {
          _mode = c == '>' ? TEXT : SKIP;  // <!DOCTYPE ...> and friends
        }
        break;
      case COMMENT:
        if (c == '-') _dashes++;
        else {
          if (c == '>' && _dashes >= 2) _mode = TEXT;
          _dashes = 0;
        }
        break;
      case CDATA:
        if (c == ']') {
          _brackets++;
        } else if (c == '>' && _brackets >= 2) {
          for (int i = 0; i < _brackets - 2; i++) text(']');
          _mode = TEXT;
        } else {
          for (int i = 0; i < _brackets; i++) text(']');
          _brackets = 0;
          text(c);
        }
        break;
      case SKIP:
        if (c == '>') _mode = TEXT;
        break;
    }
  }

  std::string attr(const char *wanted) const {
    size_t i = 0, n = _attrs.size();
    while (i < n) {
      while (i < n && (_attrs[i] == ' ' || _attrs[i] == '\t' || _attrs[i] == '\n' || _attrs[i] == '\r' || _attrs[i] == '/')) i++;
      size_t nameStart = i;
      while (i < n && _attrs[i] != '=' && _attrs[i] != ' ' && _attrs[i] != '/') i++;
      std::string name = _attrs.substr(nameStart, i - nameStart);
      while (i < n && _attrs[i] == ' ') i++;
      if (i >= n || _attrs[i] != '=') continue;
      i++;
      while (i < n && _attrs[i] == ' ') i++;
      if (i >= n) break;
      char q = _attrs[i];
      std::string value;
      if (q == '"' || q == '\'') {
        size_t close = _attrs.find(q, i + 1);
        if (close == std::string::npos) break;
        value = _attrs.substr(i + 1, close - i - 1);
        i = close + 1;
      } else {
        size_t start = i;
        while (i < n && _attrs[i] != ' ') i++;
        value = _attrs.substr(start, i - start);
      }
      if (name == wanted) return decodeEntities(value);
    }
    return "";
  }

  void startCapture(Field f) {
    _capture = f;
    _captureLevel = _depth;
    _text.clear();
  }

  void openTag(bool selfClosing) {
    int parent = _depth;
    if (!selfClosing) _depth++;
    const std::string local = localName(_name);
    if (_name == "channel") {
      _channelLevel = _depth;
    } else if (_name == "item" && !selfClosing) {
      _itemLevel = _depth;
      _item = PodcastEpisode();
      _dcDate.clear();
      _seenItem = true;
    } else if (_itemLevel && parent == _itemLevel) {
      if (_name == "enclosure") {
        if (_item.url.empty()) {  // the first one; a second enclosure is a rare alternative format
          std::string type = attr("type");
          std::string url = attr("url");
          bool audio = type.empty() || type.compare(0, 6, "audio/") == 0 || type == "application/octet-stream";
          if (!url.empty() && audio) {
            _item.url = url;
            _item.type = type;
            _item.length = (uint32_t)strtoul(attr("length").c_str(), nullptr, 10);
          }
        }
      } else if (!selfClosing) {
        if (_name == "title") startCapture(TITLE);
        else if (_name == "pubDate") startCapture(PUBDATE);
        else if (_name == "dc:date") startCapture(DCDATE);
        else if (_name == "guid") startCapture(GUID);
        else if (local == "episodeType") startCapture(EPISODETYPE);
      }
    } else if (!_itemLevel && _channelLevel && parent == _channelLevel && !selfClosing) {
      if (_name == "title" && !_seenItem && channelTitle.empty()) startCapture(CHANNEL_TITLE);
      else if (local == "new-feed-url") startCapture(NEW_FEED_URL);
    }
  }

  static std::string trim(const std::string &s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
  }

  void closeTag() {
    if (_capture != NONE && _depth == _captureLevel) {
      std::string value = trim(_text);
      switch (_capture) {
        case TITLE: _item.title = value; break;
        case PUBDATE: _item.pubDate = value; break;
        case DCDATE: _dcDate = value; break;
        case GUID: _item.guid = value; break;
        case EPISODETYPE: _item.episodeType = value; break;
        case CHANNEL_TITLE: channelTitle = value; break;
        case NEW_FEED_URL: newFeedUrl = value; break;
        default: break;
      }
      _capture = NONE;
    }
    if (_depth > 0) _depth--;
    if (_itemLevel && _depth < _itemLevel) {
      _itemLevel = 0;
      finishItem();
    }
    if (_channelLevel && _depth < _channelLevel) _channelClosed = true;
  }

  void finishItem() {
    itemsSeen++;
    if (_item.episodeType == "trailer" || !_item.valid()) return;
    _item.dated = parseFeedDate(_item.pubDate.c_str(), _item.date) ||
                  (!_dcDate.empty() && parseFeedDate(_dcDate.c_str(), _item.date));
    if (_item.dated) {
      if (_haveLast) (_item.date.utc > _lastUtc ? _sawIncrease : _sawDecrease) = true;
      _lastUtc = _item.date.utc;
      _haveLast = true;
    }
    bool newer = !_best.valid() || (_item.dated && (!_best.dated || _item.date.utc > _best.date.utc));
    if (newer) _best = _item;
  }
};

// ---------------------------------------------------------------------------
// Episode files
// ---------------------------------------------------------------------------

inline uint32_t episodeIdHash(const std::string &s) {  // FNV-1a
  uint32_t h = 2166136261u;
  for (unsigned char c : s) h = (h ^ c) * 16777619u;
  return h;
}

// Title -> file name part: German umlauts spelled out, everything else outside [A-Za-z0-9-.,+()&]
// becomes one '_'. No '[' or ']': "vol[n]" in a file name caps the volume.
inline std::string fileNamePart(const std::string &title, size_t maxLen) {
  std::string out;
  bool underscore = false;
  auto put = [&](const char *s) {
    for (; *s && out.size() < maxLen; s++) out += *s;
    underscore = false;
  };
  for (size_t i = 0; i < title.size() && out.size() < maxLen; i++) {
    unsigned char c = title[i];
    if (c == 0xC3 && i + 1 < title.size()) {
      unsigned char d = title[i + 1];
      const char *rep = d == 0xA4 ? "ae" : d == 0xB6 ? "oe" : d == 0xBC ? "ue" : d == 0x84 ? "Ae" : d == 0x96 ? "Oe" : d == 0x9C ? "Ue" : d == 0x9F ? "ss" : nullptr;
      if (rep) {
        put(rep);
        i++;
        continue;
      }
    }
    if (isalnum(c) || c == '-' || c == '.' || c == ',' || c == '+' || c == '(' || c == ')' || c == '&') {
      out += (char)c;
      underscore = false;
    } else if (!underscore && !out.empty()) {
      out += '_';
      underscore = true;
    }
    if (c >= 0xC0) {  // skip the continuation bytes of other UTF-8 characters
      while (i + 1 < title.size() && ((unsigned char)title[i + 1] & 0xC0) == 0x80) i++;
    }
  }
  while (!out.empty() && (out.back() == '_' || out.back() == '.')) out.pop_back();
  return out.empty() ? "Episode" : out;
}

// "2026-10-19_0400_Erntezeit_1a2b.mp3": sorts by date and time, also with several episodes a day;
// the 4 hex digits come from the guid and keep the name unique and stable. At most 100 characters.
inline std::string episodeFileName(const PodcastEpisode &e) {
  char prefix[24];
  if (e.dated) {
    snprintf(prefix, sizeof(prefix), "%04d-%02d-%02d_%02d%02d_", e.date.year, e.date.month, e.date.day, e.date.hour,
             e.date.minute);
  } else {
    snprintf(prefix, sizeof(prefix), "0000-00-00_0000_");
  }
  char id[8];
  snprintf(id, sizeof(id), "_%04x", (unsigned)(episodeIdHash(e.guid.empty() ? e.url : e.guid) & 0xFFFF));
  return std::string(prefix) + fileNamePart(e.title, 100 - strlen(prefix) - strlen(id) - 4) + id + ".mp3";
}

// True for names episodeFileName() produces; only those count as episodes (for retention and
// "newest"), so files uploaded into a podcast album are left alone.
inline bool isEpisodeFileName(const char *name) {
  for (int i = 0; i < 16; i++) {
    char c = name[i];
    bool digitPos = !(i == 4 || i == 7 || i == 10 || i == 15);
    if (digitPos ? !isdigit((unsigned char)c) : c != (i == 4 || i == 7 ? '-' : '_')) return false;
  }
  size_t len = strlen(name);
  return len > 25 && strcasecmp(name + len - 4, ".mp3") == 0;
}

// The order a podcast folder plays in: episodes newest first, then the other files (uploads) in
// name order, then the end. The playlist is sorted by name, and episode names start with their
// date, so the newest episode is the last one. `isEpisode(i)` tells whether entry i is an episode.
// Returns the entry after `current`, the first one for current = -1, and -1 at the end.
template <typename IsEpisode>
inline int podcastOrderNext(int count, int current, IsEpisode isEpisode) {
  auto firstOther = [&](void) {
    for (int i = 0; i < count; i++)
      if (!isEpisode(i)) return i;
    return -1;
  };
  if (current < 0 || current >= count) {
    for (int i = count - 1; i >= 0; i--)
      if (isEpisode(i)) return i;
    return firstOther();
  }
  if (isEpisode(current)) {
    for (int i = current - 1; i >= 0; i--)
      if (isEpisode(i)) return i;
    return firstOther();
  }
  for (int i = current + 1; i < count; i++)
    if (!isEpisode(i)) return i;
  return -1;
}

// ---------------------------------------------------------------------------
// Download planning
// ---------------------------------------------------------------------------

// What to do with an episode file, given what is on the card and what the server says.
struct ResumePlan {
  bool complete = false;  // nothing to download
  bool restart = false;   // throw away what is on the card
  uint32_t skip = 0;      // server bytes left out at the start (the ID3 tag)
  uint32_t from = 0;      // first server byte to request
  uint32_t expected = SIZE_UNKNOWN;  // final size of the local file
};

// local: bytes on the card; localHasId3: the local file starts with a tag (a full copy);
// ranges: the server honoured a Range request; total: file size on the server;
// tagSize: id3TagSize() of the server's first bytes; savedTotal: the server size the partial
// file was started with (SIZE_UNKNOWN if unknown) - ad-stitching servers serve a different
// file each time, so a partial file is only continued while that size still matches.
inline ResumePlan planResume(uint32_t local, bool localHasId3, bool ranges, uint32_t total, uint32_t tagSize,
                             bool allowSkip, uint32_t savedTotal) {
  ResumePlan p;
  p.skip = (allowSkip && !localHasId3) ? tagSize : 0;
  if (total != SIZE_UNKNOWN && p.skip >= total) p.skip = 0;
  p.expected = total != SIZE_UNKNOWN ? total - p.skip : SIZE_UNKNOWN;
  if (local > 0 && p.expected != SIZE_UNKNOWN && local == p.expected &&
      (savedTotal == SIZE_UNKNOWN || savedTotal == total)) {
    p.complete = true;
    return p;
  }
  bool sameFile = savedTotal == SIZE_UNKNOWN || savedTotal == total;
  if (local > 0 && (!ranges || !sameFile || (p.expected != SIZE_UNKNOWN && local > p.expected))) p.restart = true;
  p.from = p.skip + (p.restart ? 0 : local);
  return p;
}

#endif
