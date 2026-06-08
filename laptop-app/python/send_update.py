#!/usr/bin/env python3
"""
OTA Phase 1 test sender — Laptop → QNX
Usage: python3 send_update.py <image_file> <qnx_ip>
"""

import socket
import hashlib
import json
import struct
import sys
import os
import time

QNX_PORT = 55000

def compute_sha256(path):
    print(f"[SEND] Computing SHA256 of {path}...")
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    digest = h.hexdigest()
    print(f"[SEND] SHA256 = {digest}")
    return digest

def send_update(image_path, qnx_ip):
    image_size = os.path.getsize(image_path)
    sha256     = compute_sha256(image_path)

    header = {
        "version"   : "1.0.0-test",
        "imageSize" : image_size,
        "sha256"    : sha256,
        "signature" : ""          # Phase 3
    }
    header_json  = json.dumps(header).encode("utf-8")
    header_len   = struct.pack(">I", len(header_json))   # 4-byte big-endian

    print(f"[SEND] Connecting to {qnx_ip}:{QNX_PORT}...")
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    
    try:
        sock.connect((qnx_ip, QNX_PORT))
        print("[SEND] Connected successfully to QNX board!")
    except socket.gaierror:
        print(f"[SEND] ERROR: Your computer cannot resolve the host '{qnx_ip}'.")
        sys.exit(1)
    except ConnectionRefusedError:
        print(f"[SEND] ERROR: Connection refused. Is your QNX server app actually listening on port {QNX_PORT}?")
        sys.exit(1)
    except Exception as e:
        print(f"[SEND] ERROR: Failed to connect: {e}")
        sys.exit(1)

    # Send header length + header JSON
    try:
        sock.sendall(header_len)
        sock.sendall(header_json)
        print(f"[SEND] Header sent ({len(header_json)} bytes)")

        # Stream image
        print(f"[SEND] Sending image ({image_size} bytes)...")
        sent     = 0
        last_pct = -1
        t0       = time.time()

        with open(image_path, "rb") as f:
            while True:
                chunk = f.read(512 * 1024)
                if not chunk:
                    break
                sock.sendall(chunk)
                sent += len(chunk)
                pct = int((sent * 100) / image_size)
                if pct != last_pct:
                    elapsed = time.time() - t0
                    rate    = (sent / elapsed / 1024 / 1024) if elapsed > 0 else 0
                    print(f"\r[SEND] {sent}/{image_size} bytes ({pct}%)  {rate:.1f} MB/s   ",
                          end="", flush=True)
                    last_pct = pct

        print(f"\n[SEND] Done. Total time: {time.time()-t0:.1f}s")
    except BrokenPipeError:
        print("\n[SEND] ERROR: QNX closed the connection unexpectedly during transmission (Broken Pipe).")
    finally:
        sock.close()

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <image_file> <qnx_ip>")
        sys.exit(1)

    image_path = sys.argv[1]
    qnx_ip     = sys.argv[2]

    if not os.path.exists(image_path):
        print(f"[SEND] ERROR: File not found: {image_path}")
        sys.exit(1)

    send_update(image_path, qnx_ip)

