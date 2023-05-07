#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define ERR_MSG(msg)                                                           \
  do {                                                                         \
    fprintf(stderr, "line:%d\n", __LINE__);                                    \
    perror(msg);                                                               \
  } while (0)

#define SERVER_PORT 8888
#define SERVER_IP "127.0.0.1"

int main(int argc, const char *argv[]) {
  int cfd = socket(AF_INET, SOCK_STREAM, 0);
  if (cfd < 0) {
    ERR_MSG("socket");
    return -1;
  }

  struct sockaddr_in sin;
  sin.sin_family = AF_INET;
  sin.sin_port = htons(SERVER_PORT);
  sin.sin_addr.s_addr = inet_addr(SERVER_IP);

  if (connect(cfd, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
    ERR_MSG("connect");
    return -1;
  }
  printf("connect success\n");

  char buf[128] = "";
  ssize_t res = 0;
  while (1) {
    printf("请输入>>> ");
    fgets(buf, sizeof(buf), stdin);
    buf[strlen(buf) - 1] = '\0';
    if (send(cfd, buf, sizeof(buf), 0) < 0) {
      ERR_MSG("send");
      return -1;
    }
    printf("发送成功\n");
    bzero(buf, sizeof(buf));
    res = recv(cfd, buf, sizeof(buf), 0);
    if (res < 0) {
      ERR_MSG("recv");
      return -1;
    } else if (0 == res) {
      printf("服务器下线\n");
      break;
    }
    printf("%s __%d__\n", buf, __LINE__);
  }

  close(cfd);

  return 0;
}