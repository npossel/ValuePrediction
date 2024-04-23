#!/bin/sh
echo $1 $2

cd $1
python3 "$2"
cd ../..
