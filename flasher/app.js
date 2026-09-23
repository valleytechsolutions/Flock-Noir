const release = document.querySelector('#release');
const connection = document.querySelector('#connection');
const button = document.querySelector('#installButton');
try {
  const response = await fetch('firmware-info.json', {cache: 'no-store'});
  if (!response.ok) throw new Error('Firmware metadata unavailable');
  const info = await response.json();
  if (info.board !== 'xiao_esp32s3_sense' || !/^\d+\.\d+\.\d+$/.test(info.version) || !/^[0-9a-f]{64}$/.test(info.sha256)) throw new Error('Invalid release metadata');
  release.textContent = `Flock Noir ${info.version} · ${(info.size / 1048576).toFixed(2)} MiB`;
  document.querySelector('#releaseLink').href = `https://github.com/valleytechsolutions/Flock-Noir/releases/tag/xiao-v${info.version}`;
  if (!window.isSecureContext) {
    connection.textContent = 'Open the HTTPS GitHub Pages address to use USB installation.';
  } else if (!('serial' in navigator)) {
    connection.textContent = 'USB installation needs desktop Chrome or Edge. Safari, Firefox and iPhone browsers do not provide Web Serial.';
  } else {
    connection.textContent = 'Loading USB installer…';
    await import('https://unpkg.com/esp-web-tools@10.4.0/dist/web/install-button.js?module');
    await customElements.whenDefined('esp-web-install-button');
    document.querySelector('#installer').manifest = `manifest-${info.sha256}.json`;
    button.disabled = false;
    connection.textContent = 'Ready. Your browser will ask you to choose a USB serial device.';
  }
} catch (error) {
  connection.textContent = 'Could not load the installer. Check your internet connection, reload, or use the release downloads.';
}
