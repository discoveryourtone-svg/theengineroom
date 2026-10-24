#!/usr/bin/env bash
# Runs a build command; if it fails, posts the key error lines as GitHub
# annotations so they show on the run's summary page (no log digging needed).
#   scripts/ci_run.sh "<title>" <command> [args...]
title="$1"; shift
log="$(mktemp)"
set -o pipefail
"$@" 2>&1 | tee "$log"
code=$?
[ $code -eq 0 ] && exit 0

enc() { sed -e 's/%/%25/g' -e 's/\r//g' | awk 'BEGIN{ORS="%0A"} {print}'; }

errs="$(grep -E "error|Error|ERROR|fatal|FAILED|failed|No such file|not found|cannot" "$log" \
        | grep -viE "warning|0 error|-Werror" | head -60)"
[ -n "$errs" ] || errs="$(tail -40 "$log")"

i=0
while IFS= read -r -d '' chunk && [ $i -lt 6 ]; do
  i=$((i + 1))
  echo "::error title=$title (errors $i)::$(printf '%s' "$chunk" | enc)"
done < <(printf '%s\n' "$errs" | awk 'NR%15==1 && NR>1 {printf "\0"} {print}' ; printf '\0')

echo "::error title=$title (last lines)::$(tail -25 "$log" | enc)"
exit $code
