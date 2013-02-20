#include "common.h"

SERVER SERVER_DESC;

void init_server();
void cleanup_server();

void quit_handler(int i);

void receive_and_handle(int queue, MSG_TYPE type, void (*handler) (const void *));

void loop();

void change_room(char* name, char* room);

void response_status(int queue, int status);

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

  log_write("ALIVE: %d\n", SERVER_DESC.server_msgid);
}

void cleanup_server() {
repository_lock();

  repository_server_remove(SERVER_DESC.server_msgid);

repository_unlock();

  msgctl(SERVER_DESC.client_msgid, IPC_RMID, 0);
  msgctl(SERVER_DESC.server_msgid, IPC_RMID, 0);

  log_write("DOWN: %d\n", SERVER_DESC.server_msgid);
}


void change_room(char* name, char* room) {
  int i;

  ROOM desc;
  int room_exist = 0;

  for (i = 0; i < GLOBAL_REPO->active_clients; i++) {
    if (strcmp(GLOBAL_REPO->clients[i].name, name) == 0) {
      strcpy(desc.name, GLOBAL_REPO->clients[i].room);

      strcpy(GLOBAL_REPO->clients[i].room, room);

      printf("Przechodze z '%s' do '%s'\n", desc.name, room);

      break;
    }
  }

  for (i = 0; i < GLOBAL_REPO->active_rooms; i++) {
    if (strcmp(GLOBAL_REPO->rooms[i].name, desc.name) == 0) {
      GLOBAL_REPO->rooms[i].clients--;
      if (GLOBAL_REPO->rooms[i].clients == 0) {
        desc.clients = -1;

        printf("Zostawiam pusty pokoj\n");
      }
    } else
    if (strcmp(GLOBAL_REPO->rooms[i].name, room) == 0) {
      GLOBAL_REPO->rooms[i].clients++;
      room_exist = 1;

      printf("Dolaczam do istniejacego\n");
    }
  }

  if (desc.clients == -1) {
    repository_room_remove(desc.name);
    printf("Usuwam stary pokoj\n");
  }

  if (room_exist == 0) {
    strcpy(desc.name, room);
    desc.clients = 1;

    repository_room_add(desc);

    printf("Tworze nowy pokoj\n");
  }
}

void response_status(int queue, int status) {
  STATUS_RESPONSE res;

  res.type = STATUS;
  res.status = status;

  msgsnd(queue, &res, sizeof(res)-sizeof(long), 0);
}

void handle_server_list(const void * req) {
  SERVER_LIST_REQUEST * slr = (SERVER_LIST_REQUEST*) req;

  printf("Got request!\n");
  int i;

repository_lock();

  SERVER_LIST_RESPONSE res;
  res.type = SERVER_LIST;
  res.active_servers = GLOBAL_REPO->active_servers;
  for (i = 0; i < GLOBAL_REPO->active_servers; i++) {
    res.servers[i] = GLOBAL_REPO->servers[i].client_msgid;
    res.clients_on_servers[i] = GLOBAL_REPO->servers[i].clients;
  }

repository_unlock();

  msgsnd(slr->client_msgid, &res, sizeof(res)-sizeof(long), 0);
}

void handle_room_list(const void * req) {
  CLIENT_REQUEST * cr = (CLIENT_REQUEST*) req;

repository_lock();
printf("dostalem!\n");
  ROOM_LIST_RESPONSE res;
  res.type = ROOM_LIST;
  res.active_rooms = GLOBAL_REPO->active_rooms;
  //memcpy(res.rooms, GLOBAL_REPO->rooms, sizeof(ROOM) * GLOBAL_REPO->active_rooms);
int i;
  for (i = 0; i < GLOBAL_REPO->active_rooms; i++) {
    strcpy(res.rooms[i].name, GLOBAL_REPO->rooms[i].name);
    res.rooms[i].clients = GLOBAL_REPO->rooms[i].clients;
  }

  msgsnd(cr->client_msgid, &res, sizeof(res)-sizeof(long), 0);

printf("odsylam!\n");

repository_unlock();

}

void handle_client_list(const void * req) {

}

void handle_login(const void * req) {
  CLIENT_REQUEST * cr = (CLIENT_REQUEST*) req;
  int i;
  int ok = 1;

repository_lock();

  if (SERVER_DESC.clients >= SERVER_CAPACITY) {
    response_status(cr->client_msgid, 503);

    ok = 0;
  }

  if (ok)
  for (i = 0; i < GLOBAL_REPO->active_clients; i++) {
    if (strcmp(GLOBAL_REPO->clients[i].name, cr->client_name) == 0) {
      response_status(cr->client_msgid, 409);
      ok = 0;
      break;
    }
  }

  if (ok) {
    CLIENT desc;
    strcpy(desc.name, cr->client_name);
    desc.server_id = SERVER_DESC.server_msgid;

    repository_client_add(desc);

    SERVER_DESC.clients++;

    change_room(desc.name, "");

    response_status(cr->client_msgid, 201);
  }

repository_unlock();

  if (ok)
    log_write("LOGGED_IN@%d: %s\n", SERVER_DESC.server_msgid, cr->client_name);
}

void handle_logout(const void * req) {
  CLIENT_REQUEST * cr = (CLIENT_REQUEST*) req;

  int i;

repository_lock();

  change_room(cr->client_name, "");

  GLOBAL_REPO->rooms[0].clients--;

  repository_client_remove(cr->client_name);

repository_unlock();

  log_write("LOGGED_OUT@%d: %s\n", SERVER_DESC.server_msgid, cr->client_name);
}

void handle_status(const void * req) {

}

void handle_change_room(const void * req) {
  CHANGE_ROOM_REQUEST* crr = (CHANGE_ROOM_REQUEST*) req;

repository_lock();

  change_room(crr->client_name, crr->room_name);

  response_status(crr->client_msgid, 202);

repository_unlock();

}

void handle_private(const void * req) {

}

void handle_public(const void * req) {

}

void handle_private_server(const void * req) {

}

void handle_public_server(const void * req) {

}
