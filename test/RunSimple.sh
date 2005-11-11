#!/bin/bash

echo "LOCAL_TEST_DIR = $LOCAL_TEST_DIR"
echo "LOCAL_TMP_DIR = $LOCAL_TMP_DIR"

cd $LOCAL_TEST_DIR

RC=0
P=$$
PREFIX=results_${USER}${P}
OUTDIR=/tmp/${PREFIX}

mkdir ${OUTDIR}
cp *.cfg ${OUTDIR}
cd ${OUTDIR}

cmsRun --parameter-set StreamOut.cfg > out 2>&1
cmsRun --parameter-set StreamIn.cfg  > in  2>&1

# echo "CHECKSUM = 1" > out
# echo "CHECKSUM = 1" > in

ANS_OUT=`grep CHECKSUM out`
ANS_IN=`grep CHECKSUM in`

if [ "${ANS_OUT}" != "${ANS_IN}" ]
then
    echo "Simple Stream Test Failed"
    RC=1
fi

#rm -rf ${OUTDIR}
exit ${RC}
