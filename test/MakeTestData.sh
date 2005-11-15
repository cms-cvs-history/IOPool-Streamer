#!/bin/bash

echo "LOCAL_TEST_DIR = $LOCAL_TEST_DIR"
echo "LOCAL_TMP_DIR = $LOCAL_TMP_DIR"
echo "LOCAL_TOP_DIR = $LOCAL_TOP_DIR"
echo "LOCAL_TEST_BIN = $LOCAL_TEST_BIN"

cd $LOCAL_TMP_DIR

FILE=${LOCAL_TEST_DIR}/MakeTestData.cfg

sed 's/#R1//g' ${FILE} > ${FILE}_R1
sed 's/#R2//g' ${FILE} > ${FILE}_R2
sed 's/#R3//g' ${FILE} > ${FILE}_R3

echo "Generating test data"
cmsRun --parameter-set ${FILE}_R1 || exit 1
cmsRun --parameter-set ${FILE}_R2 || exit 1
cmsRun --parameter-set ${FILE}_R3 || exit 1
echo "Data written to $LOCAL_TMP_DIR"

${LOCAL_TEST_BIN}/RunFileReader_t ${FILE}_R1 ${FILE}_R2 ${FILE}_R3

