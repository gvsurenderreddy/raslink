#!/bin/bash
# update-stage2.sh - Update the system
# Stage Two
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
# Make sure system-update runs in screen
if [ -z "$STY" ]; then exec screen -S system-update /bin/bash "$0"; fi
status() {
    $@
    if [ $? -ne 0 ]; then
        return 1
        exit 1
    else
        return 0
    fi
}
echo "Running update, stage two."
echo "This will take a while.
System-update is running in a screen session.
If your session disconnects during the update,
after reconnecting, run
'screen -dr'
to reconnect to the update screen."
sleep 3
service asterisk stop &>/dev/null
echo "You cannot use your node during this process.
It has been disabled."
sleep 1
# Restore bashrc
mv /root/.bashrc.orig /root/.bashrc
# Check for release upgrade
chmod +x /usr/src/utils/AllStar-build/common/release-upgrade.sh
status /usr/src/utils/AllStar-build/common/release-upgrade.sh
# Check and update repository URL
chmod +x /usr/src/utils/AllStar-build/common/remote-fetch.sh
/usr/src/utils/AllStar-build/common/remote-fetch.sh
# Make sure version runs at login
if [[ "$(grep -ic "/usr/bin/version" /root/.bashrc)" = "1" ]]; then
  sed -i '/\/usr\/bin\/version/d' /root/.bashrc
fi
if [[ "$(grep -ic "/usr/bin/version" /root/.profile)" = "0" ]]; then
  echo "/usr/bin/version" >> /root/.profile
fi
chmod +x /usr/src/utils/AllStar-build/debian/chk-packages.sh
status /usr/src/utils/AllStar-build/debian/chk-packages.sh
sleep 0.5
chmod +x /usr/src/utils/AllStar-build/common/update-dahdi.sh
status /usr/src/utils/AllStar-build/common/update-dahdi.sh
sleep 0.5
chmod +x /usr/src/utils/AllStar-build/common/update-libpri.sh
status /usr/src/utils/AllStar-build/common/update-libpri.sh
sleep 0.5
chmod +x /usr/src/utils/AllStar-build/common/update-asterisk.sh
status /usr/src/utils/AllStar-build/common/update-asterisk.sh
sleep 0.5
chmod +x /usr/src/utils/AllStar-build/common/update-uridiag.sh
status /usr/src/utils/AllStar-build/common/update-uridiag.sh
sleep 0.5
# Make sure configuration files and scripts are loaded
echo "Updating start up scripts..."
(cp /usr/src/utils/AllStar-build/common/rc.updatenodelist /usr/local/bin/rc.updatenodelist;chmod +x /usr/local/bin/rc.updatenodelist)
(cp /usr/src/utils/AllStar-build/common/rc.nodenames /usr/local/bin/rc.nodenames;chmod +x /usr/local/bin/rc.nodenames)
chmod +x /usr/src/utils/AllStar-build/debian/make-links.sh
/usr/src/utils/AllStar-build/debian/make-links.sh
cp -a /usr/src/utils/astsrc/sounds/* /var/lib/asterisk/sounds
gsmcount=`ls -1 /var/lib/asterisk/sounds/rpt/*.gsm 2>/dev/null | wc -l`
if [ "$gsmcount" != "0" ]; then
  rm -f /var/lib/asterisk/sounds/rpt/*.gsm
fi
echo "Done"
sleep 0.5
echo "Cleaning up object files..."
cd /usr/src/utils
(git clean -f;git checkout -f;rm -f 1) &>/dev/null
echo "Done"
sleep 0.5
echo "Updating system boot configuration..."
cp /usr/src/utils/AllStar-build/common/asterisk.service /etc/systemd/system
cp /usr/src/utils/AllStar-build/common/asterisk.timer /etc/systemd/system
cp /usr/src/utils/AllStar-build/common/updatenodelist.service /etc/systemd/system
cp /usr/src/utils/AllStar-build/common/nodenames.service /etc/systemd/system
systemctl daemon-reload
systemctl enable asterisk.timer &>/dev/null
systemctl enable updatenodelist.service &>/dev/null
systemctl enable avahi-daemon &>/dev/null
if [ "$(systemctl status avahi-daemon | grep -ic 'dead')" = "1" ]; then
  systemctl start avahi-daemon
fi
if [ ! -e /root/.nonames ]; then
  systemctl enable nodenames.service &>/dev/null
fi
if [[ "$(grep -ic "snd_pcm_oss" /etc/modules)" > "1" ]]; then
  sed -i '/snd_pcm_oss/d' /etc/modules
  echo "snd_pcm_oss" >> /etc/modules
fi
echo "Done"
sleep 0.5
echo "The update is complete."
echo "You can run this tool at any time by typing 'system-update' at a root prompt."
echo "Re-enabling your node..."
sync
sudo service asterisk start
echo "Done"
date > /root/.lastupdate

