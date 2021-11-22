#!/bin/bash
clang -std=c99 -Wall -pedantic *.c
mkdir pa2
cp ./*.c ./pa2
cp ./*.h ./pa2
cp ./libruntime.so ./pa2
tar -czvf pa2.tar.gz pa2
rm -rf ./pa2
rm a.out
