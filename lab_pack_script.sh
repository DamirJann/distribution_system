#!/bin/bash
clang -std=c99 -Wall -pedantic *.c
mkdir pa4
cp ./*.c ./pa4
cp ./*.h ./pa4
cp ./libruntime.so ./pa4
tar -czvf pa4.tar.gz pa4
rm -rf ./pa4
