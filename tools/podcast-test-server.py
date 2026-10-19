#!/usr/bin/env python3
"""A podcast server for testing the box's podcast downloads on the local network.

Serves the MP3 files of a directory as a feed, with switches for the awkward cases real
servers produce: no Range support, no Content-Length, chunked transfer, slow links, redirect
chains, connections that drop, and big ID3 tags (podcast covers).

    tools/podcast-test-server.py --dir ~/mp3 --rate 40 --drop-after 300000 --id3 900000

Then enter http://<this machine>:8000/feed.xml as the feed address of a podcast album.
The newest episode is the file that sorts last; dates come from the file times, so they
stay the same between requests (the box names episode files after them).
"""
import argparse
import email.utils
import html
import os
import re
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import quote, unquote

parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
parser.add_argument("--dir", required=True, help="directory with the MP3 files to serve")
parser.add_argument("--port", type=int, default=8000)
parser.add_argument("--rate", type=float, default=0, help="limit each download to this many kB/s")
parser.add_argument("--no-range", action="store_true", help="ignore Range requests, always send the whole file")
parser.add_argument("--no-length", action="store_true", help="send no Content-Length, close at the end")
parser.add_argument("--chunked", action="store_true", help="answer with chunked transfer encoding")
parser.add_argument("--redirects", type=int, default=0, help="redirect hops before the file")
parser.add_argument("--drop-after", type=int, default=0, help="close the first transfer of each file after this many bytes")
parser.add_argument("--id3", type=int, default=0, help="put an ID3 tag of this many bytes in front of every file")
parser.add_argument("--oldest-first", action="store_true", help="list the oldest episode first in the feed")
args = parser.parse_args()
sys.stdout.reconfigure(line_buffering=True)  # the log shows up right away, also in a file

dropped = set()
lock = threading.Lock()


def id3_tag(size):
    """An ID3v2.4 tag of `size` bytes in total: header plus padding, which is valid."""
    body = size - 10
    syncsafe = bytes([(body >> 21) & 0x7F, (body >> 14) & 0x7F, (body >> 7) & 0x7F, body & 0x7F])
    return b"ID3\x04\x00\x00" + syncsafe + bytes(body)


TAG = id3_tag(args.id3) if args.id3 >= 10 else b""


def episodes():
    return sorted(f for f in os.listdir(args.dir) if f.lower().endswith(".mp3"))


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.0"

    def log_message(self, fmt, *a):
        print("%s %s" % (time.strftime("%H:%M:%S"), fmt % a))

    def do_GET(self):
        path = unquote(self.path.split("?")[0])
        if path == "/feed.xml":
            return self.feed()
        m = re.match(r"/r/(\d+)(/.*)", path)
        if m:  # a redirect chain, alternating absolute and relative Location headers
            hops, rest = int(m.group(1)), m.group(2)
            target = f"/r/{hops - 1}{rest}" if hops > 1 else rest
            location = target if hops % 2 else f"http://{self.headers['Host']}{target}"
            self.send_response(302)
            self.send_header("Location", location)
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        if path.startswith("/ep/"):
            return self.episode(path[4:])
        self.send_error(404)

    def feed(self):
        host = self.headers["Host"]
        prefix = f"/r/{args.redirects}" if args.redirects else ""
        items = []
        for name in episodes():
            full = os.path.join(args.dir, name)
            when = email.utils.formatdate(os.path.getmtime(full), usegmt=True)
            url = f"http://{host}{prefix}/ep/{quote(name)}"
            items.append(
                f"<item><title>{html.escape(os.path.splitext(name)[0])}</title>"
                f"<guid isPermaLink=\"false\">{html.escape(name)}</guid><pubDate>{when}</pubDate>"
                f"<enclosure url=\"{html.escape(url)}\" type=\"audio/mpeg\" length=\"{len(TAG) + os.path.getsize(full)}\"/></item>"
            )
        if not args.oldest_first:
            items.reverse()
        body = (
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rss version=\"2.0\"><channel>"
            "<title>Test podcast</title>" + "\n".join(items) + "</channel></rss>\n"
        ).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/rss+xml; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def episode(self, name):
        full = os.path.join(args.dir, os.path.basename(name))
        if not os.path.isfile(full):
            return self.send_error(404)
        size = len(TAG) + os.path.getsize(full)
        start, end, status = 0, size - 1, 200
        wanted = self.headers.get("Range")
        m = re.match(r"bytes=(\d+)-(\d*)", wanted or "")
        if m and not args.no_range:
            start = int(m.group(1))
            end = min(int(m.group(2)), size - 1) if m.group(2) else size - 1
            if start >= size:
                self.send_response(416)
                self.send_header("Content-Range", f"bytes */{size}")
                self.end_headers()
                return
            status = 206
        self.send_response(status)
        self.send_header("Content-Type", "audio/mpeg")
        self.send_header("Accept-Ranges", "none" if args.no_range else "bytes")
        if status == 206:
            self.send_header("Content-Range", f"bytes {start}-{end}/{size}")
        if args.chunked:
            self.send_header("Transfer-Encoding", "chunked")
        elif not args.no_length:
            self.send_header("Content-Length", str(end - start + 1))
        self.end_headers()

        with lock:  # dropped once per file; the box's 10-byte probe does not count
            drop = args.drop_after if args.drop_after and name not in dropped and end - start + 1 > args.drop_after else 0
        began, sent, pos = time.time(), 0, start
        with open(full, "rb") as f:
            while pos <= end:
                n = min(16384, end - pos + 1)
                if pos < len(TAG):
                    chunk = TAG[pos : pos + n]
                else:
                    f.seek(pos - len(TAG))
                    chunk = f.read(n)
                if not chunk:
                    break
                if drop and sent + len(chunk) > drop:
                    chunk = chunk[: drop - sent]
                try:
                    if args.chunked:
                        self.wfile.write(b"%x\r\n" % len(chunk) + chunk + b"\r\n")
                    else:
                        self.wfile.write(chunk)
                except (BrokenPipeError, ConnectionResetError):
                    print(f"  client went away after {sent} bytes")
                    return
                sent += len(chunk)
                pos += len(chunk)
                if drop and sent >= drop:
                    with lock:
                        dropped.add(name)
                    print(f"  dropping the connection after {sent} bytes (as asked)")
                    return
                if args.rate:
                    ahead = sent / (args.rate * 1024) - (time.time() - began)
                    if ahead > 0:
                        time.sleep(ahead)
        if args.chunked:
            self.wfile.write(b"0\r\n\r\n")
        print(f"  sent {sent} bytes of {name} ({status}, from {start})")


print(f"serving {len(episodes())} episodes from {args.dir} on port {args.port}: http://<this machine>:{args.port}/feed.xml")
ThreadingHTTPServer(("", args.port), Handler).serve_forever()
