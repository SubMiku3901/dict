#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define ERR_MSG(msg)                        \
  do {                                      \
    fprintf(stderr, "line:%d\n", __LINE__); \
    perror(msg);                            \
  } while (0)

#define SERVER_PORT 8888
#define IP "127.0.0.1"

int main(int argc, const char *argv[]) {
  // 创建流式套接字
  int cfd = socket(AF_INET, SOCK_STREAM, 0);
  if (cfd < 0) {
    ERR_MSG("socket");
    return -1;
  }

  // 绑定 -----> 必须绑定
  // 功能：将客户端的 IP 地址和端口绑定到客户端套接字文件中
  // 如果不绑定，则操作系统会自动给客户端绑定上本机 IP 和随机端口: 49152~65535

  // 填充服务器的地址信息结构体
  // 该真实结构体根据地址族制定：AF_INET：man 7 ip
  struct sockaddr_in sin;
  sin.sin_family = AF_INET;    // 必须填充 AF_INET
  sin.sin_port = htons(SERVER_PORT);  // 端口号的网络字节序 1024~49151
  sin.sin_addr.s_addr = inet_addr(IP);  // IP, 本机IP, ifconfig

  // 连接服务器
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
    // 发送
    if (send(cfd, buf, sizeof(buf), 0) < 0) {
      ERR_MSG("send");
      return -1;
    }
    printf("发送成功\n");
    // 对字符串的操作，操作之前或者之后要清空
    // 防止之前的数据对这次操作有干扰
    bzero(buf, sizeof(buf));
    // 接收
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

  // 关闭文件描述符
  close(cfd);

  return 0;
}