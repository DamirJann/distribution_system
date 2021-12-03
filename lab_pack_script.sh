#!/bin/bash
clang -std=c99 -Wall -pedantic *.c
mkdir pa5
cp ./*.c ./pa5
cp ./*.h ./pa5
cp ./libruntime.so ./pa5
tar -czvf pa5.tar.gz pa5
rm -rf ./pa5
