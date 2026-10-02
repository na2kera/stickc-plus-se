"""Verify the exact vendored source snapshot recorded during implementation."""
import hashlib
from pathlib import Path
root = Path(__file__).resolve().parent.parent
manifest = root / "docs" / "DEPENDENCIES.sha256"
count = 0
for line in manifest.read_text().splitlines():
    expected, name = line.split("  ", 1)
    path = root / name
    if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != expected:
        raise SystemExit(f"Dependency changed/missing: {name}")
    count += 1
print(f"PASS: {count} vendored files match the fixed snapshot")
