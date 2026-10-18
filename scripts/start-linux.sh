#!/bin/sh
set -eu
BASE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
if ! command -v python3 >/dev/null 2>&1; then
    echo 'Python 3 is required. See README-LINUX.txt.' >&2
    exit 1
fi
exec python3 "$BASE/share/fs15-reconstruction/tester/linux-tester.py" "$@"
