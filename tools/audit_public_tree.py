"""Read-only public-export audit. Never connects to the phone."""
from pathlib import Path
import hashlib
import json
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
DENIED = {'.apk', '.aab', '.img', '.dat', '.xz', '.zip', '.tar', '.so', '.o', '.bin', '.ko', '.jks', '.keystore', '.pk8', '.pem', '.p12', '.der', '.db', '.log'}
PATTERNS = [r'-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----', r'gh[pousr]_[A-Za-z0-9]{30,}', r'github_pat_[A-Za-z0-9_]{30,}']
failures = []
for p in ROOT.rglob('*'):
    if not p.is_file() or '.git' in p.relative_to(ROOT).parts:
        continue
    if p.suffix.lower() in DENIED:
        failures.append('Forbidden artifact: ' + p.relative_to(ROOT).as_posix())
    data = p.read_bytes()
    if b'\x00' in data:
        failures.append('Binary content: ' + p.relative_to(ROOT).as_posix())
    text = data.decode('utf-8', 'replace')
    for pattern in PATTERNS:
        if re.search(pattern, text):
            failures.append('Potential credential in: ' + p.relative_to(ROOT).as_posix())
manifest = json.loads((ROOT / 'SOURCE_MANIFEST.json').read_text(encoding='utf-8'))
for name, expected in manifest['files'].items():
    p = ROOT / name
    if not p.is_file() or hashlib.sha256(p.read_bytes()).hexdigest() != expected:
        failures.append('Manifest mismatch: ' + name)
if failures:
    print('\n'.join(failures))
    sys.exit(1)
print('PASS: source manifest and public-tree audit')
