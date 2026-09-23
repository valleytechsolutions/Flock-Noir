"""Browser checks against a staged or live flasher; never requests USB access."""
import argparse
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
import os
from pathlib import Path
import threading
from playwright.sync_api import sync_playwright


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--url', help='Live HTTPS site; otherwise serve .test-build/flasher')
    args = parser.parse_args()
    server = None
    if not args.url:
        class Quiet(SimpleHTTPRequestHandler):
            def log_message(self, *_):
                pass
        server = ThreadingHTTPServer(('127.0.0.1',0),partial(Quiet,directory=str(Path('.test-build/flasher').resolve())))
        threading.Thread(target=server.serve_forever,daemon=True).start()
    url = args.url or f'http://127.0.0.1:{server.server_port}/'
    try:
        with sync_playwright() as p:
            options = {'headless':True}
            if os.environ.get('FLOCKNOIR_BROWSER'):
                options['executable_path'] = os.environ['FLOCKNOIR_BROWSER']
            browser = p.chromium.launch(**options)
            page = browser.new_page()
            errors = []
            page.on('pageerror',lambda e:errors.append(str(e)))
            page.goto(url)
            page.wait_for_function("document.querySelector('#connection').textContent.startsWith('Ready.')",timeout=45000)
            assert page.locator('#installButton').is_enabled()
            assert page.evaluate("!!customElements.get('esp-web-install-button')")
            info = page.request.get(url+'firmware-info.json').json()
            assert info['version'] in page.locator('#release').inner_text()
            assert page.locator('#installer').evaluate('(e)=>e.manifest') == 'manifest-'+info['sha256']+'.json'
            for width in (1100,390):
                page.set_viewport_size({'width':width,'height':950})
                assert page.evaluate('document.documentElement.scrollWidth <= innerWidth+1')
            if os.environ.get('FLOCKNOIR_FLASHER_SCREENSHOT'):
                page.set_viewport_size({'width':1100,'height':950})
                page.screenshot(path=os.environ['FLOCKNOIR_FLASHER_SCREENSHOT'],full_page=True)
            # Same page must explain unsupported browsers without an active install button.
            unsupported = browser.new_page()
            unsupported.add_init_script('delete Navigator.prototype.serial;')
            unsupported.goto(url)
            unsupported.wait_for_function("document.querySelector('#connection').textContent.includes('needs desktop Chrome')")
            assert unsupported.locator('#installButton').is_disabled()
            # A failed release fetch cannot leave an active installer behind.
            failed = browser.new_page()
            failed.route('**/firmware-info.json',lambda r:r.fulfill(status=503,body='unavailable'))
            failed.goto(url)
            failed.wait_for_function("document.querySelector('#connection').textContent.includes('Could not load')")
            assert failed.locator('#installButton').is_disabled()
            assert not errors, errors
            browser.close()
    finally:
        if server:
            server.shutdown()
            server.server_close()
    print('Flasher: real ESP Web Tools module loaded; mobile layout, unsupported browser and fetch failure checked. USB flash not exercised.')


if __name__ == '__main__':
    main()
