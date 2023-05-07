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

#define ERR_MSG(msg)                                                           \
  do {                                                                         \
    fprintf(stderr, "line:%d\n", __LINE__);                                    \
    perror(msg);                                                               \
  } while (0)

#define DICT_TXT_FILE_PATH "./dict.txt"
#define SERVER_PORT 8888
#define SERVER_IP "127.0.0.1"

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

/* sqlite3_exec 回调函数，用于获取返回的单个字符串 */
int callback_username(void *username, int argc, char **argv, char **azColName) {
  strcpy((char *)username, argv[0]);
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
  // 如果数据表 user 不存在创建它
  sql = "CREATE TABLE IF NOT EXISTS user (username TEXT, password TEXT, "
        "is_login INT);";
  if (sqlite3_exec(db, sql, NULL, NULL, NULL) != SQLITE_OK) {
    printf("error on sqlite_exec(): %s\n", sqlite3_errmsg(db));
    exit(-1);
  }
  // 将所有 user 的 is_login 置为 0
  sql = "UPDATE user SET is_login = 0;";
  if (sqlite3_exec(db, sql, NULL, NULL, NULL) != SQLITE_OK) {
    printf("error on sqlite_exec(): %s\n", sqlite3_errmsg(db));
    exit(-1);
  }
  // 如果数据表 record 不存在创建它
  sql = "CREATE TABLE IF NOT EXISTS record (username TEXT, word TEXT, "
        "meaning TEXT, time TEXT);";
  if (sqlite3_exec(db, sql, NULL, NULL, NULL) != SQLITE_OK) {
    printf("error on sqlite_exec(): %s\n", sqlite3_errmsg(db));
    exit(-1);
  }
}

/* 发送消息给客户端 */
int send_msg(char type, char *msg, int newfd) {
  char buf[1024] = {0};
  sprintf(buf, "%c%c%s", type, '\0', msg);
  if (send(newfd, buf, sizeof(buf), 0) < 0) {
    ERR_MSG("send");
    return -1;
  }
  return 0;
}

/* 处理客户端登入请求 */
void deal_cli_login_msg(char *msg, sqlite3 *db, int newfd) {
  char *username = msg + 2;
  char *password = username + strlen(username) + 1;
  char query[1024] = {0};
  int count;
  // 检查该用户是否已经登入
  sprintf(query,
          "SELECT COUNT(*) FROM user WHERE username = \"%s\" AND "
          "password = \"%s\" AND is_login > 0;",
          username, password);
  if (sqlite3_exec(db, query, callback_count, &count, NULL) != SQLITE_OK) {
    printf("error on sqlite_exec(): %s\n", sqlite3_errmsg(db));
    exit(-1);
  }
  if (count > 0) { // 已经登入
    printf("[LOG]: 用户 %s 已经登入\n", username);
    send_msg('e', "该用户已经登入", newfd);
    return;
  }
  // 检查用户名和密码是否正确
  sprintf(query,
          "SELECT COUNT(*) FROM user WHERE username = \"%s\" AND "
          "password = \"%s\";",
          username, password);
  if (sqlite3_exec(db, query, callback_count, &count, NULL) != SQLITE_OK) {
    printf("error on sqlite_exec(): %s\n", sqlite3_errmsg(db));
    exit(-1);
  }
  if (count == 0) { // 用户名或密码错误
    printf("[LOG]: 用户 %s 登入失败\n", username);
    send_msg('e', "用户名或密码错误", newfd);
    return;
  }
  // 更新用户登入状态
  sprintf(query, "UPDATE user SET is_login = \"%d\" WHERE username = \"%s\";",
          newfd, username);
  if (sqlite3_exec(db, query, NULL, NULL, NULL) != SQLITE_OK) {
    printf("error on sqlite_exec(): %s\n", sqlite3_errmsg(db));
    exit(-1);
  }
  printf("[LOG]: 用户 %s 登入成功\n", username);
  send_msg('g', "登入成功", newfd);
}

/* 处理客户端注册 */
void deal_cli_signup_msg(char *msg, sqlite3 *db, int newfd) {
  char *username = msg + 2;
  char *password = username + strlen(username) + 1;
  char query[1024] = {0};
  int count;
  // 检查该用户是否已经注册
  sprintf(query, "SELECT COUNT(*) FROM user WHERE username = \"%s\";",
          username);
  if (sqlite3_exec(db, query, callback_count, &count, NULL) != SQLITE_OK) {
    printf("error on sqlite_exec(): %s\n", sqlite3_errmsg(db));
    exit(-1);
  }
  if (count > 0) { // 已经注册
    printf("[LOG]: 用户 %s 已经注册\n", username);
    send_msg('e', "该用户已经注册", newfd);
    return;
  }
  // 将用户信息插入数据库
  sprintf(query, "INSERT INTO user VALUES (\"%s\", \"%s\", 0);", username,
          password);
  if (sqlite3_exec(db, query, NULL, NULL, NULL) != SQLITE_OK) {
    printf("error on sqlite_exec(): %s\n", sqlite3_errmsg(db));
    exit(-1);
  }
  printf("[LOG]: 用户 %s 注册成功\n", username);
  send_msg('g', "注册成功", newfd);
}

void deal_cli_query_msg(char *msg, sqlite3 *db, int newfd) {
  char *word = msg + 2;
  char query[1024] = {0};
  char **result;
  int nrow, ncolumn;
  char *errmsg;
  // 查询单词
  sprintf(query, "SELECT * FROM dict WHERE word = \"%s\";", word);
  if (sqlite3_get_table(db, query, &result, &nrow, &ncolumn, &errmsg) !=
      SQLITE_OK) {
    printf("error on sqlite3_get_table(): %s\n", errmsg);
    exit(-1);
  }
  if (nrow == 0) { // 单词不存在
    printf("[LOG]: 用户查询单词 %s 不存在\n", word);
    send_msg('e', "单词不存在", newfd);
    return;
  }
  // 单词存在
  send_msg('g', result[3], newfd);
  // 通过 newfd 查询用户名
  char username[20] = {0};
  sprintf(query, "SELECT username FROM user WHERE is_login = \"%d\";", newfd);
  if (sqlite3_exec(db, query, callback_username, username, NULL) != SQLITE_OK) {
    printf("error on sqlite_exec(): %s\n", sqlite3_errmsg(db));
    exit(-1);
  }
  printf("[LOG]: 用户 %s 查询 %s ，结果为 %s\n", username, word, result[3]);
  // 保存记录
  sprintf(query,
          "INSERT INTO record VALUES (\"%s\", \"%s\", \"%s\",datetime('now', "
          "'localtime'));",
          username, word, result[3]);
  if (sqlite3_exec(db, query, NULL, NULL, NULL) != SQLITE_OK) {
    printf("error on sqlite_exec(): %s\n", sqlite3_errmsg(db));
    exit(-1);
  }
  sqlite3_free_table(result);
}

/* 处理客户端登出请求 */
void deal_cli_logout_msg(char *msg, sqlite3 *db, int newfd) {
  char *username = msg + 2;
  char query[1024] = {0};
  // 更新用户登入状态
  sprintf(query, "UPDATE user SET is_login = 0 WHERE username = \"%s\";",
          username);
  if (sqlite3_exec(db, query, NULL, NULL, NULL) != SQLITE_OK) {
    printf("error on sqlite_exec(): %s\n", sqlite3_errmsg(db));
    exit(-1);
  }
  send_msg('g', "登出成功", newfd);
  printf("[LOG]: 用户 %s 已登出\n", username);
}

/* 处理客户端连接 */
int deal_cli_msg(int newfd, struct sockaddr_in cin, sqlite3 *db) {
  char buf[1024] = {0};
  while (1) {
    bzero(buf, sizeof(buf));
    recv(newfd, buf, sizeof(buf), 0);
    switch (buf[0]) {
    case 's': // 注册
      deal_cli_signup_msg(buf, db, newfd);
      break;
    case 'l': // 登入
      deal_cli_login_msg(buf, db, newfd);
      break;
    case 'q': // 查询
      deal_cli_query_msg(buf, db, newfd);
      break;
    case 'o': // 登出
      deal_cli_logout_msg(buf, db, newfd);
      break;
    default:
      break;
    }
  }
  return 0;
}

int main(int argc, const char **argv) {
  signal(SIGCHLD, handler);

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
      deal_cli_msg(newfd, client_addr, db);
      close(newfd);
      exit(0);
    } else {
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
