import os
import sys
import time
import serial
import argparse
import struct
import zlib
from serial.tools import list_ports
import serial.serialutil

MAGIC = 0xA5A5A5A5
FRAME_HEADER = 0x01
CHUNK_PACKET = 0x02

def detect_port(preferred=None):
    ports = list_ports.comports()
    if preferred:
        for p in ports:
            if preferred in p.device or (p.description and preferred in p.description):
                return p.device
    for p in ports:
        dev = p.device.lower()
        desc = (p.description or "").lower()
        if "acm" in dev or "usb" in dev or "ttyusb" in dev or "cu.slab" in dev or "ch340" in desc or "cp210" in desc or "usb serial" in desc:
            return p.device
    if ports:
        return ports[0].device
    return None

def open_serial(port, baud):
    ser = serial.Serial(port, baud, timeout=1)
    ser.reset_input_buffer()
    ser.reset_output_buffer()
    return ser

def read_exact(ser, size):
    buf = bytearray()
    while len(buf) < size:
        chunk = ser.read(size - len(buf))
        if not chunk:
            return None
        buf.extend(chunk)
    return bytes(buf)

def find_magic(ser, timeout=2.0):
    window = bytearray()
    start = time.time()
    while True:
        if time.time() - start > timeout:
            return False
        b = ser.read(1)
        if not b:
            continue
        window += b
        if len(window) > 4:
            window.pop(0)
        if len(window) == 4 and struct.unpack('<I', bytes(window))[0] == MAGIC:
            return True

def receive_single_frame(port=None, baud=115200, out_file=None):
    if out_file is None:
        timestamp = time.strftime("%Y-%m-%d_%H-%M-%S")
        out_file = f"image_{timestamp}.jpg"
    if port is None:
        port = detect_port()
        if port is None:
            raise SystemExit('no serial ports found')
    ser = open_serial(port, baud)
    print(f'connected to {ser.port} @ {baud}', file=sys.stderr, flush=True)
    ser.write(b'R')
    ser.flush()
    print('waiting for 0xFE ack...', file=sys.stderr, flush=True)
    ack_start = time.time()
    while time.time() - ack_start < 2.0:
        b = ser.read(1)
        if b == b'\xFE':
            print('got ack', file=sys.stderr, flush=True)
            break
    chunks = []
    expected_total = 0
    expected_size = 0
    print('waiting for frame header...', file=sys.stderr, flush=True)
    if not find_magic(ser):
        ser.close()
        raise SystemExit('timeout waiting for frame header')
    typb = read_exact(ser, 1)
    if not typb or typb[0] != FRAME_HEADER:
        ser.close()
        raise SystemExit('expected frame header')
    hdr = read_exact(ser, 4 + 2 + 2)
    if not hdr:
        ser.close()
        raise SystemExit('failed to read frame header')
    total_size, chunk_size, total_chunks = struct.unpack('<I H H', hdr)
    expected_total = total_chunks
    expected_size = total_size
    chunks = [None] * total_chunks
    print(f'expecting {total_chunks} chunks, {total_size} bytes', file=sys.stderr, flush=True)
    received = 0
    while received < expected_total:
        if not find_magic(ser, timeout=5.0):
            break
        typb = read_exact(ser, 1)
        if not typb or typb[0] != CHUNK_PACKET:
            continue
        rest = read_exact(ser, 2 + 2 + 4)
        if not rest:
            continue
        chunk_idx, payload_len, crc = struct.unpack('<H H I', rest)
        payload = read_exact(ser, payload_len)
        if payload is None:
            continue
        calc = zlib.crc32(payload) & 0xFFFFFFFF
        if calc != crc:
            print(f'crc mismatch chunk {chunk_idx}', file=sys.stderr, flush=True)
            continue
        if chunk_idx < len(chunks) and chunks[chunk_idx] is None:
            chunks[chunk_idx] = payload
            received += 1
            print(f'received chunk {chunk_idx} ({received}/{expected_total})', file=sys.stderr, flush=True)
    ser.close()
    if received == expected_total and all(c is not None for c in chunks):
        data = b''.join(chunks)
        with open(out_file, 'wb') as f:
            f.write(data)
        print(f'wrote {out_file} ({len(data)} bytes)', file=sys.stderr, flush=True)
    else:
        print(f'incomplete: got {received}/{expected_total} chunks', file=sys.stderr, flush=True)
        sys.exit(1)

if __name__ == '__main__':
    p = argparse.ArgumentParser(description='Single-shot serial JPEG receiver')
    p.add_argument('--port', '-p', help='serial port to open, autodetect if omitted')
    p.add_argument('--baud', '-b', type=int, default=115200)
    p.add_argument('--out', '-o', help='output file (default: timestamped like screenshots)')
    args = p.parse_args()
    receive_single_frame(port=args.port, baud=args.baud, out_file=args.out)

