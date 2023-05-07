#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
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

char username[128] = "";
int cfd = -1;
int is_login = 0;

int prompt() {
  int choice = 0;
  if (!is_login) {
    printf("1.注册\n");
    printf("2.登录\n");
    printf("3.退出\n");
  } else if (is_login) {
    printf("1.查词\n");
    printf("2.返回上级\n");
  }
  printf("请选择>>> ");
  scanf("%d", &choice);
  while (getchar() != '\n')
    ;
  return is_login ? choice + 3 : choice;
}

int login() {
  if (is_login)
    return 1;
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
  // 接收登录结果
  while (1) {
    bzero(buf, sizeof(buf));
    ssize_t res = recv(cfd, buf, sizeof(buf), 0);
    if (res < 0) {
      ERR_MSG("recv");
      return -1;
    } else if (0 == res) {
      printf("服务器下线\n");
      return -1;
    }
    if ('g' == buf[0]) {
      printf("登录成功\n");
      is_login = 1;
      return 0;
    } else if ('e' == buf[0]) {
      printf("登录失败：%s\n", buf + 2);
      return 1;
    } else {
      continue;
    }
  }
}

int signup() {
  if (is_login)
    return 1;
  char password[128] = "";
  char buf[256] = "";
  printf("请输入用户名>>> ");
  scanf("%s", username);
  printf("请输入密码>>> ");
  scanf("%s", password);
  sprintf(buf, "%c%c%s%c%s", 's', '\0', username, '\0', password);
  if (send(cfd, buf, sizeof(buf), 0) < 0) {
    ERR_MSG("send");
    return -1;
  }
  // 接收注册结果
  while (1) {
    bzero(buf, sizeof(buf));
    ssize_t res = recv(cfd, buf, sizeof(buf), 0);
    if (res < 0) {
      ERR_MSG("recv");
      return -1;
    } else if (0 == res) {
      printf("服务器下线\n");
      return -1;
    }
    if ('g' == buf[0]) {
      printf("注册成功\n");
      return 0;
    } else if ('e' == buf[0]) {
      printf("注册失败：%s\n", buf + 2);
      return 1;
    } else {
      continue;
    }
  }
}

int query() {
  if (!is_login)
    return 1;
  char buf[1024] = {0};
  char word[128] = {0};
  printf("请输入要查询的单词>>> ");
  scanf("%s", word);
  sprintf(buf, "%c%c%s", 'q', '\0', word);
  if (send(cfd, buf, sizeof(buf), 0) < 0) {
    ERR_MSG("send");
    return -1;
  }
  // 接收查询结果
  while (1) {
    bzero(buf, sizeof(buf));
    ssize_t res = recv(cfd, buf, sizeof(buf), 0);
    if (res < 0) {
      ERR_MSG("recv");
      return -1;
    } else if (0 == res) {
      printf("服务器下线\n");
      return -1;
    }
    if ('e' == buf[0]) {
      printf("查询失败：%s\n", buf + 2);
      return 1;
    } else if ('g' == buf[0]) {
      printf("查询结果：%s\n", buf + 2);
      return 0;
    } else {
      continue;
    }
  }
}

int logout() {
  if (!is_login)
    return 1;
  char buf[1024] = {0};
  sprintf(buf, "%c%c%s", 'o', '\0', username);
  if (send(cfd, buf, sizeof(buf), 0) < 0) {
    ERR_MSG("send");
    return -1;
  }
  // 接收注销结果
  while (1) {
    bzero(buf, sizeof(buf));
    ssize_t res = recv(cfd, buf, sizeof(buf), 0);
    if (res < 0) {
      ERR_MSG("recv");
      return -1;
    } else if (0 == res) {
      printf("服务器下线\n");
      return -1;
    }
    if ('g' == buf[0]) {
      printf("注销成功\n");
      is_login = 0;
      return 0;
    } else if ('e' == buf[0]) {
      printf("注销失败：%s\n", buf + 2);
      return 1;
    } else {
      continue;
    }
  }
}

void handle_exit(int sig) {
  logout();
  exit(0);
}

int main(int argc, const char *argv[]) {
  signal(SIGINT, handle_exit);
  cfd = socket(AF_INET, SOCK_STREAM, 0);
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

  while (1) {
    switch (prompt()) {
    case 1:
      signup();
      break;
    case 2:
      login();
      break;
    case 3:
      logout();
      exit(0);
      break;
    case 4:
      query();
      break;
    case 5:
      logout();
      break;
    default:
      printf("输入错误\n");
      break;
    }
  }
  close(cfd);
  return 0;
}