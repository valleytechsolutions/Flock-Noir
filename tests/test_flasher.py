"""Check release integrity and flash ranges without attaching a serial port."""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('build_flasher', ROOT/'tools/build_flasher.py')
builder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(builder)


class FlasherTests(unittest.TestCase):
    def test_release_layout_and_stale_image_rejection(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for folder in ('binaries', 'flasher', 'web', 'firmware/FlockNoir'):
                (root/folder).mkdir(parents=True)
            (root/'flasher/index.html').write_text('test')
            (root/'web/logo.png').write_bytes(b'logo')
            (root/'firmware/FlockNoir/config.h').write_text('#define FLOCK_NOIR_VERSION "0.5.0"')
            data = bytearray((i % 256 for i in range(0x12000)))
            data[0] = data[0x10000] = 0xe9
            digest = hashlib.sha256(data).hexdigest()
            binary = root/'binaries/FlockNoir-merged-0x0.bin'
            binary.write_bytes(data)
            binary.with_suffix('.bin.sha256').write_text(digest+'  '+binary.name)
            info = dict(version='0.5.0',board='xiao_esp32s3_sense',size=len(data),sha256=digest)
            metadata = root/'binaries/firmware-info.json'
            metadata.write_text(json.dumps(info))
            with patch.object(builder, 'ROOT', root):
                builder.stage(root/'site')
                manifest = json.loads((root/'site/manifest.json').read_text(encoding='utf-8'))
                self.assertEqual(manifest,json.loads((root/'site'/('manifest-'+digest+'.json')).read_text(encoding='utf-8')))
                self.assertTrue(all(digest in p['path'] for p in manifest['builds'][0]['parts']))
                self.assertTrue(manifest['new_install_prompt_erase'])
                self.assertEqual(manifest['new_install_improv_wait_time'],0)
                self.assertEqual(manifest['builds'][0]['chipFamily'],'ESP32-S3')
                for part in manifest['builds'][0]['parts']:
                    chunk = (root/'site'/part['path']).read_bytes()
                    start, end = part['offset'], part['offset']+len(chunk)
                    self.assertTrue(end <= 0x9000 or start >= 0xe000, 'NVS would be overwritten')
                    self.assertEqual(chunk, data[start:end])
                info['version'] = '0.4.4'
                metadata.write_text(json.dumps(info))
                with self.assertRaisesRegex(ValueError, 'Stale'):
                    builder.stage(root/'site')
                info['version'] = '0.5.0'
                metadata.write_text(json.dumps(info))
                binary.write_bytes(bytes(data)+b'corrupt')
                with self.assertRaisesRegex(ValueError, 'Stale'):
                    builder.stage(root/'site')


if __name__ == '__main__':
    unittest.main()
