#!/usr/bin/env bash
# Generate and upload ocpn-plugins.xml to Cloudsmith from all uploaded XML metadata files.
# Run after platform builds so the catalog stays current.
set -e

if [ -z "$CLOUDSMITH_API_KEY" ]; then
    echo "Warning: CLOUDSMITH_API_KEY not set, skipping catalog update."
    exit 0
fi

REPO="${CLOUDSMITH_REPO:-pa2wlt/1tracker-alpha}"
CATALOG_FILE="/tmp/ocpn-plugins.xml"

echo "Fetching XML metadata files from Cloudsmith repo: $REPO"

python3 - <<'PYEOF'
import urllib.request, json, re, datetime, os, sys

repo = os.environ.get('CLOUDSMITH_REPO', 'pa2wlt/1tracker-alpha')
api  = f"https://api.cloudsmith.io/v1/packages/{repo}/?page_size=200"

with urllib.request.urlopen(api) as r:
    packages = json.load(r)

items = packages if isinstance(packages, list) else packages.get('results', [])

seen = set()
entries = []
for p in items:
    fn  = p.get('filename', '')
    url = p.get('cdn_url', '')
    if not fn.endswith('.xml') or fn in seen:
        continue
    seen.add(fn)
    try:
        with urllib.request.urlopen(url) as r:
            content = r.read().decode('utf-8')
        content = re.sub(r'<\?xml[^?]*\?>', '', content).strip()
        content = re.sub(r'<!--.*?-->', '', content, flags=re.DOTALL).strip()
        if '<plugin' in content:
            entries.append(content)
            print(f"  + {fn}")
    except Exception as e:
        print(f"  ! {fn}: {e}", file=sys.stderr)

today = datetime.date.today().isoformat()
catalog = f'''<?xml version="1.0" encoding="UTF-8"?>
<plugins>
  <version>1</version>
  <date>{today}</date>
{"".join(chr(10) + e for e in entries)}
</plugins>
'''

with open('/tmp/ocpn-plugins.xml', 'w') as f:
    f.write(catalog)

print(f"Catalog written: {len(entries)} plugins")
PYEOF

echo "Uploading ocpn-plugins.xml to $REPO..."
cloudsmith push raw --no-wait-for-sync \
    --name "ocpn-plugins" \
    --version "latest" \
    --summary "OpenCPN plugin catalog for 1tracker_pi" \
    --republish \
    "$REPO" "$CATALOG_FILE"

echo "Catalog uploaded."
