#!/bin/sh
set -eu
base=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
if ! command -v python3 >/dev/null 2>&1; then
    printf '%s\n' 'Python 3 is required. See README-FIRST.txt.' >&2
    exit 1
fi
exec python3 "$base/start-ge6.py" "$@"
