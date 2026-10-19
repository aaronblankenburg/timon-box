// Host tests for src/podcast-feed.h (no ESP32 needed). Run: tools/host-test.sh
//
// The fixtures are the first seven items of real feeds from different hosters (public
// broadcasters, Simplecast with a five-hop tracking chain, Podlove, Podigee, Anchor/Spotify).
// fixtures/expected.txt comes from make_expected.py, an independent regex + Python RFC 822
// reader, so the parser is checked against something other than itself.

#include "../../src/podcast-feed.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <vector>

static int checks = 0, failures = 0;

#define CHECK(cond)                                                           \
  do {                                                                        \
    checks++;                                                                 \
    if (!(cond)) {                                                            \
      failures++;                                                             \
      std::cout << "FAIL " << __FILE__ << ":" << __LINE__ << ": " #cond "\n"; \
    }                                                                         \
  } while (0)

#define CHECK_EQ(a, b)                                                                                            \
  do {                                                                                                            \
    checks++;                                                                                                     \
    auto _a = (a);                                                                                                \
    auto _b = (b);                                                                                                \
    if (!(_a == _b)) {                                                                                            \
      failures++;                                                                                                 \
      std::cout << "FAIL " << __FILE__ << ":" << __LINE__ << ": " #a " == " #b "\n  got:      " << _a << "\n" \
                << "  expected: " << _b << "\n";                                                                  \
    }                                                                                                             \
  } while (0)

static std::string readFile(const std::string &path) {
  std::ifstream in(path, std::ios::binary);
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

static std::string dateTime(const PodcastEpisode &e) {
  char buf[32];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d", e.date.year, e.date.month, e.date.day, e.date.hour,
           e.date.minute);
  return buf;
}

static void testId3() {
  const uint8_t wdr[10] = {'I', 'D', '3', 4, 0, 0, 0x00, 0x32, 0x0f, 0x4d};  // the 821 kB WDR cover tag
  CHECK_EQ(id3TagSize(wdr, 10), 821207u);
  const uint8_t footer[10] = {'I', 'D', '3', 4, 0, 0x10, 0, 0, 0x02, 0x01};
  CHECK_EQ(id3TagSize(footer, 10), 10u + 257u + 10u);
  const uint8_t v3flag[10] = {'I', 'D', '3', 3, 0, 0x10, 0, 0, 0x02, 0x01};  // no footer before v2.4
  CHECK_EQ(id3TagSize(v3flag, 10), 10u + 257u);
  const uint8_t notSyncsafe[10] = {'I', 'D', '3', 4, 0, 0, 0x80, 0, 0, 0};
  CHECK_EQ(id3TagSize(notSyncsafe, 10), 0u);
  const uint8_t mp3[10] = {0xFF, 0xFB, 0x90, 0x64, 0, 0, 0, 0, 0, 0};
  CHECK_EQ(id3TagSize(mp3, 10), 0u);
  CHECK_EQ(id3TagSize(wdr, 9), 0u);
  CHECK(isMp3FrameSync(0xFF, 0xFB));
  CHECK(isMp3FrameSync(0xFF, 0xE3));
  CHECK(!isMp3FrameSync(0xFF, 0x00));
  CHECK(!isMp3FrameSync('I', 'D'));
}

static void testContentRange() {
  uint32_t s, e, t;
  CHECK(parseContentRange("bytes 0-9/58520663", s, e, t));
  CHECK_EQ(s, 0u);
  CHECK_EQ(e, 9u);
  CHECK_EQ(t, 58520663u);
  CHECK(parseContentRange("bytes 1000000-58520662/58520663", s, e, t));
  CHECK_EQ(s, 1000000u);
  CHECK(parseContentRange("bytes 5-6/*", s, e, t));
  CHECK_EQ(t, SIZE_UNKNOWN);
  CHECK(!parseContentRange("", s, e, t));
  CHECK(!parseContentRange("bytes */100", s, e, t));
  CHECK(!parseContentRange("items 0-9/10", s, e, t));
  CHECK(!parseContentRange(nullptr, s, e, t));
}

static void testUrls() {
  CHECK_EQ(resolveUrl("https://a.com/x/y.mp3?q=1", "https://b.com/z"), std::string("https://b.com/z"));
  CHECK_EQ(resolveUrl("https://a.com/x/y.mp3", "http://b.com/z"), std::string("http://b.com/z"));
  CHECK_EQ(resolveUrl("https://a.com/x/y.mp3", "//c.net/p"), std::string("https://c.net/p"));
  CHECK_EQ(resolveUrl("http://a.com/x/y.mp3", "/abs?x=1"), std::string("http://a.com/abs?x=1"));
  CHECK_EQ(resolveUrl("https://a.com/x/y.mp3?q=1", "z.mp3"), std::string("https://a.com/x/z.mp3"));
  CHECK_EQ(resolveUrl("https://a.com", "z"), std::string("https://a.com/z"));
  CHECK_EQ(plainHttpVariant("https://cdn.example/a.mp3?x=1"), std::string("http://cdn.example/a.mp3?x=1"));
  CHECK_EQ(plainHttpVariant("http://cdn.example/a.mp3"), std::string("http://cdn.example/a.mp3"));
}

static void testDates() {
  FeedDate d;
  CHECK(parseFeedDate("Mon, 19 Oct 2026 04:00:44 GMT", d));
  CHECK_EQ(d.utc, (int64_t)1792382444);
  CHECK(d.year == 2026 && d.month == 10 && d.day == 19 && d.hour == 4 && d.minute == 0);
  CHECK(parseFeedDate("Sat, 17 Oct 2026 10:00:00 +0200", d));
  CHECK_EQ(d.utc, (int64_t)1792224000);
  CHECK_EQ(d.hour, 10);  // as written, for the file name
  CHECK(parseFeedDate("Fri, 9 Oct 2026 14:00:00 +0000", d));
  CHECK_EQ(d.utc, (int64_t)1791554400);
  CHECK(parseFeedDate("19 Oct 2026 04:00 GMT", d));
  CHECK_EQ(d.utc, (int64_t)1792382400);
  CHECK(parseFeedDate("Tue, 10 Jun 03 04:00:00 EST", d));
  CHECK_EQ(d.utc, (int64_t)1055235600);
  CHECK(parseFeedDate("Fr, 16 Okt 2026 10:00:39 +0200", d));  // German month name
  CHECK(d.month == 10 && d.day == 16);
  CHECK(parseFeedDate("2026-10-17T10:00:00+02:00", d));
  CHECK_EQ(d.utc, (int64_t)1792224000);
  CHECK(parseFeedDate("2026-10-17", d));
  CHECK_EQ(d.utc, (int64_t)1792195200);
  CHECK(parseFeedDate("  Mon, 19 Oct 2026 04:00:44 GMT\n", d));
  CHECK(!parseFeedDate("", d));
  CHECK(!parseFeedDate("yesterday", d));
  CHECK(!parseFeedDate("32 Oct 2026 10:00:00 GMT", d));
  CHECK(!parseFeedDate("Mon, 19 Foo 2026 04:00:44 GMT", d));
}

static PodcastEpisode episode(const char *title, const char *date, const char *guid) {
  PodcastEpisode e;
  e.title = title;
  e.guid = guid;
  e.url = "https://x/a.mp3";
  e.dated = date && parseFeedDate(date, e.date);
  return e;
}

static void testFileNames() {
  std::string a = episodeFileName(episode("Erntezeit", "Mon, 19 Oct 2026 04:00:44 GMT", "/1a1ca6b7"));
  CHECK_EQ(a.substr(0, 26), std::string("2026-10-19_0400_Erntezeit_"));
  CHECK_EQ(a.size(), 26u + 4u + 4u);
  CHECK(a.compare(a.size() - 4, 4, ".mp3") == 0);
  CHECK(isEpisodeFileName(a.c_str()));
  CHECK_EQ(episodeFileName(episode("Erntezeit", "Mon, 19 Oct 2026 04:00:44 GMT", "/1a1ca6b7")), a);  // stable
  CHECK(episodeFileName(episode("Erntezeit", "Mon, 19 Oct 2026 04:00:44 GMT", "other")) != a);      // unique

  CHECK_EQ(fileNamePart("Kakadu - eure Fragen: - Warum darf ich nicht l\xc3\xa4nger Zocken?", 75),
           std::string("Kakadu_-_eure_Fragen_-_Warum_darf_ich_nicht_laenger_Zocken"));
  CHECK_EQ(fileNamePart("CheckPod Klima | Viel mehr als Wetter", 75), std::string("CheckPod_Klima_Viel_mehr_als_Wetter"));
  CHECK_EQ(fileNamePart("Freundschaft \xe2\x80\x93 Check In", 75), std::string("Freundschaft_Check_In"));
  CHECK_EQ(fileNamePart("Die Tiere im Zoo, 2. Teil - Folge 27", 75), std::string("Die_Tiere_im_Zoo,_2._Teil_-_Folge_27"));
  CHECK_EQ(fileNamePart("Gr\xc3\xb6\xc3\x9f" "e \xc3\x84rger \xc3\x9c" "ber", 75), std::string("Groesse_Aerger_Ueber"));
  CHECK_EQ(fileNamePart("loud vol[21] track", 75), std::string("loud_vol_21_track"));  // no vol[] cap
  CHECK_EQ(fileNamePart("\xf0\x9f\x8e\x89 Party!", 75), std::string("Party"));
  CHECK_EQ(fileNamePart("", 75), std::string("Episode"));
  CHECK_EQ(fileNamePart("???", 75), std::string("Episode"));

  std::string longName = episodeFileName(episode(std::string(300, 'x').c_str(), "Mon, 19 Oct 2026 04:00:44 GMT", "g"));
  CHECK_EQ(longName.size(), 100u);
  CHECK(isEpisodeFileName(longName.c_str()));
  std::string undated = episodeFileName(episode("Ohne Datum", nullptr, "g"));
  CHECK_EQ(undated.substr(0, 27), std::string("0000-00-00_0000_Ohne_Datum_"));

  CHECK(!isEpisodeFileName("01-Kalle_Klang-Bruder_Jakob.mp3"));
  CHECK(!isEpisodeFileName("2026-10-19_0400_Erntezeit_1a2b.txt"));
  CHECK(!isEpisodeFileName("2026-10-19-0400_Erntezeit_1a2b.mp3"));
}

static std::vector<std::string> split(const std::string &line, char sep) {
  std::vector<std::string> out;
  std::stringstream ss(line);
  std::string part;
  while (std::getline(ss, part, sep)) out.push_back(part);
  return out;
}

static void testFixtures(const std::string &dir) {
  std::ifstream expected(dir + "/expected.txt");
  std::string line;
  int feeds = 0;
  std::mt19937 rng(42);
  while (std::getline(expected, line)) {
    std::vector<std::string> f = split(line, '\t');
    if (f.size() != 4) continue;
    feeds++;
    std::string xml = readFile(dir + "/" + f[0] + ".xml");
    CHECK(!xml.empty());

    FeedParser whole;
    whole.push(xml.data(), xml.size());
    // Chunks of random size, as they come from the network: tags, entities and CDATA get split.
    FeedParser chunked;
    for (size_t pos = 0; pos < xml.size() && !chunked.done();) {
      size_t n = std::uniform_int_distribution<size_t>(1, 1500)(rng);
      if (n > xml.size() - pos) n = xml.size() - pos;
      chunked.push(xml.data() + pos, n);
      pos += n;
    }
    for (FeedParser *p : {&whole, &chunked}) {
      const PodcastEpisode &e = p->newest();
      std::cout << "  " << f[0] << ": " << e.title.substr(0, 50) << " (" << dateTime(e) << ")\n";
      CHECK_EQ(e.title, f[2]);
      CHECK_EQ(dateTime(e), f[1]);
      CHECK_EQ(e.url, f[3]);
      CHECK(e.isMp3());
      CHECK(p->done());
      CHECK_EQ(p->itemsSeen, 5);  // newest first: no need to read on
      CHECK(!p->channelTitle.empty());
    }
  }
  CHECK_EQ(feeds, 8);
}

static std::string item(const char *title, const char *date, const char *extra = "",
                        const char *enclosure = "<enclosure url=\"https://x.example/%s.mp3\" type=\"audio/mpeg\" length=\"1\"/>") {
  char enc[512];
  snprintf(enc, sizeof(enc), enclosure, title);
  std::string s = "<item><title>" + std::string(title) + "</title>";
  if (date) s += "<pubDate>" + std::string(date) + "</pubDate>";
  return s + extra + enc + "</item>\n";
}

static FeedParser parse(const std::string &body, const std::string &channelExtra = "") {
  std::string xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<!DOCTYPE rss>\n<rss version=\"2.0\" "
                    "xmlns:itunes=\"http://www.itunes.com/dtds/podcast-1.0.dtd\"><channel><title>Feed</title>" +
                    channelExtra + body + "</channel></rss>";
  FeedParser p;
  for (char c : xml) p.push(&c, 1);  // worst case: one byte at a time
  return p;
}

static void testSynthetic() {
  {  // entities in text are decoded, inside CDATA they are not
    FeedParser p = parse("<item><title>Tom &amp; Jerry &#8211; &#x2013; &bogus; &lt;3</title>"
                         "<enclosure url='https://x.example/a.mp3?a=1&amp;b=2' type='audio/mpeg'/></item>");
    CHECK_EQ(p.newest().title, std::string("Tom & Jerry \xe2\x80\x93 \xe2\x80\x93 &bogus; <3"));
    CHECK_EQ(p.newest().url, std::string("https://x.example/a.mp3?a=1&b=2"));
    FeedParser q = parse("<item><title><![CDATA[Tom &amp; Jerry ]] ]]]></title>"
                         "<enclosure url=\"https://x.example/a.mp3\" type=\"audio/mpeg\"></enclosure></item>");
    CHECK_EQ(q.newest().title, std::string("Tom &amp; Jerry ]] ]"));
    CHECK(q.newest().valid());
  }
  {  // the newest by date wins, wherever it is; trailers and video do not count
    std::string body = item("old pinned", "Mon, 01 Jan 2019 10:00:00 +0000") +
                       item("trailer", "Tue, 20 Oct 2026 10:00:00 +0000", "<itunes:episodeType>trailer</itunes:episodeType>") +
                       item("video", "Tue, 20 Oct 2026 09:00:00 +0000", "", "<enclosure url=\"https://x.example/%s.mp4\" type=\"video/mp4\"/>") +
                       item("newest", "Mon, 19 Oct 2026 10:00:00 +0000", "<itunes:title>Not this</itunes:title>") +
                       item("older", "Sun, 18 Oct 2026 10:00:00 +0000") + item("oldest", "Sat, 17 Oct 2026 10:00:00 +0000") +
                       item("never read", "Wed, 21 Oct 2026 10:00:00 +0000");
    FeedParser p = parse(body);
    CHECK_EQ(p.newest().title, std::string("newest"));
    CHECK_EQ(p.itemsSeen, 5);  // the dates fell, so it stopped: "never read" stays unread by design
  }
  {  // oldest first: it reads on to the end
    std::string body;
    for (int day = 1; day <= 9; day++) {
      char date[64], title[16];
      snprintf(date, sizeof(date), "Thu, %02d Oct 2026 10:00:00 +0000", day);
      snprintf(title, sizeof(title), "day %d", day);
      body += item(title, date);
    }
    FeedParser p = parse(body);
    CHECK_EQ(p.newest().title, std::string("day 9"));
    CHECK_EQ(p.itemsSeen, 9);
  }
  {  // without dates the feed order decides, and it still stops after five
    std::string body;
    for (int i = 1; i <= 8; i++) body += item(("n" + std::to_string(i)).c_str(), nullptr);
    FeedParser p = parse(body);
    CHECK_EQ(p.newest().title, std::string("n1"));
    CHECK_EQ(p.itemsSeen, 5);
  }
  {  // non-MP3 newest is reported as such
    std::string body = item("m4a", "Mon, 19 Oct 2026 10:00:00 +0000", "", "<enclosure url=\"https://x.example/%s.m4a\" type=\"audio/x-m4a\"/>") +
                       item("mp3", "Sun, 18 Oct 2026 10:00:00 +0000");
    FeedParser p = parse(body);
    CHECK_EQ(p.newest().title, std::string("m4a"));
    CHECK(!p.newest().isMp3());
  }
  {  // truncated in the middle of an item: the complete ones count
    std::string xml = "<rss><channel><title>T</title>" + item("one", "Mon, 19 Oct 2026 10:00:00 +0000") +
                      "<item><title>cut off";
    FeedParser p;
    p.push(xml.data(), xml.size());
    CHECK_EQ(p.newest().title, std::string("one"));
    CHECK(!p.done());
  }
  {  // channel data, comments, image title, new-feed-url
    FeedParser p = parse("<!-- <item><title>commented</title></item> -->" + item("real", "Mon, 19 Oct 2026 10:00:00 +0000"),
                         "<image><title>Logo</title></image><itunes:new-feed-url>https://new.example/feed</itunes:new-feed-url>");
    CHECK_EQ(p.channelTitle, std::string("Feed"));
    CHECK_EQ(p.newFeedUrl, std::string("https://new.example/feed"));
    CHECK_EQ(p.newest().title, std::string("real"));
    CHECK(p.done());  // </channel> seen
  }
  {  // no enclosure, only media:content: not an episode
    FeedParser p = parse("<item><title>media</title><media:content url=\"https://x.example/a.mp3\"/></item>");
    CHECK(!p.newest().valid());
  }
  {  // MP3 detection
    PodcastEpisode e;
    e.url = "https://x.example/a.MP3?token=1";
    CHECK(e.isMp3());
    e.type = "application/octet-stream";
    CHECK(e.isMp3());
    e.type = "audio/x-m4a";  // a declared type beats the extension
    CHECK(!e.isMp3());
    e.type = "audio/mpeg";
    e.url = "https://x.example/play/12345";
    CHECK(e.isMp3());
  }
}

static void testPlanResume() {
  const uint32_t U = SIZE_UNKNOWN;
  ResumePlan p = planResume(0, false, true, 1000, 100, true, U);
  CHECK(!p.complete && !p.restart);
  CHECK_EQ(p.skip, 100u);
  CHECK_EQ(p.from, 100u);
  CHECK_EQ(p.expected, 900u);
  p = planResume(400, false, true, 1000, 100, true, 1000);  // continue
  CHECK(!p.complete && !p.restart);
  CHECK_EQ(p.from, 500u);
  p = planResume(900, false, true, 1000, 100, true, 1000);
  CHECK(p.complete);
  p = planResume(1000, true, true, 1000, 100, true, U);  // a full copy with its tag
  CHECK(p.complete);
  CHECK_EQ(p.skip, 0u);
  p = planResume(400, false, false, 1000, 100, true, U);  // server lost range support
  CHECK(p.restart);
  CHECK_EQ(p.from, 100u);
  p = planResume(400, false, true, 1000, 100, true, 1200);  // ad-stitched: a different file now
  CHECK(p.restart);
  p = planResume(950, false, true, 1000, 100, true, U);  // more on the card than the file has
  CHECK(p.restart);
  p = planResume(0, false, true, 1000, 100, false, U);  // tag kept (no MP3 frame after it)
  CHECK_EQ(p.skip, 0u);
  CHECK_EQ(p.expected, 1000u);
  p = planResume(300, false, true, U, 100, true, U);  // size unknown
  CHECK_EQ(p.expected, U);
  CHECK_EQ(p.from, 400u);
  p = planResume(0, false, true, 50, 100, true, U);  // "tag" longer than the file: no tag
  CHECK_EQ(p.skip, 0u);
}

int main(int argc, char **argv) {
  std::string fixtures = argc > 1 ? argv[1] : "test/host/fixtures";
  testId3();
  testContentRange();
  testUrls();
  testDates();
  testFileNames();
  testFixtures(fixtures);
  testSynthetic();
  testPlanResume();
  std::cout << checks - failures << " of " << checks << " checks passed\n";
  return failures ? 1 : 0;
}
