# Instruction to Start Server

First time running: go to the directory: `hw1`, execute:

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
http://localhost:8080/          # will display hello world
http://localhost:8080/video     # will play a webm video
```