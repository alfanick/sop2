#include "common.h"

void init() {
  REPO_SEMAPHORE_ID = semget(SEM_REPO, 1, 0666 | IPC_CREAT | IPC_EXCL);

  if (REPO_SEMAPHORE_ID == -1) {
    REPO_SEMAPHORE_ID = semget(SEM_REPO, 1, 0666);
  } else {
    semaphore_set(REPO_SEMAPHORE_ID, 1);
  }

  init_repository();

  LOG_SEMAPHORE_ID = semget(SEM_LOG, 1, 0666 | IPC_CREAT | IPC_EXCL);

  if (LOG_SEMAPHORE_ID == -1) {
    LOG_SEMAPHORE_ID = semget(SEM_LOG, 1, 0666);
  } else {
    semaphore_set(LOG_SEMAPHORE_ID, 1);
  }

  SHARED_QUEUE_ID = msgget(SERVER_LIST_MSG_KEY, 0666 | IPC_CREAT | IPC_EXCL);

  if (SHARED_QUEUE_ID == -1) {
    SHARED_QUEUE_ID = msgget(SERVER_LIST_MSG_KEY, 0666);
  }
}

void init_repository() {
repository_lock();

  REPO_SHM_ID = shmget(ID_REPO, sizeof(REPO), 0666 | IPC_CREAT | IPC_EXCL);

  int b = 1;
  if (REPO_SHM_ID == -1) {
    REPO_SHM_ID = shmget(ID_REPO, sizeof(REPO), 0666);
    b = 0;
  }

  GLOBAL_REPO = shmat(REPO_SHM_ID, NULL, 0);

  if (b) {
    GLOBAL_REPO->active_clients = 0;
    GLOBAL_REPO->active_rooms = 0;

    int i;
    for (i = 0; i < 20; i++) {
      strcpy(GLOBAL_REPO->rooms[i].name, "~~~~~");
    }
  }

repository_unlock();
}

void cleanup() {
  shmdt(&GLOBAL_REPO);
}

void semaphore_set(int i, int v) {
  semctl(i, 0, SETVAL, v);
}

void semaphore_up(int i) {
  struct sembuf s;
  s.sem_num = 0;
  s.sem_op = 1;
  s.sem_flg = 0;

  semop(i, &s, 1);
}

void semaphore_down(int i) {
  struct sembuf s;
  s.sem_num = 0;
  s.sem_op = -1;
  s.sem_flg = 0;

  semop(i, &s, 1);
}

void repository_lock() {
  semaphore_down(REPO_SEMAPHORE_ID);
}

void repository_unlock() {
  semaphore_up(REPO_SEMAPHORE_ID);
}

int _servers_sort(const SERVER * a, const SERVER * b) {
  if (a->server_msgid < b->server_msgid)
    return -1;

  if (a->server_msgid > b->server_msgid)
    return 1;

  return 0;
}

void repository_sort_servers() {
  qsort(GLOBAL_REPO->servers, GLOBAL_REPO->active_servers, sizeof(SERVER), (int (*)(const void *, const void *))_servers_sort);
}

int _clients_sort(const CLIENT * a, const CLIENT * b) {
  return strcmp(a->name, b->name);
}

void repository_sort_clients() {
  qsort(GLOBAL_REPO->clients, GLOBAL_REPO->active_clients, sizeof(CLIENT), (int (*)(const void *, const void *))_clients_sort);
}

int _rooms_sort(const ROOM * a, const ROOM * b) {
  return strcmp(a->name, b->name);
}

void repository_sort_rooms() {
  qsort(GLOBAL_REPO->rooms, GLOBAL_REPO->active_rooms, sizeof(ROOM), (int (*)(const void *, const void *))_rooms_sort);
}

void repository_server_add(SERVER desc) {
  GLOBAL_REPO->servers[GLOBAL_REPO->active_servers] = desc;

  GLOBAL_REPO->active_servers++;

  repository_sort_servers();
}

void repository_client_add(CLIENT desc) {
  GLOBAL_REPO->clients[GLOBAL_REPO->active_clients] = desc;

  GLOBAL_REPO->active_clients++;

  repository_sort_clients();
}

void repository_room_add(ROOM desc) {
  GLOBAL_REPO->rooms[GLOBAL_REPO->active_rooms] = desc;

  GLOBAL_REPO->active_rooms++;

  repository_sort_rooms();
}

void repository_server_remove(int id) {
  int i;
  for (i = 0; i < GLOBAL_REPO->active_servers; i++) {
    if (GLOBAL_REPO->servers[i].server_msgid == id) {
      GLOBAL_REPO->servers[i].server_msgid = INT_MAX;
      break;
    }
  }

  repository_sort_servers();

  GLOBAL_REPO->active_servers--;
}

void repository_client_remove(char * name) {
  int i;
  for (i = 0; i < GLOBAL_REPO->active_clients; i++) {
    if (strcmp(GLOBAL_REPO->clients[i].name, name) == 0) {
      strcpy(GLOBAL_REPO->clients[i].name, "~~~~~");
      break;
    }
  }

  repository_sort_clients();

  GLOBAL_REPO->active_clients--;
}

void repository_room_remove(char * name) {
  int i;
  for (i = 0; i < GLOBAL_REPO->active_rooms; i++) {
    if (strcmp(GLOBAL_REPO->rooms[i].name, name) == 0) {
      strcpy(GLOBAL_REPO->rooms[i].name, "~~~~~");
      break;
    }
  }

  repository_sort_rooms();

  GLOBAL_REPO->active_rooms--;
}

void log_lock() {
  semaphore_down(LOG_SEMAPHORE_ID);
}

void log_unlock() {
  semaphore_up(LOG_SEMAPHORE_ID);
}

void log_write(const char* data, ...) {
log_lock();

  FILE *log = fopen(LOG_FILE, "at");

  if (!log) {
    log = fopen(LOG_FILE, "wt");
  }

  va_list args;
  va_start(args, data);
  vfprintf(log, data, args);
  va_end(args);

  fclose(log);

log_unlock();
}

void receive_and_handle(int queue, MSG_TYPE type, void (*handler) (const void *)) {
  void * out = malloc(8192);

  if (msgrcv(queue, out, 8192, (long)type, IPC_NOWAIT) != -1) {
    handler(out);
  }
}

