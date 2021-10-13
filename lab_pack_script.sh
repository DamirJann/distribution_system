#!/bin/bash
clang -std=c99 -Wall -pedantic *.c
mkdir pa1
cp ./*.c ./pa1
cp ./*.h ./pa1
tar -czvf pa1.tar.gz pa1
rm -rf ./pa1
rm a.out