# UDP Design Summary #

[Design Summary Click Here To Onedrive (UW Login Necessary)](https://uwnetid-my.sharepoint.com/:w:/g/personal/kanjames_uw_edu/Ec88pMNxz4RClakobswYV0kBYc_c_hz-0qI7j1K3hhNDkg?e=5zsq7P)

<br/>

# Testing Instructions #
## *** You need to start the HTTP Server and Peer Server first! ***
Go to this url to add video (Do not start the "path" with a "/"):
```
http://localhost:8345/peer/add?path=video/sample4.ogg&host=localhost&port=8347&rate=1600000
```
Play video at this url:
```
http://localhost:8345/peer/view/video/sample4.ogg
```



<br />

# Instruction to Start HTTP Server #

Go to the directory: `hw2`, execute one of the follwoing:

```
make
./vodserver [port1] [port2] # defaults to 8345 and 8346 respectively if empty
```


```
make clean
make run [port1] [port2] # defaults to 8345, 8346 if empty
```

Then, navigate to:

```
http://localhost:8345/          # will display hello world
http://localhost:8345/video/video.webm     # will play a webm video
```
<br />

# Test Video Streaming From Peer Server #
In another terminal, execute one of the following:

```
make
./peerserver [port3] # defaults to 8347 if empty
```

```
make clear-server
make run-server [port3] # defaults to 8347 if empty
```

<br />


# Docker #

Start `in one terminal window`, and do the following sequentially.

Initial Image build:
```
sh docker-init.sh
```

Start UDP server:
```
docker-compose up
```

Start UDP client:

In `new terminal window`, type:
```
docker exec -it hw2 sh container-start-peer.sh
```


<br />

# How To Check If File Copy Is Identical? #
If two checksums are the same, then they are identical. (file names can be different but have the same checksum)
```
openssl sha512 [filepath]       
```
Example:
```
openssl sha512 sample4_recv.ogg
openssl sha512 sample4.ogg
```
<br />


# Test UDP Locally (Removed) #

## Use two terminal windows: ##

In terminal 1 (This is the server)
```
make clean-server && make run-server
```
In termianl 2 (This is the client)
```
make clean-client && make run-client
```
<br />
