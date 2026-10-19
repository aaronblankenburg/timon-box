"""Writes fixtures/expected.txt: the newest non-trailer episode of every fixture feed, found
with a plain regex reader and Python's RFC 822 parser, as a cross-check for FeedParser."""
import email.utils, html, os, re, sys

here = os.path.dirname(os.path.abspath(__file__))
fixtures = os.path.join(here, "fixtures")

def text(item, tag):
    m = re.search(rb"<" + tag + rb">(.*?)</" + tag + rb">", item, re.S)
    if not m:
        return ""
    v = m.group(1).strip()
    c = re.fullmatch(rb"<!\[CDATA\[(.*)\]\]>", v, re.S)
    v = c.group(1) if c else v
    v = v.decode("utf-8", "replace")
    return (v if c else html.unescape(v)).strip()

lines = []
for name in sorted(f[:-4] for f in os.listdir(fixtures) if f.endswith(".xml")):
    data = open(os.path.join(fixtures, name + ".xml"), "rb").read()
    best = None
    for item in re.findall(rb"<item[\s>].*?</item>", data, re.S):
        if text(item, rb"itunes:episodeType") == "trailer":
            continue
        enc = re.search(rb"<enclosure[^>]*>", item, re.S)
        if not enc:
            continue
        url = re.search(rb'url=["\']([^"\']*)', enc.group(0))
        if not url:
            continue
        when = email.utils.parsedate_to_datetime(text(item, rb"pubDate"))
        cand = (when.timestamp(), when, text(item, rb"title"), html.unescape(url.group(1).decode()))
        if best is None or cand[0] > best[0]:
            best = cand
    _, when, title, url = best
    lines.append(f"{name}\t{when.strftime('%Y-%m-%d %H:%M')}\t{title}\t{url}")
open(os.path.join(fixtures, "expected.txt"), "w").write("\n".join(lines) + "\n")
print("\n".join(l[:150] for l in lines))
