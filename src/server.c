#include "common.h"

SERVER SERVER_DESC;

void init_server();
void cleanup_server();

void quit_handler(int i);

void receive_and_handle(int queue, MSG_TYPE type, void (*handler) (const void *));

void loop();

void handle_server_list(const void *);
void handle_room_list(const void *);
void handle_client_list(const void *);
void handle_login(const void *);
void handle_logout(const void *);
void handle_status(const void *);
void handle_change_room(const void *);
void handle_private(const void *);
void handle_public(const void *);
void handle_private_server(const void *);
void handle_public_server(const void *);

int main(int argc, char* argv[]) {
  init();
  init_server();

  signal(SIGINT, quit_handler);
  signal(SIGQUIT, quit_handler);

  loop();

  cleanup_server();
  cleanup();

  return EXIT_SUCCESS;
}

void quit_handler(int i) {
  cleanup_server();
  cleanup();

  exit(0);
}

void loop() {
  while (1) {
    receive_and_handle(SHARED_QUEUE_ID,           SERVER_LIST,  handle_server_list);

    receive_and_handle(SERVER_DESC.client_msgid,  ROOM_LIST,    handle_room_list);
    receive_and_handle(SERVER_DESC.client_msgid,  CLIENT_LIST,  handle_client_list);
    receive_and_handle(SERVER_DESC.client_msgid,  LOGIN,        handle_login);
    receive_and_handle(SERVER_DESC.client_msgid,  LOGOUT,       handle_logout);
    receive_and_handle(SERVER_DESC.client_msgid,  STATUS,       handle_status);
    receive_and_handle(SERVER_DESC.client_msgid,  CHANGE_ROOM,  handle_change_room);
    receive_and_handle(SERVER_DESC.client_msgid,  PRIVATE,      handle_private);
    receive_and_handle(SERVER_DESC.client_msgid,  PUBLIC,       handle_public);

    receive_and_handle(SERVER_DESC.server_msgid,  PRIVATE,      handle_private_server);
    receive_and_handle(SERVER_DESC.server_msgid,  PUBLIC,       handle_public_server);

  }
}

void receive_and_handle(int queue, MSG_TYPE type, void (*handler) (const void *)) {
  void * out = malloc(8192);

  if (msgrcv(queue, out, 8192, (long)type, IPC_NOWAIT) != -1) {
    handler(out);
  }
}

void init_server() {
  SERVER_DESC.clients = 0;
  srand(time(0));

  do {
    SERVER_DESC.client_msgid = msgget(rand() % 10000000 + 1000, 0666 | IPC_CREAT | IPC_EXCL);
  } while (SERVER_DESC.client_msgid == -1);

  do {
    SERVER_DESC.server_msgid = msgget(rand() % 10000000 + 1000, 0666 | IPC_CREAT | IPC_EXCL);
  } while (SERVER_DESC.server_msgid == -1);

repository_lock();

  repository_server_add(SERVER_DESC);

repository_unlock();
}

void cleanup_server() {
repository_lock();

  repository_server_remove(SERVER_DESC.server_msgid);

repository_unlock();

  msgctl(SERVER_DESC.client_msgid, IPC_RMID, 0);
  msgctl(SERVER_DESC.server_msgid, IPC_RMID, 0);
}


void handle_server_list(const void * req) {

}

void handle_room_list(const void * req) {

}

void handle_client_list(const void * req) {

}

void handle_login(const void * req) {

}

void handle_logout(const void * req) {

}

void handle_status(const void * req) {

}

void handle_change_room(const void * req) {

}

void handle_private(const void * req) {

}

void handle_public(const void * req) {

}

void handle_private_server(const void * req) {

}

void handle_public_server(const void * req) {

}
