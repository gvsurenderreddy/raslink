#! /bin/bash
# update-dahdi.sh - Used to build Dahdi for AllStar

# Script Start
echo "Downloading and unpacking dahdi..."
cd /usr/src/utils/
if [ -e /usr/src/utils/astsrc/dahdi* ]
then
  rm -rf /usr/src/utils/astsrc/dahdi*
fi
wget http://downloads.asterisk.org/pub/telephony/dahdi-linux-complete/dahdi-linux-complete-current.tar.gz &>/dev/null
cd /usr/src/utils/astsrc/
tar zxvf /usr/src/utils/dahdi-linux-complete-current.tar.gz &>/dev/null
mv dahdi* dahdi
rm -rf /usr/src/utils/*.tar.gz
echo "Done"
cd /usr/src/utils/astsrc/dahdi/
echo "Building dahdi..."
# Patch dahdi for use with AllStar
# https://allstarlink.org/dude-dahdi-2.10.0.1-patches-20150306
patch -p1 < /usr/src/utils/AllStar-build/patches/patch-dahdi-dude-current
# Remove setting the owner to asterisk
patch -p0 < /usr/src/utils/AllStar-build/patches/patch-dahdi.rules
# Add proper timer
patch -p1 < /usr/src/utils/AllStar-build/patches/dahdi-dynamic-multispan-fifo.patch
# Build and install dahdi
(make all;make install;make config)
if [ "$(grep -ic "dahdi" /etc/modules)" == "1" ]; then
  sed -i '/dahdi/d' /etc/modules
fi
if [ -f /etc/init.d/dahdi ]; then
  update-rc.d dahdi remove
  rm -rf /etc/init.d/dahdi
  systemctl disable dahdi.timer
  rm -rf /etc/systemd/system/dahdi.timer
  systemctl daemon-reload
fi
echo "Done"
exit 0
