#!/usr/bin/env python3
"""Holds the box's serial port for a whole test session.

Logs every line the box prints, with seconds since the start, to LOG, and sends every line
written to FIFO to the box (a ">>>" line in the log marks it). One long-lived process instead
of short captures: some ways of opening the port reset the ESP32, and a second reader garbles
both. The process ends when the port goes away (box unplugged or switched off), so it never
keeps a dead port; the box then comes back under the next free /dev/ttyUSB number.

    setsid nohup ~/.platformio/penv/bin/python tools/serial-bridge.py /dev/ttyUSB0 box.log box.fifo &
    echo 'heap:' > box.fifo        # always "command:" with the colon, a few seconds apart
    tail -f box.log

Stop it before flashing (the upload needs the port): kill the PID that `fuser /dev/ttyUSB0` shows.
"""
import os
import sys
import threading
import time

import serial

port, log_path, fifo_path = sys.argv[1], sys.argv[2], sys.argv[3]
if not os.path.exists(fifo_path):
    os.mkfifo(fifo_path)
s = serial.Serial(port, 115200, timeout=0.5)  # plain open: no dtr/rts changes, they reset the box
t0 = time.time()
log = open(log_path, "a", buffering=1)


def stamp(text):
    log.write("%8.1f  %s\n" % (time.time() - t0, text))


def reader():
    buf = b""
    while True:
        try:
            data = s.read(512)
        except serial.SerialException as e:
            stamp("[bridge] port gone (%s), exiting" % e)
            os._exit(1)
        if not data:
            continue
        buf += data
        while b"\n" in buf:
            line, buf = buf.split(b"\n", 1)
            stamp(line.decode(errors="replace").rstrip("\r"))


def writer():
    while True:
        with open(fifo_path) as f:  # blocks until someone writes
            for line in f:
                line = line.rstrip("\n")
                stamp(">>> " + line)
                try:
                    s.write((line + "\n").encode())
                except serial.SerialException as e:
                    stamp("[bridge] port gone (%s), exiting" % e)
                    os._exit(1)


threading.Thread(target=reader, daemon=True).start()
writer()
