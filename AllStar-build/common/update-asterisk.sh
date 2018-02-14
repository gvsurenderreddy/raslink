#! /bin/bash
# update-asterisk.sh - Build asterisk for AllStar
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
        echo "Asterisk failed to install.
Please see <https://jlappliedtechnologies.com/raslink/> for assistance."
        return 1
        exit 1
    else
        return 0
    fi
}
echo "Building Asterisk..."
# Remove modules
status rm -f /usr/lib/asterisk/modules/*
cd /usr/src/utils/astsrc/asterisk/
# Configure the build process
status export PTLIB_CONFIG=/usr/share/ptlib/make/ptlib-config
status ./configure CXX=g++-4.8 CC=gcc-4.8
# Optimize for the arm cpu if running on the Raspberry Pi
distro=$(lsb_release -is)
if [[ $distro = "Raspbian" ]]; then
  sed -i '/PROC\=/c\PROC\=arm' ./makeopts.in
fi
# Build and install Asterisk
status make
status make install
if [ -e /etc/init.d/asterisk ]; then
  (update-rc.d asterisk remove;rm /etc/init.d/asterisk)
fi
# Fix comment in rpt.conf
sed -i 's/Say phonetic call sign/Say call sign/' /etc/asterisk/rpt* | sed -i 's/say phonetic call sign/Say call sign/' /etc/asterisk/rpt*
# Load res_crypto module
sed -i '/res_crypto.so/c\load \=> res_crypto.so ;   Cryptographic Digital Signatures                  ' /etc/asterisk/modules.conf
# Load app_sendtext module
sed -i '/app_sendtext.so/c\load \=> app_sendtext.so ;   Send Text Applications                            ' /etc/asterisk/modules.conf
# Add low pass and high pass filter configuration to usbradio
filters=$(grep -ic 'rxlpf' /etc/asterisk/usbradio.conf)
if [[ $filters = "0" ]]; then
  echo "rxlpf = 0     ; Receiver Audio Low Pass Filter 0,1,2
      ; 0 - 3.0 kHz cutoff (Default) value for reduced noise and increased intelligibility. (default)
      ; 1 - 3.3 kHz cutoff for increased high end, sibilance and brightness.
      ; 2 - 3.5 kHz cutoff for even more high end, sibilance and brightness.
rxhpf = 0     ; Receiver Audio High Pass Filter 0,1
      ; 0 - 300 Hz cutoff. (Default) value to reduce sub-audible signals for retransmission and in the receiver speaker. (default)
      ; 1 - 250 Hz cutoff for additional received and retransmitted bass response.
      ; recommend using this filter with a CTCSS tone no higher than 186.2 Hz.

txlpf = 0     ; Transmitter Audio Low Pass Filter 0,1
      ; 0 - 3.0 kHz cutoff. (Default)
      ; 1 - 3.3 kHz cutoff for increased high end, sibilance and brightness.

txhpf = 0     ; Transmitter Audio High Pass Filter 0,1,2
      ; 0 - 300 Hz cutoff Reduce interference between voice and sub-audible signaling tones and codes. (default)
      ; 1 - 250 Hz cutoff Increase bass response in transmitted audio.
      ; 2 - 120 Hz cutoff for special applications requiring additional bass response in transmitted audio.
      ; Not recommended due to the increased possibility of voice energy interfering with sub-audible signaling." >> /etc/asterisk/usbradio.conf
fi
# set jbmaxsize in usbradio
sed -i 's/jbmaxsize \= 500/jbmaxsize \= 250/' /etc/asterisk/usbradio*
sed -i 's/jbmaxsize \= 200/jbmaxsize \= 250/' /etc/asterisk/usbradio*
echo "Done."
exit 0
