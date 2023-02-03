# Instruction to Start Server

Design doc location: https://docs.google.com/document/d/1TXFtoXJwDS0cmcqsKqL0H-cUqX7ZJrGzrEKIrFhsiCY/edit?usp=sharing

Go to the directory: `hw2`, execute one of the follwoing:

```
make
vodserver [port] # defaults to 8345 if empty
```


```
make clean
make run [port] # defaults to 8345 if empty
```

Then, navigate to:

```
http://localhost:8345/          # will display hello world
http://localhost:8345/video/video.webm     # will play a webm video
```


<br />

# Docker

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
docker exec -it hw2 sh container-start-client.sh
```


<br />


# How To Check If File Copy Is Identical?
If two checksums are the same, then they are identical. (file names can be different but have the same checksum)
```
openssl sha512 [filepath]       
```
Example:
```
openssl sha512 sample4_recv.ogg
openssl sha512 sample4.ogg
```