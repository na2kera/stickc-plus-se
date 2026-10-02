"""Render the production drawScreen using desktop M5GFX/SDL2 into PPM files.

Requires a C/C++ compiler and an existing SDL2 development installation.
No ESP32, browser or window is used. The test uses the actual bundled fonts.
"""
import concurrent.futures
import os
from pathlib import Path
import shlex
import subprocess
import sys
import struct
import zlib

ROOT = Path(__file__).resolve().parent.parent
os.chdir(ROOT)
OUT = ROOT / "build" / "preview-objects"
OUT.mkdir(parents=True, exist_ok=True)
(ROOT / "build" / "previews").mkdir(parents=True, exist_ok=True)
sdk = Path("/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk")
flags = ["-O1", "-ffunction-sections", "-fdata-sections", "-Iinclude", "-Ilib/M5GFX/src", "-DLGFX_SDL", "-DMINIGAMES_RENDER_QA"]
flags += shlex.split(subprocess.check_output(["pkg-config", "--cflags", "sdl2"], text=True))
if sys.platform == "darwin" and sdk.exists():
    flags += ["-isysroot", str(sdk)]
cpp = ["src/games/model.cpp", "src/app/app.cpp", "src/platform/render.cpp", "test/render_preview.cpp", "lib/M5GFX/src/lgfx/v1/lgfx_v1.cpp"]
c = list(Path("lib/M5GFX/src/lgfx/Fonts").rglob("*.c")) + list(Path("lib/M5GFX/src/lgfx/utility").glob("*.c"))

def compile_source(source):
    source = Path(source)
    obj = OUT / (str(source).replace("/", "_") + ".o")
    # Only library sources are cached; application changes always rebuild.
    if not (str(source).startswith("lib/") and obj.exists() and obj.stat().st_mtime > source.stat().st_mtime):
        command = [os.environ.get("CXX", "c++"), "-std=c++17"] if source.suffix == ".cpp" else [os.environ.get("CC", "cc")]
        subprocess.run(command + flags + ["-c", str(source), "-o", str(obj)], check=True)
    return str(obj)

with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
    objects = list(pool.map(compile_source, cpp + c))
link_flags = ["-Wl,-dead_strip"] if sys.platform == "darwin" else ["-Wl,--gc-sections"]
subprocess.run([os.environ.get("CXX", "c++")] + flags + objects + link_flags + shlex.split(subprocess.check_output(["pkg-config", "--libs", "sdl2"], text=True)) + ["-o", "build/render_preview"], check=True)
subprocess.run(["build/render_preview"], check=True)

# A compact contact sheet from the real production renderer, without an image
# processing dependency. Individual full-size PPM files remain in build/previews.
files = sorted((ROOT / "build" / "previews").glob("*.ppm"))
chosen = [files[n] for n in [0, 1, 2, 3, 6, 12, 15, 18, 21, 26, 40, 44]]
width, height = 720, 4 * 158
pixels = bytearray([245] * (width * height * 3))
for i, path in enumerate(chosen):
    data = path.read_bytes().split(b"\n", 3)[3]
    x0, y0 = i % 3 * 240, i // 3 * 158
    for y in range(135):
        offset = ((y0 + y) * width + x0) * 3
        pixels[offset:offset + 720] = data[y * 720:(y + 1) * 720]
scan = b"".join(b"\0" + pixels[y * width * 3:(y + 1) * width * 3] for y in range(height))
def chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xffffffff)
(ROOT / "docs" / "UI_PREVIEW.png").write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(scan)) + chunk(b"IEND", b""))
