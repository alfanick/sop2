#ifndef _INCLUDE_COMMON_H
#define _INCLUDE_COMMON_H

#include <time.h>

#include <limits.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include <unistd.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/msg.h>

#include <signal.h>

#define ID_REPO 80085

#define SEM_REPO 80666

#define SEM_LOG 80777

#define SERVER_LIST_MSG_KEY 8168008

#define MAX_SERVER_NUM 20

#define SERVER_CAPACITY 20
#define MAX_CLIENTS MAX_SERVER_NUM*SERVER_CAPACITY
#define MAX_NAME_SIZE 20
#define MAX_MSG_SIZE 1024

#define TIMEOUT 5

#define LOG_FILE "/tmp/czat.log"

typedef enum MSG_TYPE {
  SERVER_LIST = 1,
  ROOM_LIST,
  CLIENT_LIST,
  LOGIN,
  LOGOUT,
  STATUS,
  HEARTBEAT,
  CHANGE_ROOM,
  PRIVATE,
  PUBLIC
} MSG_TYPE;

typedef struct CLIENT {
  char name[MAX_NAME_SIZE];
  int server_id;
  char room[MAX_NAME_SIZE];
} CLIENT;

typedef struct ROOM {
  char name[MAX_NAME_SIZE];
  int clients;
} ROOM;

typedef struct SERVER {
  int client_msgid;
  int server_msgid;
  int clients;
} SERVER;

typedef struct SERVER_LIST_REQUEST {
  long type;
  int client_msgid;
} SERVER_LIST_REQUEST;

typedef struct SERVER_LIST_RESPONSE {
  long type;
  int active_servers;
  int servers[MAX_SERVER_NUM];
  int clients_on_servers[MAX_SERVER_NUM];
} SERVER_LIST_RESPONSE;

typedef struct ROOM_LIST_RESPONSE {
  long type;
  int active_rooms;
  ROOM rooms[MAX_CLIENTS];
} ROOM_LIST_RESPONSE;

typedef struct CLIENT_LIST_RESPONSE {
  long type;
  int active_clients;
  char names[MAX_NAME_SIZE][MAX_CLIENTS];
} CLIENT_LIST_RESPONSE;

typedef struct CLIENT_REQUEST {
  long type;
  int client_msgid;
  char client_name[MAX_NAME_SIZE];
} CLIENT_REQUEST;

typedef struct CHANGE_ROOM_REQUEST {
  long type;
  int client_msgid;
  char client_name[MAX_NAME_SIZE];
  char room_name[MAX_NAME_SIZE];
} CHANGE_ROOM_REQUEST;

typedef struct STATUS_RESPONSE {
  long type;
  int status;
} STATUS_RESPONSE;

typedef struct TEXT_MESSAGE {
  long type;
  int from_id;
  char from_name[MAX_NAME_SIZE];
  char to[MAX_NAME_SIZE];
  time_t time;
  char text[MAX_MSG_SIZE];
} TEXT_MESSAGE;

typedef struct REPO {
  CLIENT clients[MAX_CLIENTS];
  ROOM rooms[20];
  SERVER servers[MAX_SERVER_NUM];
  int active_clients;
  int active_rooms;
  int active_servers;
} REPO;

int REPO_SEMAPHORE_ID;
int LOG_SEMAPHORE_ID;
int REPO_SHM_ID;
int SHARED_QUEUE_ID;
REPO * GLOBAL_REPO;

void init();
void init_repository();

void cleanup();

void semaphore_set(int i, int v);
void semaphore_up(int i);
void semaphore_down(int i);

void log_write(const char* data, ...);

void repository_lock();
void repository_unlock();

void repository_sort_servers();
void repository_sort_rooms();
void repository_sort_clients();

void repository_server_add(SERVER desc);
void repository_server_remove(int id);

void repository_room_add(ROOM desc);
void repository_room_remove(char * name);

void repository_client_add(CLIENT desc);
void repository_client_remove(char * name);

void receive_and_handle(int queue, MSG_TYPE type, void (*handler) (const void *));

#endif

