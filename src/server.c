#include "common.h"

SERVER SERVER_DESC;

void init_server();
void cleanup_server();

void quit_handler(int i);

void receive_and_handle(int queue, MSG_TYPE type, void (*handler) (const void *));

void loop();

void change_room(char* name, char* room);

void logout_user(char* name);

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

void handle_heartbeat(const void *);
void check_heartbeats();


CLIENT_REL LOCAL_CLIENTS[SERVER_CAPACITY];
void init_local_clients();
void local_client_add(CLIENT desc, int msgid);
void local_client_remove(char * name);
void local_client_set_time(int i, int t);
int local_client_tick_time(int i);
void local_client_room_change(char * name, char * room);

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
  time_t start = time(NULL);
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

    receive_and_handle(SERVER_DESC.client_msgid,  HEARTBEAT,    handle_heartbeat);

    if (time(NULL)-start > 1) {
      start = time(NULL);
      printf("sekunda\n");

      check_heartbeats();
    }
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

  init_local_clients();

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

  ROOM_LIST_RESPONSE res;
  res.type = ROOM_LIST;
  res.active_rooms = GLOBAL_REPO->active_rooms;
  memcpy(res.rooms, GLOBAL_REPO->rooms, sizeof(GLOBAL_REPO->rooms));

  msgsnd(cr->client_msgid, &res, sizeof(res)-sizeof(long), 0);

repository_unlock();

}

void handle_client_list(const void * req) {
  CLIENT_REQUEST * cr = (CLIENT_REQUEST*) req;
  int i;

repository_lock();

  CLIENT_LIST_RESPONSE res;
  res.type = CLIENT_LIST;
  res.active_clients = GLOBAL_REPO->active_clients;

  for (i = 0; i < MAX_CLIENTS; i++) {
    strcpy(res.names[i], GLOBAL_REPO->clients[i].name);
  }

  msgsnd(cr->client_msgid, &res, sizeof(res)-sizeof(long), 0);

repository_unlock();

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
    local_client_add(desc, cr->client_msgid);

    SERVER_DESC.clients++;

    change_room(desc.name, "");

    response_status(cr->client_msgid, 201);
  }

repository_unlock();

  if (ok)
    log_write("LOGGED_IN@%d: %s\n", SERVER_DESC.server_msgid, cr->client_name);
}

void logout_user(char * name) {

repository_lock();

  change_room(name, "");

  GLOBAL_REPO->rooms[0].clients--;

  repository_client_remove(name);

  local_client_remove(name);

  SERVER_DESC.clients--;

repository_unlock();

  log_write("LOGGED_OUT@%d: %s\n", SERVER_DESC.server_msgid, name);

}

void handle_logout(const void * req) {
  CLIENT_REQUEST * cr = (CLIENT_REQUEST*) req;

  logout_user(cr->client_name);
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

void handle_heartbeat(const void * req) {
  CLIENT_REQUEST* cr = (CLIENT_REQUEST*)req;

  int i;

  for (i = 0; i < SERVER_CAPACITY; i++) {
    if (strcmp(cr->client_name, LOCAL_CLIENTS[i].name)==0) {
      local_client_set_time(i, TIMEOUT_DELAY);

      break;
    }
  }
}

void check_heartbeats() {
  int i, d;
  STATUS_RESPONSE res;
  res.type = HEARTBEAT;
  res.status = SERVER_DESC.client_msgid;

  for (i = 0; i < SERVER_CAPACITY; i++) {
    d = local_client_tick_time(i);

    if (d <= 0) {
      logout_user(LOCAL_CLIENTS[i].name);
    } else
    if (d == TIMEOUT) {
      msgsnd(LOCAL_CLIENTS[i].client_msgid, &res, sizeof(res)-sizeof(long), 0);
    }
  }
}

//CLIENT_REL LOCAL_CLIENTS[SERVER_CAPACITY];
void init_local_clients() {
  int i;
  for (i = 0; i < SERVER_CAPACITY; i++) {
    LOCAL_CLIENTS[i].timeout = INT_MAX;
    strcpy(LOCAL_CLIENTS[i].room, "");
    strcpy(LOCAL_CLIENTS[i].name, "");
  }
}


void local_client_add(CLIENT desc, int msgid) {
  int i;

  printf("dodaje klienta\n");
  for (i = 0; i < SERVER_CAPACITY; i++) {
    if (LOCAL_CLIENTS[i].timeout == INT_MAX) {
      LOCAL_CLIENTS[i].timeout = TIMEOUT_DELAY;
      LOCAL_CLIENTS[i].client_msgid = msgid;
      strcpy(LOCAL_CLIENTS[i].room, desc.room);
      strcpy(LOCAL_CLIENTS[i].name, desc.name);

      break;
    }
  }
}

void local_client_remove(char * name) {
  int i;

  printf("usuwam klienta\n");
  for (i = 0; i < SERVER_CAPACITY; i++) {
    if (strcmp(LOCAL_CLIENTS[i].name, name) == 0) {
      LOCAL_CLIENTS[i].timeout = INT_MAX;
      strcpy(LOCAL_CLIENTS[i].room, "");
      strcpy(LOCAL_CLIENTS[i].name, "");

      break;
    }
  }
}

void local_client_set_time(int i, int t) {
  LOCAL_CLIENTS[i].timeout = t;
  printf("ustawiam czas\n");
}

int local_client_tick_time(int i) {
  if (LOCAL_CLIENTS[i].timeout == INT_MAX)
    return INT_MAX;
  else {
    printf("tick!\n");
    return --LOCAL_CLIENTS[i].timeout;
  }
}

void local_client_room_change(char * name, char * room) {

  printf("klient zmienia pokoj");
  int i;
  for (i = 0; i < SERVER_CAPACITY; i++) {
    if (strcmp(LOCAL_CLIENTS[i].name, name) == 0) {
      strcpy(LOCAL_CLIENTS[i].room, room);

      break;
    }
  }
}
