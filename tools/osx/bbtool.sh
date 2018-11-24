#!/bin/sh
#  bbtool.sh
#  
#  patch BASIC program into BlueBasic firmare image
#  the firmware image needs to be for 16K basic program size fitting in the 148K binary
#
#  Created by Kai Scheffer on 15-Nov-2018
#  Copyright © 2018 Kai Scheffer, Switzerland.

basic=$1
firmware=$2
output=$3
pid=$4

usage()
{
echo ""
echo "Patch BASIC program into firmware image for CC2541 device."
echo "Requires 'BlueBasic' interpreter in the path for processing the BASIC program."
echo "Requires 'crc16' in the path to reconstruct the required check sum."
echo "The firmware image needs to be of type B wit 148K size and 16K program space inside." 
echo "(C) 15. November 2018 Kai Scheffer" 
echo ""
echo "usage: ${0} basic_program.bb firmware_image_in.bin firmware_image_out.bin <xx>"
echo "xx: optional 2 digit hex value for PID"
}

if [[ ${basic} == "" ]]; then
echo "Missing BASIC input file ${basic}."
usage
exit 2
fi

if [[ ${firmware} == "" ]]; then
echo "Missing firmware input file ${firmware}."
usage
exit 2
fi

if [[ ${output} == "" ]]; then
echo "Missing firmware output file ${output}."
usage
exit 2
fi


# translate BlueBasic program into binary token form
#  argument 1 BlueBasic progam in text form
#  argument 2 firmware image to be patched
#  argument 3 output firmware file
# the required BlueBasic simulator needs to be in the path
translate_basic()
{
#flash file needs to be cleared to avoid scattering of data
if [[ ((-e ${TDIR}/basic.bin)) ]]; then
rm -rf ${TDIR}/basic.bin
fi
#BlueBasic ${PLATFORM_ID}/basic.bin < ${1}
#reverse feeding the program accelerates the heap sort inside the interpreter :)
tail -r ${1} | BlueBasic ${TDIR}/basic.bin
BlueBasic ${TDIR}/basic.bin <<< "AUTORUN ON"

# split image in the first 128K and the last 4K
dd if=${2} bs=1024 count=128 of=${TDIR}/head.bin 2>/dev/null
dd if=${2} bs=1024 count=4 skip=144 of=${TDIR}/tail.bin 2>/dev/null
cat ${TDIR}/head.bin ${TDIR}/basic.bin ${TDIR}/tail.bin >${3}
rm ${TDIR}/head.bin
rm ${TDIR}/basic.bin
rm ${TDIR}/tail.bin
}

# patch Product ID in binary image
# argument 1 is the binary file
# argument 2 is the PID
patch_pid()
{
echo "Patching Product ID x${2} in binary image ${1}"
perl -0777 -pi -e "s/\x01\x0D\x00\x00.\x10\x01/\x01\x0D\x00\x00\x${2}\x10\x01/sg" ${1}
#crc16 ${1} ${1}
}

# exit with given number
fini()
{
rm -rf ${TDIR}
exit ${1}
}

echo "Patching BlueBasic program '${basic}' into firmware image '${firmware}' for TI CC2541"

TDIR=$(mktemp -d /tmp/bbtool.XXXXXXXXX)
echo "temporary folder: $TDIR"

patched=${TDIR}/patched.bin

translate_basic ${basic} ${firmware} ${patched}

if [[ ${pid} != "" ]]; then
tstpid=$(echo "${pid}" | grep -e '[0-9a-fA-F]\{2\}')
if [[ ${pid} == ${tstpid} ]]; then
patch_pid ${patched} "${pid}"
else
echo "Error: PID ${pid} not a 2 digit hex number."
fini 2
fi
fi

crc16 ${patched} ${output}
fini 0
