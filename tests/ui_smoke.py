"""Browser regression check with simulated API responses, not hardware results.

Requires Playwright and an installed Chromium browser. Set FLOCKNOIR_BROWSER to
its executable or install Playwright Chromium. Run from the repository root.
"""
import json
import os
from pathlib import Path
from playwright.sync_api import sync_playwright

root = Path(__file__).resolve().parents[1]
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
                category='Flock candidate',method='wildcard_probe',tier=3,rssi=-61,ageMs=120,count=2)]
posts = []
camera_requests = []
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
                  '/api/radio/files':[], '/api/recs':[]}
        return request.fulfill(json=fixtures.get(path,{}))
    page.route('**/*',route)
    page.goto('http://flocknoir.test/')
    if os.environ.get('FLOCKNOIR_TEST_FONT'):
        page.add_style_tag(content=':root{--mono:"Courier New",monospace}')
    page.wait_for_function("document.querySelector('#bannerTxt').textContent.includes('IR + RADIO NEARBY')")
    assert page.locator('.tab').count() == 4
    assert page.locator('#rows script').count() == 0
    page.wait_for_function("document.querySelector('#detectorRadioRows').textContent.includes('B4:1E:52')")
    assert page.locator('#detectorRadioRows img').count() == 0
    status.update(irDet=False, radioEvents=3,
                  radioAlert=dict(category='Axon candidate',method='company_or_service',tier=2,alpr=False))
    page.evaluate('tick()')
    page.wait_for_function("document.querySelector('#bannerTxt').textContent.includes('AXON CANDIDATE')")
    assert page.locator('#count').text_content() == '4'
    for width in (1100,390):
        page.set_viewport_size({'width':width,'height':1000})
        fits_viewport()
    status.update(irDet=True, radioAlert=None)
    page.locator('[data-tab=wardrive]').click()
    page.wait_for_function("document.querySelector('#radioRows').textContent.includes('B4:1E:52')")
    assert page.locator('#radioRows img').count() == 0
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
    radio.update(supported=True, hardware='pi', modeHint='Pi: field mode hops the dedicated monitor adapter. The dashboard hotspot stays on.',
                 detail='Set RADIO_MONITOR_IFACE to a dedicated USB WiFi interface')
    page.reload()
    page.locator('[data-tab=wardrive]').click()
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
