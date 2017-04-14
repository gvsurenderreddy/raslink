#! /bin/bash
# Patch and build Libpri for All Star

# Script Start
echo "Building libpri..."
cd /usr/src/utils/astsrc/libpri/
# Patch libpri for use with AllStar
patch </usr/src/utils/AllStar-build/patches/patch-libpri-makefile
# Build and install libpri
(make;make install)
cp /usr/src/utils/AllStar-build/common/dahdi.service /etc/systemd/system
if [ -f /etc/init.d/dahdi ]; then
  update-rc.d dahdi remove
  rm -rf /etc/init.d/dahdi
fi
systemctl daemon-reload
systemctl start dahdi.service
echo "Done"
exit 0
