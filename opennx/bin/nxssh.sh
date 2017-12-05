#!/bin/sh

export LD_LIBRARY_PATH="/srv/pv/debug/nx/nxcompp:${LD_LIBRARY_PATH}"

exec /usr/bin/nxssh "$@"
