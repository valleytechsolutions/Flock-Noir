"""Package the factory image after a successful PlatformIO build."""
from pathlib import Path
import hashlib
import shutil

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
