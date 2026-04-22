#!/usr/bin/env bash
# Delete old package versions on Cloudsmith, keeping only the N most recent
# per filename. Works around the lack of native retention on the open-source plan.
set -e

if [ -z "$CLOUDSMITH_API_KEY" ]; then
    echo "Warning: CLOUDSMITH_API_KEY not set, skipping cleanup."
    exit 0
fi

# Same repo routing as update-catalog.sh
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
export CLOUDSMITH_REPO="$REPO"
KEEP="${CLEANUP_KEEP:-5}"

echo "Cleaning up $REPO (keeping newest $KEEP versions per filename)..."

python3 - <<PYEOF
import urllib.request, urllib.error, json, os, sys

repo    = os.environ['CLOUDSMITH_REPO']
keep    = int(os.environ.get('CLEANUP_KEEP', '5'))
key     = os.environ['CLOUDSMITH_API_KEY']
dry_run = os.environ.get('DRY_RUN', '0') not in ('0', '', 'false', 'False')

def api(method, path, body=None):
    url = path if path.startswith('http') else f'https://api.cloudsmith.io/v1{path}'
    req = urllib.request.Request(url, method=method)
    req.add_header('X-Api-Key', key)
    if body is not None:
        req.add_header('Content-Type', 'application/json')
        req.data = json.dumps(body).encode()
    return urllib.request.urlopen(req)

def parse_next_link(link_header):
    if not link_header:
        return None
    # RFC 5988: <url>; rel="next", <url>; rel="prev"
    for part in link_header.split(','):
        segs = part.split(';')
        if len(segs) >= 2 and 'rel="next"' in segs[1]:
            return segs[0].strip().strip('<>')
    return None

def build_num(ver):
    try: return int(ver.split('+')[1].split('.')[0])
    except Exception: return 0

# Collect every package, grouped by filename
groups = {}   # filename -> [(build_num, version, slug_perm)]
page = f'/packages/{repo}/?page_size=100'
while page:
    with api('GET', page) as r:
        items = json.load(r)
        link_hdr = r.headers.get('Link', '')
    if isinstance(items, dict):
        items = items.get('results', [])
    for p in items:
        name = p.get('name', '')
        fn   = p.get('filename', '')
        if not name or fn == 'ocpn-plugins.xml' or name == 'ocpn-plugins':
            continue
        groups.setdefault(name, []).append((
            build_num(p.get('version','0')),
            p.get('version',''),
            p.get('slug_perm',''),
        ))
    page = parse_next_link(link_hdr)

deleted = 0
kept    = 0
for name, versions in sorted(groups.items()):
    versions.sort(key=lambda x: x[0], reverse=True)
    keepers = versions[:keep]
    doomed  = versions[keep:]
    kept += len(keepers)
    if not doomed:
        continue
    print(f'{name}: keep {len(keepers)}, delete {len(doomed)}')
    for bn, ver, slug in doomed:
        if dry_run:
            print(f'  - [dry-run] {ver} ({slug})')
            deleted += 1
            continue
        try:
            api('DELETE', f'/packages/{repo}/{slug}/')
            print(f'  - {ver} ({slug})')
            deleted += 1
        except urllib.error.HTTPError as e:
            print(f'  ! failed to delete {ver} ({slug}): {e}', file=sys.stderr)

print(f'\nTotal: kept {kept}, deleted {deleted}')
PYEOF

echo "Cleanup done."
