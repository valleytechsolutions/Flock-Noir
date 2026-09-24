"""Browser regression check with simulated API responses, not hardware results.

Requires Playwright and an installed Chromium browser. Set FLOCKNOIR_BROWSER to
its executable or install Playwright Chromium. Run from the repository root.
"""
import json
import os
import sys
from urllib.parse import parse_qs
from pathlib import Path
from playwright.sync_api import sync_playwright

root = Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root/'pi'))
from flocknoir.alerts import PRESETS, KINDS
settings = dict(enabled=True,max=5,alertIdx=0,
    tones=[dict(name='Custom A',rtttl='A:d=8,o=5,b=160:c,e,g'),dict(name='Custom B',rtttl='B:d=8,o=5,b=160:d,f,a')],
    alertKinds=[dict(id=k,name=n,sound=s) for k,n,s in KINDS],
    soundPresets=[dict(id=k,name=n,rtttl=r) for k,n,r in PRESETS])
status = dict(detected=False, irDet=True, irEn=True, irPresent=True, irFreq=10.0,
              irDuty=.2, irPulseMs=20, irSampleHz=1000, irRaw=230, irBaseline=200,
              irNoise=2, irGaps=0, irClipped=False, irAmp=650, radioNearby=True,
              irWave=[0,0,0,100,100,0]*10, gpsChars=2000, fix=False, sats=0,
              sd=True, sdFree=2000, sdTotal=4000, logged=1, fps=40, wd=True,
              recent=[dict(t='demo',src='<script>alert(1)</script>',lat=None,lon=None,
                           hz=10,duty=.2,conf=.5,evidence='ir_timing_match')])
radio = dict(supported=True, mode='dashboard',channel=1,ble=True,capture=False,
             watch='',target='',packets=180,dropped=0,logErrors=0,wifiReady=True,bleReady=True)
devices = [dict(mac='B4:1E:52:00:00:01',protocol='WiFi',name='<img src=x onerror=alert(1)>',
                category='Flock candidate',method='wildcard_probe',tier=3,alpr=True,rssi=-61,ageMs=120,count=2)]
posts = []
fail_profile = [False]
camera_requests = []
status.update(profile='general',cameraReady=True,fusion=dict(mask=15,method='ir+camera+ble+wifi',
    assessment='multiple_sources_nearby',ageMs=[100,200,150,80],radioTier=3))
radio.update(bleScanning=True,profile='general')
devices.append(dict(mac='10:11:12:01:02:03',protocol='BLE',category='Biscuit candidate',
    name='Biscuit',method='biscuit_name_service',tier=3,alpr=False,rssi=-52,ageMs=100,count=1))
with sync_playwright() as p:
    options = {'headless':True}
    if os.environ.get('FLOCKNOIR_BROWSER'):
        options['executable_path'] = os.environ['FLOCKNOIR_BROWSER']
    browser = p.chromium.launch(**options)
    page = browser.new_page(viewport={'width':1100,'height':1000})
    def fits_viewport():
        overflow=page.evaluate("""[...document.querySelectorAll('body *')]
          .filter(e=>e.getBoundingClientRect().right>innerWidth+1 && e.getBoundingClientRect().width)
          .map(e=>[e.tagName,e.id,e.className,e.getBoundingClientRect().right]).slice(0,20)""")
        assert page.evaluate('document.documentElement.scrollWidth <= innerWidth+1'), overflow
    errors = []
    page.on('pageerror',lambda error: errors.append(str(error)))
    def route(request):
        path = request.request.url.split('flocknoir.test')[-1]
        if request.request.method == 'POST':
            posts.append(request.request.post_data)
            if path=='/api/profile':
                if fail_profile[0]:return request.fulfill(status=500,json={'ok':False})
                status['profile']=parse_qs(request.request.post_data)['profile'][0]
                radio['profile']=status['profile']
                if status.get('exclusiveModes'):
                    mode=status['profile'];status.update(opticalActive=mode=='alpr',irPaused=mode!='alpr',wd=mode=='wardrive',irDet=False,detected=False,radioAlert=None,alprAlert=None)
                    radio.update(wifiScanning=mode!='axon',bleWindowMs=90 if mode=='axon' else 100,bleIntervalMs=100 if mode=='axon' else 200)
            if path=='/api/settings':
                fields=parse_qs(request.request.post_data,keep_blank_values=True)
                for k in settings['alertKinds']:
                    k['sound']=fields['sound_'+k['id']][0]
                settings['tones']=[dict(name=fields['nm'+str(i)][0],rtttl=fields['rt'+str(i)][0])
                                   for i in range(int(fields['count'][0]))]
            return request.fulfill(json={'ok':True})
        if path == '/':
            return request.fulfill(content_type='text/html',body=(root/'web/index.html').read_text())
        if path == '/logo.png':
            return request.fulfill(content_type='image/png',body=(root/'web/logo.png').read_bytes())
        if path.startswith('/api/frame.jpg'):
            camera_requests.append(path)
            if len(camera_requests)==1:
                return request.fulfill(status=503,body='No recent frame')
            return request.fulfill(content_type='image/jpeg',body=(root/'tests/fixtures/jpeg/420.jpg').read_bytes())
        fixtures={'/api/status':status,'/api/radio':radio,'/api/radio/devices':devices,
                  '/api/radio/files':[], '/api/recs':[], '/api/settings':settings}
        return request.fulfill(json=fixtures.get(path,{}))
    page.route('**/*',route)
    page.goto('http://flocknoir.test/')
    if os.environ.get('FLOCKNOIR_TEST_FONT'):
        page.add_style_tag(content=':root{--mono:"Courier New",monospace}')
    page.wait_for_function("document.querySelector('#bannerTxt').textContent.includes('IR + RADIO NEARBY')")
    assert page.locator('.tab').count() == 6
    assert 'by Valleytech' not in page.locator('body').inner_text()
    assert 'Your Pal Kal' in page.locator('footer').inner_text()
    assert page.locator('#rows script').count() == 0
    page.wait_for_function("document.querySelector('#detectorRadioRows').textContent.includes('B4:1E:52')")
    assert page.locator('#detectorRadioRows img').count() == 0
    assert 'Biscuit' not in page.locator('#detectorRadioRows').inner_text()
    assert 'ir+camera+ble+wifi' in page.locator('#fusionSummary').inner_text()
    page.locator('#alprProfile').click()
    page.wait_for_function("document.querySelector('#profileState').textContent.startsWith('ALPR focus active')")
    assert any(body=='profile=alpr' for body in posts)
    status.update(irDet=False, radioEvents=3,
                  radioAlert=dict(category='Axon candidate',method='company_or_service',tier=2,alpr=False))
    page.evaluate('tick()')
    page.wait_for_function("document.querySelector('#generalBannerTxt').textContent.includes('AXON CANDIDATE')")
    assert page.locator('#count').text_content() == '4'
    for width in (1100,390):
        page.set_viewport_size({'width':width,'height':1000})
        fits_viewport()
    status.update(irDet=True, radioAlert=None)
    page.locator('[data-tab=scanner]').click()
    page.wait_for_function("document.querySelector('#radioRows').textContent.includes('B4:1E:52')")
    assert page.locator('#radioRows img').count() == 0
    assert 'Biscuit' in page.locator('#radioRows').inner_text()
    page.locator('#deviceFilter').select_option('biscuit')
    page.wait_for_function("!document.querySelector('#radioRows').textContent.includes('B4:1E:52')")
    assert 'Biscuit' in page.locator('#radioRows').inner_text()
    page.locator('#deviceFilter').select_option('all')
    page.locator('#generalProfile').click()
    page.wait_for_function("document.querySelector('#generalProfileState').textContent.startsWith('General alerts active')")
    page.locator('#radioTarget').fill('AA:BB:CC:DD:EE:FF')
    page.locator('#radioSave').click()
    page.wait_for_function("document.querySelector('#toast').textContent==='Radio settings saved'")
    assert any('target=AA%3ABB%3ACC%3ADD%3AEE%3AFF' in body for body in posts)
    for width in (1100,390):
        page.set_viewport_size({'width':width,'height':1000})
        overflow = page.evaluate("""[...document.querySelectorAll('body *')]
            .filter(e=>e.getBoundingClientRect().right>innerWidth+1 && e.getBoundingClientRect().width)
            .map(e=>[e.tagName,e.id,e.className,e.getBoundingClientRect().right]).slice(0,20)""")
        assert page.evaluate('document.documentElement.scrollWidth <= innerWidth+1'), (width,overflow)
    page.set_viewport_size({'width':1100,'height':1000})
    # Synthetic preview, stored outside the repo unless explicitly requested.
    if os.environ.get('FLOCKNOIR_SCREENSHOT'):
        devices[0]['name'] = 'Demo beacon (simulated)'
        page.evaluate('pollRadio()')
        page.wait_for_function("document.querySelector('#radioRows').textContent.includes('Demo beacon')")
        page.evaluate("document.querySelector('#toast').classList.remove('show')")
        page.screenshot(path=os.environ['FLOCKNOIR_SCREENSHOT'],full_page=True)
    # Exclusive XIAO modes: tabs start their scanner, view/settings tabs do not.
    status.update(version='0.6.0',exclusiveModes=True,profile='alpr',opticalActive=True,irPaused=False)
    radio.update(exclusiveModes=True,profile='alpr',wifiScanning=True,bleWindowMs=100,bleIntervalMs=200,axonReminderSeconds=10)
    page.reload()
    page.wait_for_function("document.querySelector('#activeMode').textContent==='ALPR ACTIVE'")
    page.locator('[data-tab=pig]').click()
    page.wait_for_function("document.querySelector('#activeMode').textContent==='PIG DETECTOR ACTIVE'")
    assert posts[-1]=='profile=axon'
    page.wait_for_function("document.querySelector('#pigHealth').textContent.includes('90/100')")
    assert 'Paused by scan mode' in page.locator('#readyIr').text_content()
    status['radioAlert']=dict(category='Axon candidate',method='axon_service',tier=2,alpr=False,rssi=-45,ageMs=100)
    devices.append(dict(mac='00:25:DF:01:02:03',protocol='BLE',name='Axon',category='Axon candidate',method='axon_service',tier=2,alpr=False,rssi=-45,ageMs=100,count=2))
    page.evaluate('tick()');page.evaluate('pollRadio()')
    page.wait_for_function("document.querySelector('#pigBannerTxt').textContent.includes('POSSIBLE AXON BODY CAMERA NEARBY')")
    assert 'Biscuit' not in page.locator('#pigRows').inner_text()
    assert 'B4:1E' not in page.locator('#pigRows').inner_text()
    if os.environ.get('FLOCKNOIR_PIG_SCREENSHOT'):
        page.set_viewport_size({'width':1100,'height':1000})
        page.evaluate("document.querySelector('#toast').classList.remove('show')")
        page.screenshot(path=os.environ['FLOCKNOIR_PIG_SCREENSHOT'],full_page=True)
    status['radioAlert']['ageMs']=4000;page.evaluate('tick()')
    page.wait_for_function("document.querySelector('#pigBannerTxt').textContent.includes('NO RECENT MATCH')")
    page.locator('#axonRepeat').fill('15');page.locator('#axonSave').click()
    page.wait_for_function("document.querySelector('#toast').textContent==='Axon reminder saved'")
    assert posts[-1]=='repeatSeconds=15'
    for width in (1100,390):
        page.set_viewport_size({'width':width,'height':1000});fits_viewport()
    profile_count=sum(body.startswith('profile=') for body in posts)
    page.locator('[data-tab=camera]').click()
    assert page.locator('#recBtn').is_disabled()
    page.locator('[data-tab=settings]').click()
    assert sum(body.startswith('profile=') for body in posts)==profile_count
    page.locator('[data-tab=wardrive]').click()
    page.wait_for_function("document.querySelector('#activeMode').textContent==='WARDRIVE ACTIVE'")
    assert posts[-1]=='profile=wardrive'
    assert status['wd'] and status['irPaused']
    assert page.locator('#wdEn').is_checked()
    page.locator('#fieldCoverage').click()
    page.wait_for_function("document.querySelector('#toast').textContent.includes('Hotspot off shortly')")
    assert posts[-1]=='mode=field'
    fail_profile[0]=True
    page.locator('[data-tab=detector]').click()
    page.wait_for_function("document.querySelector('#toast').textContent.includes('Mode change failed')")
    assert page.locator('#wardrive').is_visible() and status['profile']=='wardrive'
    fail_profile[0]=False
    page.locator('[data-tab=detector]').click()
    page.wait_for_function("document.querySelector('#activeMode').textContent==='ALPR ACTIVE'")
    assert not status['wd'] and not status['irPaused']
    # Restore legacy fixtures for the existing hardware capability fallback.
    status.update(exclusiveModes=False,irDet=True,radioNearby=True)
    radio['exclusiveModes']=False
    radio['supported'] = False
    page.reload()
    page.wait_for_function("document.querySelector('#bannerTxt').textContent.includes('IR + RADIO NEARBY')")
    assert page.locator('#radioCard').is_hidden()
    page.locator('[data-tab=camera]').click()
    page.locator('#camBtn').click()
    page.wait_for_function("document.querySelector('#cam').naturalWidth===64")
    assert len(camera_requests)>=2  # recover while the first camera frame is pending
    assert page.locator('#cam').evaluate('e=>getComputedStyle(e).imageRendering') == 'auto'
    page.locator('#camBtn').click()
    assert page.locator('#cam').get_attribute('src') is None
    page.locator('[data-tab=settings]').click()
    page.wait_for_selector('#sound_axon')
    assert page.locator('#sound_alpr_ir').input_value()=='retro'
    assert page.locator('#sound_axon').input_value()=='siren'
    assert page.locator('#sound_meta').input_value()=='confused'
    page.locator('#sound_axon').select_option('slot:1')
    page.locator('#sound_meta').select_option('silent')
    page.locator('.sound-test[data-kind=axon]').click()
    page.wait_for_function("document.querySelector('#toast').textContent==='Playing preview'")
    assert any(parse_qs(body).get('rtttl')==['B:d=8,o=5,b=160:d,f,a'] for body in posts)
    page.locator('#tones .del').first.click()
    assert page.locator('#sound_axon').input_value()=='slot:0'
    page.locator('#save').click()
    page.wait_for_function("document.querySelector('#toast').textContent==='Saved to device'")
    page.locator('[data-tab=detector]').click()
    page.locator('[data-tab=settings]').click()
    page.wait_for_selector('#sound_axon')
    assert page.locator('#sound_axon').input_value()=='slot:0'
    assert page.locator('#sound_meta').input_value()=='silent'
    for width in (1100,390):
        page.set_viewport_size({'width':width,'height':1000})
        fits_viewport()
    radio.update(supported=True, hardware='pi', modeHint='Pi: field mode hops the dedicated monitor adapter. The dashboard hotspot stays on.',
                 detail='Set RADIO_MONITOR_IFACE to a dedicated USB WiFi interface')
    page.reload()
    page.locator('[data-tab=scanner]').click()
    page.wait_for_function("document.querySelector('#radioModeHint').textContent.includes('hotspot stays on')")
    assert 'BOOT' not in page.locator('#radioModeHint').text_content()
    page.locator('#radioMode').select_option('field')
    page.locator('#radioSave').click()
    page.wait_for_function("document.querySelector('#toast').textContent.includes('dashboard stays on')")
    page.set_viewport_size({'width':390,'height':1000})
    assert page.evaluate('document.documentElement.scrollWidth <= innerWidth+1')
    assert not errors, errors
    browser.close()
print('UI desktop/mobile, safe rendering, settings POST and Pi capability checks passed')
