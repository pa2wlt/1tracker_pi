#!/usr/bin/env bash
# Generate and upload ocpn-plugins.xml to Cloudsmith from all uploaded XML metadata files.
# Run after platform builds so the catalog stays current.
set -e

if [ -z "$CLOUDSMITH_API_KEY" ]; then
    echo "Warning: CLOUDSMITH_API_KEY not set, skipping catalog update."
    exit 0
fi

# Pick target repo by git tag (matches Shipdriver's Metadata.cmake routing):
# tag containing "beta" or "rc" -> beta repo, other tags -> prod, no tag -> alpha
if [ -n "$CLOUDSMITH_REPO" ]; then
    REPO="$CLOUDSMITH_REPO"
elif [ -n "$CIRCLE_TAG" ]; then
    lc_tag="$(echo "$CIRCLE_TAG" | tr '[:upper:]' '[:lower:]')"
    case "$lc_tag" in
        *beta*|*rc*) REPO="pa2wlt/1tracker-beta" ;;
        *)           REPO="pa2wlt/1tracker-prod" ;;
    esac
else
    REPO="pa2wlt/1tracker-alpha"
fi
# Export so the python heredoc below and the cloudsmith CLI see the same repo.
export CLOUDSMITH_REPO="$REPO"
CATALOG_FILE="/tmp/ocpn-plugins.xml"

echo "Fetching XML metadata files from Cloudsmith repo: $REPO"

python3 - <<'PYEOF'
import urllib.request, json, re, datetime, os, sys

repo = os.environ.get('CLOUDSMITH_REPO', 'pa2wlt/1tracker-alpha')

def build_num(ver):
    """Extract the build number after '+' for version comparison."""
    try:
        return int(ver.split('+')[1].split('.')[0])
    except Exception:
        return 0

# Paginate through all packages, keeping the highest-build-number XML per filename.
# Cloudsmith returns results as a top-level JSON array and advertises the next page
# via an RFC 5988 Link header (rel="next") — not via a body field. Follow that
# header until it is absent.
best = {}   # filename -> (version, cdn_url)
page_url = f"https://api.cloudsmith.io/v1/packages/{repo}/?page_size=100&q=filename:.xml"

while page_url:
    with urllib.request.urlopen(page_url) as r:
        data = json.load(r)
        link_hdr = r.headers.get('Link', '')
    items = data if isinstance(data, list) else data.get('results', [])
    for p in items:
        fn  = p.get('filename', '')
        url = p.get('cdn_url', '')
        ver = p.get('version', '0')
        if not fn.endswith('.xml') or fn == 'ocpn-plugins.xml':
            continue
        if fn not in best or build_num(ver) > build_num(best[fn][0]):
            best[fn] = (ver, url)
    m = re.search(r'<([^>]+)>;\s*rel="next"', link_hdr)
    page_url = m.group(1) if m else None

entries = []
for fn, (ver, url) in sorted(best.items()):
    try:
        with urllib.request.urlopen(url) as r:
            content = r.read().decode('utf-8')
        content = re.sub(r'<\?xml[^?]*\?>', '', content).strip()
        content = re.sub(r'<!--.*?-->', '', content, flags=re.DOTALL).strip()
        if '<plugin' in content:
            entries.append(content)
            print(f"  + {fn} ({ver})")
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

VERSION="$(date -u +%Y.%m.%d)"
echo "Uploading ocpn-plugins.xml to $REPO (version $VERSION)..."
cloudsmith push raw --no-wait-for-sync \
    --name "ocpn-plugins" \
    --version "$VERSION" \
    --summary "OpenCPN plugin catalog for 1tracker_pi" \
    --republish \
    "$REPO" "$CATALOG_FILE"

echo "Catalog uploaded."
