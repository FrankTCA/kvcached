#include <arpa/inet.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "ds.h"
#include "print_functions.h"
#ifndef CLIENT_H
#define CLIENT_H
#define SA struct sockaddr
#define BUFSIZ 1024

#define LOCALIP "127.0.0.1"


char* send_data_i(int port, int pipe, const bool shouldReturn, int packets) {
  int sockfd, connfd;
  struct sockaddr_in serv_addr, cli_addr;

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd == -1) {
    printf("Socket failed and data could not be sent!\n");
    exit(1);
  }
  bzero(&serv_addr, sizeof(serv_addr));

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr(LOCALIP);
  serv_addr.sin_port = htons(port);
  if (connect(sockfd, (SA*)&serv_addr, sizeof(serv_addr)) == -1) {
    printf("Could not connect to server!\n");
    exit(1);
  }

  printf("Connected.\n");
  char buffer[BUFSIZ];
  for (int i = 0; i < packets; i++) {
    bzero(buffer, BUFSIZ);
    int re = read(pipe, buffer, BUFSIZ);
    if (re == 0) {
      break;
    }
    send(sockfd, buffer, BUFSIZ, 0);
  }
  bzero(buffer, BUFSIZ);
  if (shouldReturn == true) {
    read(sockfd, buffer, BUFSIZ);
    close(sockfd);
    return strdup(buffer);
  }
  close(sockfd);
  return NULL;
}


char* send_data(int port, char* data, const bool shouldReturn) {
  int sockfd, connfd;
  struct sockaddr_in serv_addr, cli_addr;

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd == -1) {
    printf("Socket failed and data could not be sent!\n");
    exit(1);
  }
  bzero(&serv_addr, sizeof(serv_addr));

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr(LOCALIP);
  serv_addr.sin_port = htons(port);
  if (connect(sockfd, (SA*)&serv_addr, sizeof(serv_addr)) == -1) {
    printf("Could not connect to server!\n");
    exit(1);
  }

  char buffer[BUFSIZ];
  bzero(buffer, BUFSIZ);
  strcpy(buffer, data);
  send(sockfd, buffer, BUFSIZ, 0);
  bzero(buffer, BUFSIZ);
  if (shouldReturn == true) {
    read(sockfd, buffer, BUFSIZ);
    close(sockfd);
    return strdup(buffer);
  }
  close(sockfd);
  return NULL;
}

int test_running(int port) {
  int sockfd, connfd;
  struct sockaddr_in serv_addr, cli_addr;

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd == -1) {
    return 0;
  }
  bzero(&serv_addr, sizeof(serv_addr));

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr(LOCALIP);
  serv_addr.sin_port = htons(port);
  if (connect(sockfd, (SA*)&serv_addr, sizeof(serv_addr)) == -1) {
    return 0;
  }

  char buffer[BUFSIZ];
  bzero(buffer, BUFSIZ);
  strcpy(buffer, "P");
  send(sockfd, buffer, BUFSIZ, 0);
  bzero(buffer, BUFSIZ);
  read(sockfd, buffer, BUFSIZ);
  if (strcmp(buffer, "PONG") == 0) {
    close(sockfd);
    return 1;
  }
  close(sockfd);
  return 0;
}

int set_value_str(char* key_s, char* value) {
  s8 key = { .buf = (u8 *) key_s, .len = strlen(key_s), };
  s8 val = { .buf = (u8 *) value, .len = strlen(value), };
  int p[2];
  if (pipe(p) < 0) {
    perror("Failed to create pipe syscall.\n");
    exit(1);
  }

  write(p[1], "S", 2);
  //sprintf(buf, "S");
  write_u32_bytes(p, key.len);
  //s_print_u32_bytes(buf, key.len);
  write(p[1], key.buf, key.len);
  //s_s8_print(buf, key);
  write_u32_bytes(p, val.len);
  //s_print_u32_bytes(buf, val.len);
  write(p[1], val.buf, key.len);
  close(p[1]);
  //s_s8_print(buf, val);
  return p[0];
}

char* get_value_str(char* key_s) {
  s8 key = { .buf = (u8 *) key_s, .len = strlen(key_s), };

  char* buf;
  sprintf(buf, "G");
  s_print_u32_bytes(buf, key.len);
  s_s8_print(buf, key);
  return buf;
}

char* delete_str(char* key_s) {
  s8 key = { .buf = (u8 *) key_s, .len = strlen(key_s), };

  char* buf;
  sprintf(buf, "D");
  s_print_u32_bytes(buf, key.len);
  s_s8_print(buf, key);
  return buf;
}

char* clear_str() {
  return "D";
}

void set_value(int port, char* key, char* value) {
  printf("Before setvalue string.\n");
  int p = set_value_str(key, value);
  printf("Before send data.\n");
  send_data_i(port, p, false, 5);
}

char* get_value(int port, char* key) {
  char* msg = get_value_str(key);
  return send_data(port, msg, true);
}

void delete(int port, char* key) {
  char* msg = delete_str(key);
  send_data(port, msg, false);
}

void clear(int port) {
  char* msg = clear_str();
  send_data(port, msg, true);
}

#endif //CLIENT_H
