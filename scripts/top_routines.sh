#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/.." && pwd)
toolkit_root=$(CDPATH= cd -- "$repo_root/../../Sites/mymetrics" && pwd)
config_path="$toolkit_root/data/repos/1tracker_pi/config.json"

python3 "$toolkit_root/top_routines.py" \
  --repo-root "$repo_root" \
  --config "$config_path" \
  "$@"
