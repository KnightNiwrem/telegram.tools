#!/usr/bin/env python3
# Part of the telegram.tools rich-message renderer (GPL-3.0, see
# renderer/LICENSE).
"""Compares two render outputs (reference vs candidate) per plan §3.

Checks, in order of severity:
1. metadata dimensions (geometric mismatch is always a failure);
2. exact pixel equality;
3. otherwise writes an amplified diff PNG and reports the differing pixel
   count — thresholds are a reviewer decision (plan Phase 9), never applied
   silently here.

Usage: compare-renders.py reference.png candidate.png [diff-out.png]
Exit codes: 0 identical, 1 pixels differ, 2 geometry differs, 3 usage/io.
"""
import struct
import sys
import zlib


def read_png_rgba(path):
    with open(path, "rb") as file:
        data = file.read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"{path}: not a PNG")
    pos, width, height, bitdepth, colortype = 8, 0, 0, 0, 0
    idat = b""
    while pos < len(data):
        (length,) = struct.unpack(">I", data[pos:pos + 4])
        kind = data[pos + 4:pos + 8]
        body = data[pos + 8:pos + 8 + length]
        if kind == b"IHDR":
            width, height, bitdepth, colortype = struct.unpack(
                ">IIBB", body[:10])
        elif kind == b"IDAT":
            idat += body
        pos += 12 + length
    if bitdepth != 8 or colortype not in (2, 6):
        raise ValueError(f"{path}: unsupported PNG format")
    channels = 4 if colortype == 6 else 3
    raw = zlib.decompress(idat)
    stride = width * channels
    out = bytearray(width * height * 4)
    previous = bytearray(stride)
    pos = 0
    for y in range(height):
        filter_type = raw[pos]
        pos += 1
        line = bytearray(raw[pos:pos + stride])
        pos += stride
        for x in range(stride):
            a = line[x - channels] if x >= channels else 0
            b = previous[x]
            c = previous[x - channels] if x >= channels else 0
            if filter_type == 1:
                line[x] = (line[x] + a) & 0xFF
            elif filter_type == 2:
                line[x] = (line[x] + b) & 0xFF
            elif filter_type == 3:
                line[x] = (line[x] + (a + b) // 2) & 0xFF
            elif filter_type == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pred = a if (pa <= pb and pa <= pc) else b if pb <= pc else c
                line[x] = (line[x] + pred) & 0xFF
        previous = line
        for x in range(width):
            src = x * channels
            dst = (y * width + x) * 4
            out[dst:dst + 3] = line[src:src + 3]
            out[dst + 3] = line[src + 3] if channels == 4 else 255
    return width, height, bytes(out)


def write_png_rgba(path, width, height, pixels):
    def chunk(kind, body):
        payload = kind + body
        return (struct.pack(">I", len(body)) + payload
                + struct.pack(">I", zlib.crc32(payload)))
    raw = b"".join(
        b"\x00" + pixels[y * width * 4:(y + 1) * width * 4]
        for y in range(height))
    with open(path, "wb") as file:
        file.write(b"\x89PNG\r\n\x1a\n")
        file.write(chunk(b"IHDR", struct.pack(
            ">IIBBBBB", width, height, 8, 6, 0, 0, 0)))
        file.write(chunk(b"IDAT", zlib.compress(raw)))
        file.write(chunk(b"IEND", b""))


def main():
    if len(sys.argv) < 3:
        print(__doc__, file=sys.stderr)
        return 3
    reference, candidate = sys.argv[1], sys.argv[2]
    diff_out = sys.argv[3] if len(sys.argv) > 3 else None
    rw, rh, rp = read_png_rgba(reference)
    cw, ch, cp = read_png_rgba(candidate)
    if (rw, rh) != (cw, ch):
        print(f"GEOMETRY: {rw}x{rh} != {cw}x{ch}")
        return 2
    if rp == cp:
        print(f"IDENTICAL: {rw}x{rh}")
        return 0
    differing = 0
    diff = bytearray(rw * rh * 4)
    for index in range(0, len(rp), 4):
        delta = max(
            abs(rp[index + c] - cp[index + c]) for c in range(4))
        if delta:
            differing += 1
            amplified = min(255, delta * 8)
            diff[index] = 255
            diff[index + 1] = 255 - amplified
            diff[index + 2] = 255 - amplified
        diff[index + 3] = 255
    print(f"DIFFER: {differing}/{rw * rh} pixels "
          f"({100.0 * differing / (rw * rh):.3f}%)")
    if diff_out:
        write_png_rgba(diff_out, rw, rh, bytes(diff))
        print(f"diff written to {diff_out}")
    return 1


if __name__ == "__main__":
    sys.exit(main())
