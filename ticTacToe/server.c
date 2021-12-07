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

#define DEBUG

#define CLIENT_MAX 128
#define CLIENT_NAME_MAXLEN 128
#define ROOM_MAX 128

#define BUF_SIZE 128
#define SPLIT_WORD " :\n"
#define PASSWD_LOCATION "./passwd"

int onlineNum = 0;
int roomNum = 1;

struct ONLINECLIENT {
  char username[CLIENT_NAME_MAXLEN];
  int fd;
} onlineClient[CLIENT_MAX];

struct GAMEROOM {
  int player1;
  int player2;
  int step;
  char table[8];
} gameRoom[ROOM_MAX];

void errorExit(const char *errorMessage);

int init(in_port_t *port);
void accept_request(void *arg);

int auth(const char *username, const char *password);
void getOnlineClient(int client);
void showTable(int client, int roomId);
void showUsage(int client);
void showWelcome(int client);
int judge(int roomId);
void sendAll(int client, const char *message);
void sendPm(int client, int target, const char *message);
int registerAccount(const char *arg1, const char *arg2);

int registerAccount(const char *arg1, const char *arg2) {
  FILE *fp;
  char buf[BUF_SIZE];

  // check duplicate
  fp = fopen(PASSWD_LOCATION, "r");

  while (fscanf(fp, "%s", buf) != EOF) {
    char *user = strtok(buf, SPLIT_WORD);
    char *pass = strtok(NULL, SPLIT_WORD);
    if (user == NULL || pass == NULL) continue;
    if (strncmp(user, arg1, strlen(user)) == 0) {
      fclose(fp);
      return 1;  // auth sucessful
    }
  }
  fclose(fp);

  fp = fopen(PASSWD_LOCATION, "a");
  if (fp == NULL) return -1;
  fprintf(fp, "%s:%s\n", arg1, arg2);
  fclose(fp);
  return 0;
}

void sendPm(int client, int target, const char *message) {
  char buf[BUF_SIZE];
  int len = 0;
  len = sprintf(buf, "pm form [%s]: %s\n", onlineClient[client].username,
                message);
  send(target, buf, len, 0);
}

void sendAll(int client, const char *message) {
  char buf[BUF_SIZE];
  int len = 0;
  len = sprintf(buf, "all form [%s]: %s\n", onlineClient[client].username,
                message);
  for (int i = 0; i != CLIENT_MAX; ++i) {
    if (onlineClient[i].fd != 0) {
      send(onlineClient[i].fd, buf, len, 0);
    }
  }
}

void showWelcome(int client) {
  send(client,
       "_______________          ________                 ______________      "
       "\n",
       strlen("_______________          ________                 "
              "______________      \n"),
       0);
  send(client,
       "___  __/___  _/______    ___  __/_____ _______    ___  __/_  __ \\____ "
       "\n",
       strlen("___  __/___  _/______    ___  __/_____ _______    ___  __/_  __ "
              "\\____ \n"),
       0);
  send(client,
       "__  /   __  / _  ___/    __  /  _  __ `/  ___/    __  /  _  / / /  _ "
       "\\\n",
       strlen("__  /   __  / _  ___/    __  /  _  __ `/  ___/    __  /  _  / / "
              "/  _ \\\n"),
       0);
  send(client,
       "_  /   __/ /  / /__      _  /   / /_/ // /__      _  /   / /_/ //  "
       "__/\n",
       strlen("_  /   __/ /  / /__      _  /   / /_/ // /__      _  /   / /_/ "
              "//  __/\n"),
       0);
  send(client,
       "/_/    /___/  \\___/      /_/    \\__,_/ \\___/      /_/    \\____/ "
       "\\___/ \n",
       strlen("/_/    /___/  \\___/      /_/    \\__,_/ \\___/      /_/    "
              "\\____/ \\___/ \n"),
       0);
  send(client, "\n", strlen("\n"), 0);
}

void showUsage(int client) {
  send(client, "===== Usage =====\n", strlen("===== Usage =====\n"), 0);
  send(client, "login [username] [password]\n",
       strlen("login [username] [password]\n"), 0);
  send(client, "== After login ==\n", strlen("== After login ==\n"), 0);
  send(client, "logout\n", strlen("logout\n"), 0);
  send(client, "list\n", strlen("list\n"), 0);
  send(client, "invite [clientId]\n", strlen("invite [clientId]\n"), 0);
  send(client, "pm [clientId] [message]\n", strlen("pm [clientId] [message]\n"),
       0);
  send(client, "all [message]\n", strlen("all [message]\n"), 0);
  send(client, "==== In Game ====\n", strlen("==== In Game ====\n"), 0);
  send(client, "# [0-8]\n", strlen("# [0-8]\n"), 0);
  send(client, "# table\n", strlen("# table\n"), 0);
  send(client, "=================\n", strlen("=================\n"), 0);
}

// 0: not finish 1: player1 win 2: player1 win 3:tie
int judge(int roomId) {
  const char *table = gameRoom[roomId].table;

  if (table[0] == table[1] && table[1] == table[2]) return table[0];
  if (table[3] == table[4] && table[4] == table[5]) return table[3];
  if (table[6] == table[7] && table[7] == table[8]) return table[6];
  if (table[0] == table[3] && table[3] == table[6]) return table[0];
  if (table[1] == table[4] && table[4] == table[7]) return table[1];
  if (table[2] == table[5] && table[5] == table[8]) return table[2];
  if (table[0] == table[4] && table[4] == table[8]) return table[0];
  if (table[2] == table[4] && table[4] == table[6]) return table[2];

  if (gameRoom[roomId].step == 8) return 3;

  return 0;
}

void showTable(int client, int roomId) {
  char buf[BUF_SIZE];
  size_t len = 0;
  const char OX[3] = {' ', 'O', 'X'};
  const char *table = gameRoom[roomId].table;
  send(client, "==== Table ====\n", strlen("==== Table ====\n"), 0);
  len = sprintf(buf, "| %c | %c | %c |\n", OX[table[0]], OX[table[1]],
                OX[table[2]]);
  send(client, buf, len, 0);
  len = sprintf(buf, "| %c | %c | %c |\n", OX[table[3]], OX[table[4]],
                OX[table[5]]);
  send(client, buf, len, 0);
  len = sprintf(buf, "| %c | %c | %c |\n", OX[table[6]], OX[table[7]],
                OX[table[8]]);
  send(client, buf, len, 0);
  send(client, "===== End =====\n", strlen("===== End =====\n"), 0);
}

// get all online client to find player2
void getOnlineClient(int client) {
  char buf[BUF_SIZE];
  size_t bufLen = 0;
  printf("[LIST] list user by %s\n", onlineClient[client].username);
  send(client, "=== Online Client ===\n", strlen("=== Online Client ===\n"), 0);
  for (int i = 0; i != CLIENT_MAX; i++) {
    if (onlineClient[i].fd != 0) {
      bufLen = snprintf(buf, BUF_SIZE, "username : %s clinetID : %d\n",
                        onlineClient[i].username, onlineClient[i].fd);
      send(client, buf, bufLen, 0);
    }
  }

  send(client, "=== End ===\n", strlen("=== End ===\n"), 0);
}

// login auth
int auth(const char *username, const char *password) {
  FILE *fp;
  char buf[BUF_SIZE];
  fp = fopen(PASSWD_LOCATION, "r");

  while (fscanf(fp, "%s", buf) != EOF) {
    char *user = strtok(buf, SPLIT_WORD);
    char *pass = strtok(NULL, SPLIT_WORD);
    if (user == NULL || pass == NULL) continue;
    if (strncmp(user, username, strlen(user)) == 0 &&
        strncmp(pass, password, strlen(pass)) == 0) {
      fclose(fp);
      return 1;  // auth sucessful
    }
  }
  fclose(fp);
  return 0;  // auth failed
}

// init() will finish create a socket and listen
int init(in_port_t *port) {
  int serverd = 0;
  int on = 1;
  struct sockaddr_in name;

  serverd = socket(PF_INET, SOCK_STREAM, 0);
  if (serverd == -1) errorExit("init failed: socket");
  memset(&name, 0, sizeof(name));

  // use ipv4
  name.sin_family = AF_INET;
  // htons Network Byte Order -> Host Byte Order
  name.sin_port = htons(*port);
  name.sin_addr.s_addr = htonl(INADDR_ANY);

  if ((setsockopt(serverd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))) < 0) {
    errorExit("init failed: setsockopt failed");
  }
  if (bind(serverd, (struct sockaddr *)&name, sizeof(name)) < 0)
    errorExit("init failed: bind");
  if (*port == 0) /* if dynamically allocating a port */
  {
    socklen_t namelen = sizeof(name);
    if (getsockname(serverd, (struct sockaddr *)&name, &namelen) == -1)
      errorExit("init failed: getsockname");
    *port = ntohs(name.sin_port);
  }
  if (listen(serverd, 5) < 0) errorExit("listen");
  return serverd;
}

// handle after request success
void accept_request(void *arg) {
  char buf[BUF_SIZE];
  int len = 0;
  int status = 0;  // Not login : 0, login successful : 1
  int roomId = 0;

  int client = (intptr_t)arg;

  showWelcome(client);

  while (1) {
    recv(client, buf, BUF_SIZE, 0);
    char *method = strtok(buf, SPLIT_WORD);

    if (method == NULL) {
      send(client, "Unknown Method\n", strlen("Unknown Method\n"), 0);
      continue;
    }

    char *arg1 = strtok(NULL, SPLIT_WORD);
    char *arg2 = strtok(NULL, SPLIT_WORD);
    char *arg3 = strtok(NULL, SPLIT_WORD);

#ifdef DEBUG
    printf("[DEBUG]method: %s args: %s %s %s\n", method, arg1, arg2, arg3);
    printf("Status: %d clientFd: %d roomId: %d\n", status, client, roomId);
#endif

    if (method == NULL || (strcasestr(method, "help") != NULL)) {
      showUsage(client);
      continue;
    }

    // unlogin
    if (status == 0) {
      if (arg1 == NULL || arg2 == NULL) {
        send(client, "Unknown Method\n", strlen("Unknown Method\n"), 0);
        continue;
      }
      // login
      if (strcasestr(method, "login") != NULL) {
        printf("[AAA] : '%s' attemp to login\n", arg1);
        status = auth(arg1, arg2);

        if (status == 0) {
          send(client, "Please Check your username and account\n",
               strlen("Please Check your username and account\n"), 0);
          printf("[AAA] : '%s' login failed\n", arg1);
          continue;
        }
        send(client, "Login Successful\n", strlen("Login Successful\n"), 0);
        // set Online
        onlineClient[client].fd = client;
        strncpy(onlineClient[client].username, arg1, CLIENT_NAME_MAXLEN);
        onlineNum++;
        printf("[AAA] : '%s' login successful\n", arg1);
      } else if (strcasestr(method, "register") != NULL) {
        if (registerAccount(arg1, arg2) == 0) {
          send(client, "Resgister successful\n",
               strlen("Resgister successful\n"), 0);
        } else {
          send(client, "Resgister failed\n", strlen("Resgister failed\n"), 0);
        }
      }
    }

    // login ready
    if (status == 1) {
      // List User
      if (strcasestr(method, "list") != NULL) {
        getOnlineClient(client);
        continue;
      } else if (strcasestr(method, "pm") != NULL) {
        sendPm(client, atoi(arg1), arg2);
        continue;
      } else if (strcasestr(method, "all") != NULL) {
        sendAll(client, arg1);
        continue;
      } else if (strcasestr(method, "invite") != NULL) {
        if (arg1 == NULL) {
          send(client, "Unknown Method\n", strlen("Unknown Method\n"), 0);
          continue;
        }
        // check
        char tmp[BUF_SIZE];
        int player2 = atoi(arg1);
        printf("[log] Player1 %s(%d) invite Player2 %s(%d)\n",
               onlineClient[client].username, onlineClient[client].fd,
               onlineClient[player2].username, onlineClient[player2].fd);
        roomId = roomNum++;
        gameRoom[roomId].player1 = client;
        gameRoom[roomId].player2 = player2;
        send(client, "Waiting for reponse\n", strlen("Waiting for reponse\n"),
             0);

        len = sprintf(buf, "Game invite from %s (accept %d/deny %d)\n",
                      onlineClient[client].username, roomId, roomId);
        send(player2, buf, len, 0);
      } else if (strcasestr(method, "accept") != NULL) {
        if (arg1 == NULL) {
          send(client, "Unknown Method\n", strlen("Unknown Method\n"), 0);
          continue;
        }
        roomId = atoi(arg1);

        // game init
        gameRoom[roomId].player2 = client;
        gameRoom[roomId].step = 0;
        memset(gameRoom[roomId].table, 0, sizeof(char) * 9);
        // start
        len = sprintf(buf, "RoomId=%d. Game Start!!!\n", roomId);
        send(client, buf, len, 0);
        send(gameRoom[roomId].player1, buf, len, 0);
        send(gameRoom[roomId].player1, "Your Turn : # [0-8]\n",
             strlen("Your Turn : # [0-8]\n"), 0);
        showTable(gameRoom[roomId].player1, roomId);
        showTable(gameRoom[roomId].player2, roomId);
      } else if (strcasestr(method, "deny") != NULL) {
        if (arg1 == NULL) {
          send(client, "Unknown Method\n", strlen("Unknown Method\n"), 0);
          continue;
        }
        roomId = atoi(arg1);
        len = sprintf(buf, "RoomId=%d. You have been reject!!!\n", roomId);
        send(gameRoom[roomId].player1, buf, len, 0);
        gameRoom[roomId].player1 = 0;
        roomId = 0;
        roomNum--;
      } else if (strcasestr(method, "logout") != NULL) {
        send(client, "Logout Successful\n", strlen("Logout Successful\n"), 0);
        onlineClient[client].fd = 0;
        close(client);
        pthread_exit((void *)0);
      } else if (strcasestr(method, "#") != NULL) {
        // in game
        if (arg1 == NULL) {
          send(client, "input error: # [0-8]\n",
               strlen("input error: # [0-8]\n"), 0);
          continue;
        }

        // game not start
        if (roomId == 0 || gameRoom[roomId].player1 == 0 ||
            gameRoom[roomId].player1 == 0) {
          send(client, "You are not in game\n", strlen("You are not in game\n"),
               0);
          continue;
        }

        // show game table
        if (strcasestr(arg1, "table") != NULL) {
          send(client, "Show table\n", strlen("Show table\n"), 0);
          continue;
        }

        // playerId
        int set = atoi(arg1);
        int playerId = -1;
        if (gameRoom[roomId].player1 == client) playerId = 1;
        if (gameRoom[roomId].player2 == client) playerId = 2;

        // Not player turn
        if ((gameRoom[roomId].step % 2 == 0 && playerId == 2) ||
            (gameRoom[roomId].step % 2 == 1 && playerId == 1)) {
          send(client, "Not your turn\n", strlen("Not your turn\n"), 0);
          continue;
        }

        // outOfRange
        if (set > 8 || set < 0) {
          send(client, "input error: # [0-8]\n",
               strlen("input error: # [0-8]\n"), 0);
          continue;
        }

        // Double step
        if (gameRoom[roomId].table[set] != 0) {
          send(client, "duplicate\n", strlen("duplicate\n"), 0);
          continue;
        }

        // success
        gameRoom[roomId].table[set] = playerId;

        int result = judge(roomId);
#ifdef DEBUG
        printf("[DEBUG] gameRoom : %d step : %d judge %d:\n", roomId,
               gameRoom[roomId].step, result);
#endif
        if (result != 0) {
          if (result == 1)
            len = sprintf(buf, "Player 1 %s Win\n",
                          onlineClient[gameRoom[roomId].player1].username);
          if (result == 2)
            len = sprintf(buf, "Player 2 %s Win\n",
                          onlineClient[gameRoom[roomId].player2].username);
          if (result == 3) len = sprintf(buf, "Tie\n");
          send(gameRoom[roomId].player1, buf, len, 0);
          send(gameRoom[roomId].player2, buf, len, 0);
          showTable(gameRoom[roomId].player1, roomId);
          showTable(gameRoom[roomId].player2, roomId);
          // reset gameRoom
          gameRoom[roomId].player1 = 0;
          gameRoom[roomId].player2 = 0;
          gameRoom[roomId].step = 0;
          roomNum--;
          continue;
        }

        showTable(client, roomId);

        if (playerId == 1) {
          showTable(gameRoom[roomId].player2, roomId);
          send(gameRoom[roomId].player2, "Your Turn\n", strlen("Your Turn\n"),
               0);
        } else {
          showTable(gameRoom[roomId].player1, roomId);
          send(gameRoom[roomId].player1, "Your Turn\n", strlen("Your Turn\n"),
               0);
        }

        gameRoom[roomId].step++;
      } else {
        send(client, "Unknown Method\n", strlen("Unknown Method\n"), 0);
        continue;
      }
    }
  }
}

// error and exit
void errorExit(const char *errorMessage) {
  perror(errorMessage);
  exit(-1);
}

int main() {
  int server_sock = -1;
  in_port_t port = 4000;

  int client_socket = -1;
  struct sockaddr_in client_name;
  pthread_t newthread;

  socklen_t client_name_len = sizeof(client_name);

  // init onlineClient table
  memset(onlineClient, 0, sizeof(onlineClient));

  server_sock = init(&port);
  printf("Tic Tac Toe server running on port %d\n", port);

  while (1) {
    client_socket =
        accept(server_sock, (struct sockaddr *)&client_name, &client_name_len);

    // accept error
    if (client_socket == -1) {
      perror("[error] accept");
      continue;
    }

    /* accept_request(&client_sock); */
    if (pthread_create(&newthread, NULL, (void *)accept_request,
                       (void *)(intptr_t)client_socket) != 0)
      perror("[error] pthread_create");
  }

  close(server_sock);

  return (0);
}
