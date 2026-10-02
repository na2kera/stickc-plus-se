"""Collect an already built image; never uploads or accesses a serial port."""
import hashlib
import os
from pathlib import Path
import shutil
import sys

root = Path(__file__).resolve().parent.parent
env = sys.argv[1] if len(sys.argv) > 1 else "m5stick-c-se"
build = root / ".pio" / "build" / env
target = root / "artifacts" / env
core = Path(os.environ.get("PLATFORMIO_CORE_DIR", str(Path.home() / ".platformio")))
framework = core / "packages" / "framework-arduinoespressif32@3.20017.241212+sha.dcc1105b"
boot_app = framework / "tools" / "partitions" / "boot_app0.bin"
if not boot_app.exists():
    # PlatformIO may use the unsuffixed package name when no other version exists.
    framework = core / "packages" / "framework-arduinoespressif32"
    boot_app = framework / "tools" / "partitions" / "boot_app0.bin"
sources = [build / name for name in ["firmware.bin", "bootloader.bin", "partitions.bin"]] + [boot_app]
for source in sources:
    if not source.is_file():
        raise SystemExit(f"Missing {source}; build {env} first")
target.mkdir(parents=True, exist_ok=True)
manifest = []
for source in sources:
    destination = target / source.name
    shutil.copyfile(source, destination)
    manifest.append(f"{hashlib.sha256(destination.read_bytes()).hexdigest()}  {destination.name}")
(target / "SHA256SUMS").write_text("\n".join(manifest) + "\n")
print(f"Packaged {env}: " + ", ".join(f"{p.name}={p.stat().st_size} bytes" for p in sources))
