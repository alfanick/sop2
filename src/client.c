#include <stdlib.h>
#include "common.h"

void init_client();
void cleanup_client();

int join_server();

void output_loop();
void input_loop();

void register_client();

void parse_line(char * line, MSG_TYPE * cmd, char ** argument);

int await_status(MSG_TYPE type);

void send_request(int queue, MSG_TYPE type);
int send_request_with_status(int queue, MSG_TYPE type);

void handle_heartbeat(const void *);

CLIENT CLIENT_DESC;
SERVER_LIST_RESPONSE SERVERS;
int CLIENT_QUEUE;
int OUTPUT_PID;
int MAIN_PID;

int OUTPUT_ALIVE = 1;

int main(int argc, char* argv[]) {
  MAIN_PID = getpid();

  init();
  init_client();

  register_client();

  if ((OUTPUT_PID = fork()) == 0) {
    output_loop();
  } else {
    input_loop();
  }

  wait(NULL);

  cleanup_client();
  cleanup();

  return EXIT_SUCCESS;
}

void output_loop() {
  while (OUTPUT_ALIVE) {
    sleep(1);

    receive_and_handle(CLIENT_QUEUE, HEARTBEAT, handle_heartbeat);

    //printf("output1\n");
  }

}

void handle_heartbeat(const void * req) {
  STATUS_RESPONSE * res = (STATUS_RESPONSE*) req;

  printf("pingpong!\n");
  if (res->status == CLIENT_DESC.server_id)
    send_request(res->status, HEARTBEAT);
}

void input_loop() {
  size_t size = 1024;
  char *line = malloc(size + 1);
  char *argument;
  MSG_TYPE cmd;

  while (1) {
    getline(&line, &size, stdin);

    parse_line(line, &cmd, &argument);

    if (cmd == CHANGE_ROOM) {
      CHANGE_ROOM_REQUEST req;
      req.type = CHANGE_ROOM;
      req.client_msgid = CLIENT_QUEUE;
      strcpy(req.room_name, argument);
      strcpy(req.client_name, CLIENT_DESC.name);

      msgsnd(CLIENT_DESC.server_id, &req, sizeof(req)-sizeof(long), 0);

      if (await_status(STATUS) == 202) {
        printf("Room changed to '%s'\n", argument);
      }

    } else
    if (cmd == ROOM_LIST) {

      send_request(CLIENT_DESC.server_id, ROOM_LIST);

      ROOM_LIST_RESPONSE res;
      msgrcv(CLIENT_QUEUE, &res, sizeof(res) - sizeof(long), ROOM_LIST, 0);

      int i;
      printf("Available rooms:\n");
      for (i = 0; i < res.active_rooms; i++) {
        if (res.rooms[i].clients > 0) {
          printf("  - %s (%d)\n", res.rooms[i].name, res.rooms[i].clients);
        }
      }

    } else
    if (cmd == CLIENT_LIST) {

      send_request(CLIENT_DESC.server_id, CLIENT_LIST);

      CLIENT_LIST_RESPONSE res;
      msgrcv(CLIENT_QUEUE, &res, sizeof(res) - sizeof(long), CLIENT_LIST, 0);

      int i;
      printf("Available clients:\n");
      for (i = 0; i < res.active_clients; i++) {
        printf("  - %s\n", res.names[i]);
      }

    } else
    if (cmd == PUBLIC) {

    } else
    if (cmd == PRIVATE) {

    } else
    if (cmd == LOGOUT) {
      printf("Logged out, c'ya!\n");

      OUTPUT_ALIVE = 0;
      kill(OUTPUT_PID, SIGKILL);
      break;
    } else {
      printf("!!! Unknown command\n");
    }
  }

  free(line);
  free(argument);
}

void parse_line(char * line, MSG_TYPE * cmd, char ** argument) {
  *cmd = HEARTBEAT;

  if (line[0] == '/') {
    // join, quit, priv, list
    if (strstr(line, "/join") == line) {
      *cmd = CHANGE_ROOM;
      *argument = strndup(line + 6, strlen(line)-7);
    } else
    if (strstr(line, "/quit") == line) {
      *cmd = LOGOUT;
      *argument = NULL;
    } else
    if (strstr(line, "/msg") == line) {
      *cmd = PRIVATE;
      *argument = strndup(line + 5, strlen(line)-5);
    } else
    if (strstr(line, "/rooms") == line) {
      *cmd = ROOM_LIST;
      *argument = NULL;
    } else
    if (strstr(line, "/users") == line) {
      *cmd = CLIENT_LIST;
      *argument = NULL;
    }
  } else {
    *cmd = PUBLIC;
    *argument = strndup(line, strlen(line));
  }
}

int join_server() {
  int server_full = 1;
  int min_id = INT_MAX;
  int min_clients = INT_MAX;

  int i;

  do {
    SERVER_LIST_REQUEST req;
    req.type = SERVER_LIST;
    req.client_msgid = CLIENT_QUEUE;
    msgsnd(SHARED_QUEUE_ID, &req, sizeof(req) - sizeof(long), 0);

    msgrcv(CLIENT_QUEUE, &SERVERS, sizeof(SERVERS), SERVER_LIST, 0);

    min_id = INT_MAX;
    min_clients = INT_MAX;
    for (i = 0; i < SERVERS.active_servers; i++) {
      if (SERVERS.clients_on_servers[i] < min_clients) {
        min_id = SERVERS.servers[i];
        min_clients = SERVERS.clients_on_servers[i];
      }
    }

    switch (send_request_with_status(min_id, LOGIN)) {
      case 201:
        printf("Logged in!\n");

        server_full = 0;

        CLIENT_DESC.server_id = min_id;

        break;
      case 409:
        printf("!!! Username already in system!\n");

        return -1;
      case 503:
        printf("!!! Full server, trying another...\n");
        sleep(1);
        break;
    }
  } while(server_full);

  return 0;
}

void send_request(int queue, MSG_TYPE type) {
  CLIENT_REQUEST req;

  req.type = type;
  req.client_msgid = CLIENT_QUEUE;
  strcpy(req.client_name, CLIENT_DESC.name);

  msgsnd(queue, &req, sizeof(req) - sizeof(long), 0);
}

int await_status(MSG_TYPE type) {
  STATUS_RESPONSE res;

  time_t start_time = time(0);
  while (1) {
    if (msgrcv(CLIENT_QUEUE, &res, sizeof(res) - sizeof(long), type, IPC_NOWAIT) != -1)
      break;

    if (time(0)-start_time > TIMEOUT) {
      printf("!!! Lost connection with server! Closing...\n");

      OUTPUT_ALIVE = 0;
      kill(OUTPUT_PID, SIGKILL);
      exit(0);
    }
  }

  return res.status;
}

int send_request_with_status(int queue, MSG_TYPE type) {
  send_request(queue, type);

  return await_status(STATUS);
}

void init_client() {
  do {
    CLIENT_QUEUE = msgget(rand() % 10000000 + 1000, 0666 | IPC_CREAT | IPC_EXCL);
  } while (CLIENT_QUEUE == -1);
}

void cleanup_client() {
  printf("cleaning up...\n");

  send_request(CLIENT_DESC.server_id, LOGOUT);

  msgctl(CLIENT_QUEUE, IPC_RMID, 0);
}

void register_client() {
  do {
    printf("Username: ");
    scanf("%s", CLIENT_DESC.name);
  } while (join_server() == -1);

}
