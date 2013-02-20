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

  if (REPO_SHM_ID == -1) {
    REPO_SHM_ID = shmget(ID_REPO, sizeof(REPO), 0666);
  }

  GLOBAL_REPO = shmat(REPO_SHM_ID, NULL, 0);

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
  int i;
  for (i = 0; i < GLOBAL_REPO->active_servers; i++) {
    printf("%d ", GLOBAL_REPO->servers[i].server_msgid);
  }
  printf("\n");
}

void repository_server_add(SERVER desc) {
  GLOBAL_REPO->servers[GLOBAL_REPO->active_servers] = desc;

  GLOBAL_REPO->active_servers++;

  repository_sort_servers();
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

void log_lock() {
  semaphore_down(LOG_SEMAPHORE_ID);
}

void log_unlock() {
  semaphore_up(LOG_SEMAPHORE_ID);
}

void log_write() {
  log_lock();



  log_unlock();
}
