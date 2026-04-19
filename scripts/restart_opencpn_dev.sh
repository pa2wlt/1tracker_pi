#!/bin/sh
set -eu

osascript <<'APPLESCRIPT'
if application "OpenCPN" is running then
  tell application "OpenCPN" to activate
  delay 0.5

  tell application "System Events"
    if exists process "OpenCPN" then
      tell process "OpenCPN"
        try
          if exists (first button of front window whose name is "Close") then
            click (first button of front window whose name is "Close")
          end if
        on error
          try
            if exists (first button of front window whose name is "Cancel") then
              click (first button of front window whose name is "Cancel")
            end if
          on error
            key code 53
            delay 1
            key code 53
          end try
        end try
      end tell
    end if
  end tell

  tell application "OpenCPN" to quit
end if
APPLESCRIPT

attempt=0
while pgrep -x OpenCPN >/dev/null 2>&1; do
  attempt=$((attempt + 1))
  if [ "$attempt" -ge 6 ]; then
    echo "OpenCPN did not quit cleanly; aborting restart." >&2
    exit 1
  fi
  sleep 1
done

open -a OpenCPN
