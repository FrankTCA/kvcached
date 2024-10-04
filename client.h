#include <arpa/inet.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "ds.h"
#include "print_functions.h"
#ifndef CLIENT_H
#define CLIENT_H
#define SA struct sockaddr

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
  serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
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
  serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
  serv_addr.sin_port = htons(port);
  if (connect(sockfd, (SA*)&serv_addr, sizeof(serv_addr)) == -1) {
    return 0;
  }
  return 1;
}

char* set_value_str(char* key_s, char* value) {
  s8 key = { .buf = (u8 *) key_s, .len = strlen(key_s), };
  s8 val = { .buf = (u8 *) value, .len = strlen(value), };

  char* buf;
  sprintf(buf, "S");
  s_print_u32_bytes(buf, key.len);
  s_s8_print(buf, key);
  s_print_u32_bytes(buf, val.len);
  s_s8_print(buf, val);
  return buf;
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
  char* msg = set_value_str(key, value);
  send_data(port, msg, false);
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
