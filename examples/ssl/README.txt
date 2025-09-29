Place your TLS key/certificate PEM files here (NOT committed).

Expected filenames (configurable in future API):
  device.key.pem   - Private key (PEM, unencrypted)
  device.cert.pem  - Certificate (leaf or fullchain)
  ca_chain.pem     - Optional separate chain (intermediates)

Example (self-signed):
  openssl req -x509 -newkey rsa:2048 -nodes -days 365 \
    -keyout device.key.pem \
    -out device.cert.pem \
    -subj "/CN=esp32.local/O=MyOrg/C=DE"
