#!/bin/bash
# update-stage1.sh - Update the system
# Stage One
#    Copyright (C) 2017  Jeremy Lincicome (W0JRL)
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
echo "Starting system update.
This will take a while.
You can continue using your node during this process."
sleep 1
# Get new sources
echo "Updating source files for All Star..."
cd /usr/src/utils/
git pull
sleep 0.5
echo "Done"
# Update the system
echo "Updating system software..."
(apt-get -qq update;apt-get dist-upgrade -y)
sleep 0.5
echo "Done"
# Clean the package database
echo "Cleaning up unneeded software..."
(apt-get -qq autoremove --purge -y;apt-get -qq clean;apt-get -qq autoclean)
sleep 0.5
echo "Done"
# Setup for stage two
cd /root
mv .bashrc .bashrc.orig
cat .bashrc.orig > .bashrc
echo "/usr/src/utils/AllStar-build/debian/update-stage2.sh" >> .bashrc
echo "Rebooting to finish install"
echo "When your node reboots, you need to log in
to finish the update."
sync
sudo reboot
exit 0
