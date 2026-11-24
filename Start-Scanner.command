#!/bin/sh
set -eu
cd "$(dirname "$0")"
if [ ! -x ./qr-scanner ]; then
    make
fi
exec ./qr-scanner "$@"
