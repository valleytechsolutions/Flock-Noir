"""Stage the HTTPS flasher from the exact checked-in, hash-verified release.

Split around NVS so 'keep settings' really preserves it. No image is fetched
from an unversioned release URL at install time; a Pages deployment is atomic.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil

ROOT = Path(__file__).resolve().parents[1]


def stage(destination):
    binary = ROOT / 'binaries/FlockNoir-merged-0x0.bin'
    data = binary.read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    expected = binary.with_suffix('.bin.sha256').read_text().split()[0]
    info = json.loads((binary.parent / 'firmware-info.json').read_text())
    version = re.search(r'FLOCK_NOIR_VERSION\s+"([^"]+)"',
                        (ROOT / 'firmware/FlockNoir/config.h').read_text())[1]
    if digest != expected or info['sha256'] != digest or info['version'] != version:
        raise ValueError('Stale or damaged firmware: rebuild and run tools/package_firmware.py')
    if info['board'] != 'xiao_esp32s3_sense' or info['size'] != len(data):
        raise ValueError('Incorrect board or image size')
    if not 65536 < len(data) <= 0x610000 or data[0] != 0xe9 or data[65536] != 0xe9:
        raise ValueError('Invalid factory image layout')
    destination = Path(destination)
    destination.mkdir(parents=True, exist_ok=True)
    for source in (ROOT / 'flasher').iterdir():
        if source.is_file():
            shutil.copyfile(source, destination / source.name)
    shutil.copyfile(ROOT / 'web/logo.png', destination / 'logo.png')
    parts, checksums = [], []
    for name, offset, end in [('bootloader', 0, 0x8000), ('partitions', 0x8000, 0x9000),
                              ('boot_app0', 0xe000, 0x10000), ('firmware', 0x10000, len(data))]:
        filename = name + '.bin'
        part = data[offset:end]
        (destination / filename).write_bytes(part)
        parts.append(dict(path=filename, offset=offset))
        checksums.append(hashlib.sha256(part).hexdigest() + '  ' + filename)
    manifest = dict(name='Flock Noir — XIAO ESP32-S3 Sense', version=version,
                    new_install_prompt_erase=True, new_install_improv_wait_time=0,
                    builds=[dict(chipFamily='ESP32-S3', parts=parts)])
    (destination / 'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n', encoding='utf-8')
    (destination / 'firmware-info.json').write_text(json.dumps(info, indent=2)+'\n', encoding='utf-8')
    (destination / 'SHA256SUMS').write_text('\n'.join(checksums)+'\n', encoding='ascii')
    (destination / '.nojekyll').touch()
    print(f'Staged XIAO {version} in {destination}; NVS excluded from update parts')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', default='.test-build/flasher')
    stage(parser.parse_args().out)
