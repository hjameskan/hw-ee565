#!/bin/sh

pwd
ls -la
echo '\n'
echo 'Hi! I am a shell script!\n'
echo 'I am running in a container\n'

cd src/udp
make clean-server
make run-server
pwd && ls -la
