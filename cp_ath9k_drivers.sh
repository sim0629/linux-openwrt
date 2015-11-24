#!/bin/bash

DEST_DIR=ath9kdrivers
mkdir -p $DEST_DIR
cp --preserve=all build_output/drivers/net/wireless/ath/ath9k/ath9k.ko \
	build_output/drivers/net/wireless/ath/ath9k/ath9k_common.ko \
	build_output/drivers/net/wireless/ath/ath9k/ath9k_hw.ko \
	$DEST_DIR/
if [ 0 == $? ]; then
	echo "file copied. Check the directory: $DEST_DIR"
else
	echo "error. cannot copy files"
fi
