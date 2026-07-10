#!/bin/bash -x

# Coppied from flashFs.sh

size=$(<in/size.txt tr -d '\n')
offset=$(<in/offset.txt tr -d '\n')

../gotchi/stageRom.sh

rm -rf out/fs.bin
./in/mklittlefs --all-files -c littlefs -s $size out/fs.bin
