#! /bin/bash
# update-libpri.sh - Patch and build Libpri for All Star
#    Copyright (C) 2018  Jeremy Lincicome (W0JRL)
#    https://jlappliedtechnologies.com  admin@jlappliedtechnologies.com

#    This program is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.

#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.

#    You should have received a copy of the GNU General Public License
#    along with this program.  If not, see <http://www.gnu.org/licenses/>.

# Script Start
status() {
    $@
    if [ $? -ne 0 ]; then
        echo "***Libpri was not removed properly***
Please see <https://jlappliedtechnologies.com/raslink/> for assistance."
        sleep 5
        exit 1
    fi
}

echo "Looking for Libpri..."
cd /usr/src/utils/astsrc/libpri/
# Patch libpri for use with AllStar
#status patch </usr/src/utils/AllStar-build/patches/patch-libpri-makefile
# Build and install libpri
#status make all
if [ -e /usr/lib/libpri.so ]; then
  echo "Removing Libpri..."
  status make uninstall &>/dev/null
  echo "Done"
else
  echo "Libpri isn't installed; Skipping."
fi
exit 0
