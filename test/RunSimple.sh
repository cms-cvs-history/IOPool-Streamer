#!/bin/bash

echo "LOCAL_TEST_DIR = $LOCAL_TEST_DIR"
echo "LOCAL_TMP_DIR = $LOCAL_TMP_DIR"

cd $LOCAL_TEST_DIR

RC=0
P=$$
PREFIX=results_${USER}${P}
OUTDIR=${LOCAL_TMP_DIR}/${PREFIX}

mkdir ${OUTDIR}
cp *.cfg ${OUTDIR}
cd ${OUTDIR}

cmsRun --parameter-set StreamOut.cfg > out 2>&1
cmsRun --parameter-set StreamIn.cfg  > in  2>&1
cmsRun --parameter-set StreamCopy.cfg  > copy  2>&1

# echo "CHECKSUM = 1" > out
# echo "CHECKSUM = 1" > in

ANS_OUT=`grep CHECKSUM out`
ANS_IN=`grep CHECKSUM in`
ANS_COPY=`grep CHECKSUM copy`

if [ "${ANS_OUT}" != "${ANS_IN}" ]
then
    echo "Simple Stream Test Failed (out!=in)"
    RC=1
fi

if [ "${ANS_OUT}" != "${ANS_COPY}" ]
then
    echo "Simple Stream Test Failed (copy!=out)"
    RC=1
fi

#rm -rf ${OUTDIR}
exit ${RC}
