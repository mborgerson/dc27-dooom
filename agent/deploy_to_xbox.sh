#!/bin/bash
set -e
set -x

ftp -n -v 192.168.1.2 <<END
quote USER xbox
quote PASS xbox
binary
cd /E/apps/nxdk/
delete default.xbe
put bin/default.xbe default.xbe
quit
END
