#!/bin/sh
echo "----\nHP\n----"
./test_jpeg ../lj/lj-x86_64.so $1 $2 $3 /dev/null
echo "----\nmine\n----"
./test_jpeg ./lj.so $1 $2 $3 /dev/null
