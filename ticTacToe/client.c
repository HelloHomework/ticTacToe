#include <arpa/inet.h>
#include <ctype.h>
#include <fcntl.h>
#include <limits.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define _GNU_SOURCE

#define BUFSIZE 128


int init(in_port_t *port);
void receiver(void *args);

int init(in_port_t *port) {
  int sockfd = socket(PF_INET, SOCK_STREAM, 0);
  struct sockaddr_in addr;
  addr.sin_family = PF_INET;
  addr.sin_port = htons(*port);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);

  if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
    perror("Connection failed");
    exit(-1);
  }

  return sockfd;
}

void receiver(void *args) {
  char buf[BUFSIZE];
  int server = (intptr_t)args;
  while (1) {
    if (recv(server, buf, BUFSIZE, 0) <= 0) {
      // disconnect by server OR error
      return;
    } else {
      printf("%s", buf);
      memset(buf,0,sizeof(buf));
    }
  }
}

int main() {
  in_port_t port = 4000;
  int sockfd = 0;
  char buf[BUFSIZE];

  pthread_t id;

  sockfd = init(&port);
  if (sockfd != 0) printf("[INFO] Connect Successfully !\n");

  if (pthread_create(&id, NULL, (void *)receiver, (void *)(intptr_t)sockfd) !=
      0)
    perror("failed to start reciever");

  printf("login [username] [password]\n");

  while (1) {
    fgets(buf, BUFSIZE, stdin);

    if (buf[strlen(buf) - 1] == '\n') buf[strlen(buf) - 1] = '\0';

    if (strncasecmp(buf, "exit", 4) == 0) {
      break;
    }

    strncat(buf, ":", BUFSIZE);
    send(sockfd, buf, BUFSIZE, 0);

    if (strncasecmp(buf, "logout", 4) == 0) {
      break;
    }

    memset(buf,0,sizeof(buf));
  }

  close(sockfd);
}