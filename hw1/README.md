# Instruction to Start Server

Design doc location: https://docs.google.com/document/d/1TXFtoXJwDS0cmcqsKqL0H-cUqX7ZJrGzrEKIrFhsiCY/edit?usp=sharing

Go to the directory: `hw1`, execute one of the follwoing:

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
