#!/usr/bin/env python3
"""Stage ESP32 PlatformIO outputs for ESP Web Tools; don't modify firmware."""
from pathlib import Path
import shutil

root = Path(__file__).resolve().parent.parent
build = root / '.pio/build/esp32dev'
target = root / 'web-installer'
files = ['firmware.bin', 'bootloader.bin', 'partitions.bin', 'boot_app0.bin']
for f in files:
    source = build / f
    if not source.is_file():
        if f == 'boot_app0.bin':
            from_framework = Path.home() / '.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin'
            if from_framework.exists():
                source = from_framework
    if not source.is_file() and f == 'boot_app0.bin':
        alternate = Path.home() / '.platformio/packages/framework-arduinoespressif32/tools/boot_app0.bin'
        if alternate.exists():
            source = alternate
    if not source.is_file():
        raise SystemExit(f'Missing required build artifact: {f}')
    shutil.copy2(source, target / f)
    print('Published', f)
