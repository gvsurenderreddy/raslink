#!/bin/bash
# patch-asterisk.sh - Used to add customizations to Asterisk

# Script Start

cd ../../astsrc/asterisk/
echo "Patching Asterisk files..."
# Add the notch option
cp ../extras/notch/rpt_notch.c ./apps
sed -i 's/\/\* #include "rpt_notch.c" \*\//#include "rpt_notch.c"/' ./apps/app_rpt.c
# Add mdc1200 support
cp ../extras/mdc1200/*.c ./apps
cp ../extras/mdc1200/*.h ./apps
sed -i 's/\/\* #include "mdc_decode.c" \*\//#include "mdc_decode.c"/' ./apps/app_rpt.c
sed -i 's/\/\* #include "mdc_encode.c" \*\//#include "mdc_encode.c"/' ./apps/app_rpt.c
# Update app_rpt version information
sed -i '/ \*  version/c\ \*  version 17.07 07\/10\/2017' ./apps/app_rpt.c
sed -i '/static  char \*tdesc \= "Radio Repeater \/ Remote Base  version/c\static  char \*tdesc \= "Radio Repeater \/ Remote Base  version 17.07 07\/10\/2017";' ./apps/app_rpt.c
# Change TX enabled message
sed -i 's/"RPTENA"/"TXENA"/' ./apps/app_rpt.c
# Make EchoLink call signs use normal characters
sed -i 's/return(sayphoneticstr/return\(saycharstr/' ./apps/app_rpt.c
sed -i 's/res \= sayphoneticstr/res \= saycharstr/' ./apps/app_rpt.c
# Prevent reading false DTMF digits
sed -i 's/26.0 : 42.0/32.0 : 42.0/' ./main/dsp.c
# Increase queue size in usbradio driver
sed -i 's/define	QUEUE_SIZE	2/define	QUEUE_SIZE	10/' ./channels/chan_usbradio.c
# Make sure asterisk uses pthread
sed -i 's/ASTCFLAGS+=-Wno-unused-result/ASTCFLAGS\+\=-lpthread -Wno-unused-result/' ./Makefile
echo "Done"
exit 0
