#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#define ERR_MSG(msg)                        \
  do {                                      \
    fprintf(stderr, "line:%d\n", __LINE__); \
    perror(msg);                            \
  } while (0)

#define SERVER_PORT 8888
#define SERVER_IP "127.0.0.1"
#define DICT_TXT_FILE_PATH "./dict.txt"

/* 传给子线程的结构体，保存客户端连接信息 */
struct cli_msg {
  int newfd;
  struct sockaddr_in cin;
};

/* sqlite3_exec 回调函数，用于获取返回数据个数 */
int callback_count(void *count, int argc, char **argv, char **azColName) {
  *(int *)count = atoi(argv[0]);
  return 0;
}

/* 回收僵尸子进程 */
void handler(int sig) {
  while (waitpid(-1, NULL, WNOHANG) > 0)
    ;
}

/* 初始化数据库文件 */
void initDatabase(sqlite3 *db) {
  char *sql;
  char query[1024] = {0};
  char word[100] = {0};
  char meaning[200] = {0};
  int count;
  /* 如果数据表 dict 不存在创建它 */
  sql = "CREATE TABLE IF NOT EXISTS dict (word TEXT, meaning TEXT);";
  if (sqlite3_exec(db, sql, NULL, NULL, NULL) != SQLITE_OK) {
    printf("error on sqlite_exec(): %s\n", sqlite3_errmsg(db));
    exit(-1);
  }
  /* 如果数据表 dict 的行数为 0，从 dict.txt 中导入数据 */
  sql = "SELECT COUNT(*) FROM dict;";
  if (sqlite3_exec(db, sql, callback_count, &count, NULL) != SQLITE_OK) {
    printf("error on sqlite_exec(): %s\n", sqlite3_errmsg(db));
    exit(-1);
  }
  if (0 == count) {
    printf("[LOG]: 字典数据库为空，正在从 dict.txt 中导入数据...\n");
    FILE *dictFile = fopen(DICT_TXT_FILE_PATH, "r");
    if (dictFile == NULL) {
      printf("error on fopen(): %s\n", strerror(errno));
      exit(-1);
    }
    while (1) {
      int n = fscanf(dictFile, "%s", word);
      fscanf(dictFile, " %[^\n]", meaning);
      sprintf(query, "INSERT INTO dict VALUES (\"%s\", \"%s\");", word,
              meaning);
      if (sqlite3_exec(db, query, NULL, NULL, NULL) != SQLITE_OK) {
        printf("error on sqlite_exec(): %s\n", sqlite3_errmsg(db));
        exit(-1);
      }
      if (n == EOF) {
        printf("[LOG]: dict.txt 数据导入完成。\n");
        break;
      }
    }
  }
  /* 如果数据表 user 不存在创建它 */
  sql = "CREATE TABLE IF NOT EXISTS user (username TEXT, password TEXT, "
        "is_login INT);";
  if (sqlite3_exec(db, sql, NULL, NULL, NULL) != SQLITE_OK) {
    printf("error on sqlite_exec(): %s\n", sqlite3_errmsg(db));
    exit(-1);
  }
}

/* 处理客户端连接 */
int deal_cli_msg(int newfd, struct sockaddr_in cin) {
  char buf[128] = "";
  ssize_t res = 0;
  while (1) {
    // 对字符串的操作，操作之前或者之后要清空
    // 防止之前的数据对这次操作有干扰
    bzero(buf, sizeof(buf));
    // 接收
    res = recv(newfd, buf, sizeof(buf), 0);
    if (res < 0) {
      ERR_MSG("recv");
      return -1;
    } else if (0 == res) {
      printf("[%s : %d] newfd = %d 客户端已下线 __%d__\n",
             inet_ntoa(cin.sin_addr), ntohs(cin.sin_port), newfd, __LINE__);
      break;
    }
    printf("[%s : %d] newfd = %d : %s __%d__\n", inet_ntoa(cin.sin_addr),
           ntohs(cin.sin_port), newfd, buf, __LINE__);
    // 发送
    strncat(buf, "*_*", 3);
    if(send(newfd, buf, sizeof(buf), 0) < 0) {
      ERR_MSG("send");
      return -1;
    }
    printf("发送成功\n");
  }
  return 0;
}

int main(int argc, const char **argv) {
  __sighandler_t s = signal(SIGCHLD, handler);
  if (s == SIG_ERR) {
    printf("error on signal(): %s\n", strerror(errno));
    exit(-1);
  }

  if (argc < 2) {
    printf("usage: %s file.db\n", argv[0]);
    exit(-1);
  }

  /* 打开数据库 */
  sqlite3 *db;
  if (sqlite3_open(argv[1], &db) != SQLITE_OK) {
    printf("error on sqlite_open(): %s\n", sqlite3_errmsg(db));
    exit(-1);
  }

  /* 初始化数据库 */
  initDatabase(db);

  /* 启动服务器 */
  int server_fd;

  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(SERVER_PORT);
  server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);

  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    printf("error on socket(): %s\n", strerror(errno));
    exit(-1);
  }

  if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) <
      0) {
    printf("error on bind(): %s\n", strerror(errno));
    exit(-1);
  }

  if (listen(server_fd, 10) < 0) {
    printf("error on listen(): %s\n", strerror(errno));
    exit(-1);
  }

  printf("[LOG]: 服务器启动成功，正在监听端口 %d ...\n", SERVER_PORT);

  int newfd;
  struct sockaddr_in client_addr;
  client_addr.sin_family = AF_INET;
  socklen_t client_addr_len = sizeof(client_addr);

  while (1) {
    newfd =
        accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
    if (newfd < 0) {
      printf("error on accept(): %s\n", strerror(errno));
      exit(-1);
    }
    int cpid = fork();
    if (cpid > 0) {
      close(newfd);
    } else if (cpid == 0) {
        close(server_fd);
        deal_cli_msg(newfd, client_addr);
        close(newfd);
        exit(0);
      }
    else {
      printf("error on fork(): %s\n", strerror(errno));
      exit(-1);
    }
  }

  /* 关闭数据库 */
  if (sqlite3_close(db) != SQLITE_OK) {
    printf("error on sqlite_close(): %s\n", sqlite3_errmsg(db));
    exit(-1);
  }

  close(server_fd);
  exit(0);
}
