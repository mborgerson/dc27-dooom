#!/bin/bash
set -e
set -x

./nxdk/tools/xbedump/xbe bin/default.xbe -ds | grep -i "address" > addrs.txt
python build_gdbinit.py > .gdbinit
