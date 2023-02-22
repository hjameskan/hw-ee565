#ifndef SOCKET_UTILS_
#define SOCKET_UTILS_

#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>

void sigchld_handler(int s);
void *get_in_addr(struct sockaddr *sa);

#endif