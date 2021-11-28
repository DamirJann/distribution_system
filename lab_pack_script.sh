#!/bin/bash
clang -std=c99 -Wall -pedantic *.c
mkdir pa3
cp ./*.c ./pa3
cp ./*.h ./pa3
cp ./libruntime.so ./pa3
tar -czvf pa3.tar.gz pa3
rm -rf ./pa3
