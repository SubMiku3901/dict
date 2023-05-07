#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
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

int prompt(int status) {
  int choice = 0;
  if (0 == status) {
    printf("1.注册\n");
    printf("2.登录\n");
    printf("3.退出\n");
  } else if (1 == status) {
    printf("1.查词\n");
    printf("2.历史记录\n");
    printf("3.退出\n");
  }
  printf("请选择>>> ");
  scanf("%d", &choice);
  return choice;
}

int signup(int cfd) {
  char username[128] = "";
  char password[128] = "";
  printf("请输入用户名>>> ");
  scanf("%s", username);
  printf("请输入密码>>> ");
  scanf("%s", password);
  char buf[256] = "";
  sprintf(buf, "%c%c%s%c%s", 's', '\0', username, '\0', password);
  if (send(cfd, buf, sizeof(buf), 0) < 0) {
    ERR_MSG("send");
    return -1;
  }
  bzero(buf, sizeof(buf));
  ssize_t res = recv(cfd, buf, sizeof(buf), 0);
  if (res < 0) {
    ERR_MSG("recv");
    return -1;
  } else if (0 == res) {
    printf("服务器下线\n");
    return -1;
  }
  printf("%s __%d__\n", buf, __LINE__); // TBD: 解析
  return 0;
}

int login(int cfd) {
  char username[128] = {0};
  char password[128] = {0};
  printf("请输入用户名>>> ");
  scanf("%s", username);
  printf("请输入密码>>> ");
  scanf("%s", password);
  char buf[256] = "";
  sprintf(buf, "%c%c%s%c%s", 'l', '\0', username, '\0', password);
  if (send(cfd, buf, sizeof(buf), 0) < 0) {
    ERR_MSG("send");
    return -1;
  }
  bzero(buf, sizeof(buf));
  ssize_t res = recv(cfd, buf, sizeof(buf), 0);
  if (res < 0) {
    ERR_MSG("recv");
    return -1;
  } else if (0 == res) {
    printf("服务器下线\n");
    return -1;
  }
  if (buf[0] == 'g') {
    printf("登录成功\n");
    return 0;
  } else {
    printf("登录失败：%s\n", buf + 2);
    return 1;
  }
}

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
  int status = 0;
  int ret;
  while (1) {
    int choice = prompt(status);

    if (3 == choice)
      break;

    if (0 == status) {
      if (1 == choice) // 选择注册
        if (signup(cfd) < 0)
          exit(-1);
      if (2 == choice) { // 选择登录
        ret = login(cfd);
        if (ret < 0) exit(-1); // 连接错误
        if (ret == 0) status = 1; // 登录成功
        else continue; // 登录失败
      }
    }
    // printf("请输入>>> ");
    // fgets(buf, sizeof(buf), stdin);
    // buf[strlen(buf) - 1] = '\0';
    // if (send(cfd, buf, sizeof(buf), 0) < 0) {
    //   ERR_MSG("send");
    //   return -1;
    // }
    // printf("发送成功\n");
    // bzero(buf, sizeof(buf));
    // res = recv(cfd, buf, sizeof(buf), 0);
    // if (res < 0) {
    //   ERR_MSG("recv");
    //   return -1;
    // } else if (0 == res) {
    //   printf("服务器下线\n");
    //   break;
    // }
    // printf("%s __%d__\n", buf, __LINE__);
  }

  close(cfd);

  return 0;
}