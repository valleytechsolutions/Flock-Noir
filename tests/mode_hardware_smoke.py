"""Opt-in XIAO mode isolation check. Leaves ALPR + dashboard active, preserving sensor/tone settings."""
import argparse
import json
import time
import serial


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', required=True)
    args = parser.parse_args()
    results = []
    with serial.Serial(args.port, 115200, timeout=.2, write_timeout=2) as port:
        def command(value, key):
            port.reset_input_buffer()
            port.write(('CMD:'+value+'\n').encode())
            limit = time.monotonic()+8
            while time.monotonic()<limit:
                line = port.readline().decode(errors='replace').strip()
                if any(fault in line for fault in ('Guru Meditation','Backtrace:','Rebooting')):
                    raise RuntimeError(line)
                try:
                    data = json.loads(line)
                except ValueError:
                    continue
                if isinstance(data,dict) and key in data:
                    return data
            raise RuntimeError('No response to '+value)
        time.sleep(3)
        version=command('VERSION','version')['version']
        assert version=='0.6.0', version
        settings=command('SETTINGS','alertKinds')
        before=command('HEALTH','irEn')
        try:
            for mode in ('general','axon','wardrive','alpr'):
                assert command('PROFILE:'+mode,'ok')['ok']
                # Exercise WiFi-off Axon and its transition back to a WiFi scanner.
                if mode=='axon':assert command('COVERAGE:field','ok')['ok']
                if mode=='alpr':assert command('COVERAGE:dashboard','ok')['ok']
                time.sleep(4)
                start=command('HEALTH','irSampleHz')
                radio=command('STATUS','bleReady')
                time.sleep(2)
                end=command('HEALTH','irSampleHz')
                assert end['profile']==mode and radio['profile']==mode
                assert end['wd']==(mode=='wardrive')
                assert end['irPaused']==(mode!='alpr')
                assert end['irEn']==before['irEn'], 'Saved OPT101 enable changed'
                assert end['sd'] and radio['bleReady'] and radio['bleScanning']
                assert radio['wifiScanning']==(mode!='axon')
                assert end['gpsGood']>start['gpsGood'], 'GPS sentences stopped'
                assert end['gpsFail']==start['gpsFail'], 'GPS checksums failed'
                assert end['cameraDecodeErrors']==0
                if mode=='alpr':
                    assert end['fps']>=20 and 800<=end['irSampleHz']<=1200, end
                else:
                    assert end['analysisFrames']==start['analysisFrames']
                    assert end['irSampleHz']==0
                if mode in ('axon','wardrive'):
                    assert end['logged']==start['logged'], 'ALPR log changed in isolated mode'
                if mode=='axon':
                    assert radio['mode']=='field' and radio['bleWindowMs']==100
                results.append(dict(mode=mode,analysisFps=end['fps'],adcHz=end['irSampleHz'],
                    wifiScan=radio['wifiScanning'],bleWindowMs=radio['bleWindowMs'],
                    bleIntervalMs=radio['bleIntervalMs'],gpsSentences=end['gpsGood']-start['gpsGood'],
                    sd=end['sd'],heap=radio['freeHeap']))
            assert command('SETTINGS','alertKinds')==settings, 'Tone settings changed'
        finally:
            command('COVERAGE:dashboard','ok')
            command('PROFILE:alpr','ok')
    print(json.dumps(dict(version=version,modes=results,settingsPreserved=True),indent=2))


if __name__=='__main__':
    main()
