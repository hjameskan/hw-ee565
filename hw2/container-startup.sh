#!/bin/sh

pwd
ls -la
# ls -la
# echo '\n'
# echo 'Hi! I am a shell script!\n'
# echo 'I am running in a container\n'

# cd src/udp
# make clean-server
# make run-server
make clean
make run
# ./vodserver 8346

pwd && ls -la
