"""Package the factory image after a successful PlatformIO build."""
from pathlib import Path
import hashlib
import shutil
import json
import re

root = Path(__file__).resolve().parents[1]
source = root / '.pio/build/xiao_esp32s3_sense/firmware.factory.bin'
dest = root / 'binaries/FlockNoir-merged-0x0.bin'
if not source.is_file():
    raise SystemExit('Build first: python -m platformio run -e xiao_esp32s3_sense')
dest.parent.mkdir(exist_ok=True)
shutil.copyfile(source, dest)
digest = hashlib.sha256(dest.read_bytes()).hexdigest()
dest.with_suffix('.bin.sha256').write_text(f'{digest}  {dest.name}\n', encoding='ascii')
print(f'{dest.name}: {dest.stat().st_size} bytes, SHA256 {digest}')

version = re.search(r'FLOCK_NOIR_VERSION\s+"([^\"]+)"', (root/'firmware/FlockNoir/config.h').read_text())[1]
(dest.parent/'firmware-info.json').write_text(json.dumps(dict(version=version,
    board='xiao_esp32s3_sense',chipFamily='ESP32-S3',size=dest.stat().st_size,sha256=digest),indent=2)+'\n',encoding='utf-8')
