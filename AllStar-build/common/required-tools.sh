#! /bin/bash
# required-tools.sh - Install packages required to install AllStar Debian/RasLink
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
distro=$(lsb_release -is)
apt-get -qq install -y ntpdate g++ make build-essential automake git\
  sudo ca-certificates libusb-dev libnewt-dev libeditline-dev libedit-dev\
  libncurses5-dev libncursesw5-dev bison libssl-dev libasound2-dev\
  php5-cli libiksemel-dev libvorbis-dev\
  curl sox libsox-fmt-all usbutils libsqlite3-dev sqlite3 screen\
  fxload bc alsa-utils dnsutils php5-curl libtool autoconf gawk\
  libical-dev libspandsp-dev libneon27-dev libxml2-dev pkg-config libspeex-dev unixodbc unixodbc-dev uuid uuid-dev libsrtp0-dev dkms\
  libatomic-ops-dev libatomic1 irqbalance libresample-dev libgmime-2.6-dev freetds-dev libbluetooth-dev libcurl4-openssl-dev libghc-postgresql-libpq-dev libgtk2.0-dev libjack-dev liblua5.1-0 liblua5.1-0-dbg\
  liblua5.1-0-dev liblua5.1-bitop-dev liblua5.1-bitop0 liblua5.1-iconv0 liblua5.1-luacsnd liblua5.1-oocairo-dev liblua5.1-oocairo0 liblua5.1-oopango-dev liblua5.1-oopango0 liblua5.1-rrd-dev liblua5.1-rrd0\
  libmysqlclient-dev libopenais-dev libopenh323-dev libpopt-dev libpq-dev libradiusclient-ng-dev libsnmp-dev libspeexdsp-dev libsqlite0-dev libtaoframework-lua5.1-cil libvpb-dev lua5.1-doc\
  lua5.1-policy lua5.1-policy-dev portaudio19-dev uwsgi-plugin-lua5.1
apt-get -qq build-dep dahdi -y
if [[ $distro = "Raspbian" ]]; then
  apt-get -qq install -y i2c-tools libi2c-dev raspberrypi-kernel raspberrypi-kernel-headers
else
  apt-get -qq install -y linux-headers-$(uname -r)
fi
# put the linker where Asterisk expects it
ln -fs /bin/ln /usr/bin
exit 0
