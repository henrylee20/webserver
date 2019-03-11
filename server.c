#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/time.h>
#include <time.h>
#include <stdbool.h>
#include <unistd.h>
#include "util.h"
#include "queue.h"
#include "logger.h"
#include "thread_pool.h"
#include "cache.h"

#define MAX_THREADS 100
#define MAX_queue_len 100
#define MAX_CE 100
#define INVALID -1
#define BUFF_SIZE 1024

typedef struct request_queue {
   int fd;
   char filename[BUFF_SIZE];
} request_t;

typedef struct resource_s {
  char filename[BUFF_SIZE];
  char* data;
  int data_len;
} resource;

void resource_free(void* target) {
  resource* p = (resource*)target;
  free(p->data);
  free(p);
}

// global vars
cache* resource_cache;
thread_pool* workers_pool;
thread_pool* dispatcher_pool;
int working_worker_num = 0;

queue* request_queue;
pthread_mutex_t requests_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t requests_cond = PTHREAD_COND_INITIALIZER;

void * dynamic_pool_size_update(void *arg) {
  thread_pool* worker_pool = (thread_pool*)arg;
  int total = worker_pool->get_total_thread_num(worker_pool);
  int working = working_worker_num;
  float working_rate = (float)working / total;

  const float k_busy = 0.8;
  const float k_idle = 0.2;
  const int k_min_worker_num = 10;

  sleep(1);
  logger_debug("Manager Online\n");
  while(1) {
    if (working_rate > k_busy) {
      worker_pool->add_threads(worker_pool, total);
      logger_info("Created %d workers threads because more than %.2f%% workers are busy\n", total, k_busy * 100);
    } else if (total > k_min_worker_num && working_rate < k_idle) {
      worker_pool->reduce_threads(worker_pool, total / 2);
      logger_info("Removed %d workers threads because more than %.2f%% workers are idle, and num of workers > %d.\n",
          total / 2, k_idle * 100, k_min_worker_num);
    }
    total = worker_pool->get_total_thread_num(worker_pool);
    working = working_worker_num;
    working_rate = (float)working / total;
  }
}

int readFromDisk(const char* filename, char* buf, int buf_size) {
  FILE* fp = fopen(filename, "rb");
  if (!fp) {
    return -1;
  }

  int read_len = fread(buf, 1, buf_size, fp);

  fclose(fp);

  return read_len;
}

int getFileSize(const char* filename) {
  struct stat statbuf;
  stat(filename, &statbuf);
  return statbuf.st_size;
}

const char* const k_type_jpeg = "image/jpeg";
const char* const k_type_gif = "image/gif";
const char* const k_type_html = "text/html";
const char* const k_type_plain = "text/plain";

char* getContentType(char * filename) {
  char* extension = filename;

  while (*extension) extension++;
  while (extension >= filename && *extension != '.') extension--;

  if (!strcmp(extension, ".jpg")) {
    return k_type_jpeg;
  } else if (!strcmp(extension, ".gif")) {
    return k_type_gif;
  } else if (!strcmp(extension, ".html") || !strcmp(extension, ".htm")) {
    return k_type_html;
  } else {
    return k_type_plain;
  }
}

long getCurrentTimeInMicro() {
  struct timeval curr_time;
  gettimeofday(&curr_time, NULL);
  return curr_time.tv_sec * 1000000 + curr_time.tv_usec;
}


void * dispatch(void *arg, thread_info* info) {
  logger_debug("[dispatch %lu] start.\n", info->id);

    // Accept client connection
    int fd = accept_connection();
    if (fd < 0) {
      return NULL;
    }
  logger_debug("[dispatch %lu] accepted. fd:%d\n", info->id, fd);

    // Get request from the client
    request_t* request = (request_t*)malloc(sizeof(request_t));
    memset(request, 0, sizeof(request_t));
    request->fd = fd;

    if (get_request(fd, request->filename)) {
      // TODO count err request
      logger_debug("[dispatch %lu]cannot get request.\n", info->id);
      return NULL;
    }
  logger_debug("[dispatch %lu] got a request: %s.\n", info->id, request->filename);

    // Add the request into the queue
    pthread_mutex_lock(&requests_mutex);
    request_queue->push(request_queue, request);
    pthread_mutex_unlock(&requests_mutex);
    pthread_cond_signal(&requests_cond);

   return NULL;
}

void * worker(void *arg, thread_info* info) {
  logger_debug("[worker %lu] start.\n", info->id);

    // Get the request from the queue
    request_t* request = NULL;
    pthread_mutex_lock(&requests_mutex);
    while (!request) {
      pthread_cond_wait(&requests_cond, &requests_mutex);
      request = (request_t *) request_queue->pop(request_queue);
    }
    pthread_mutex_unlock(&requests_mutex);

    if (*info->is_hope_to_exit) {
    // Add the request back to the queue
      pthread_mutex_lock(&requests_mutex);
      request_queue->push(request_queue, request);
      pthread_mutex_unlock(&requests_mutex);
      pthread_cond_signal(&requests_cond);
      return NULL;
    }

  logger_debug("[worker %lu] got a request: %s.\n", info->id, request->filename);

    int fd = request->fd;
    char* filename = request->filename;
    filename++;   // skip '/' at the head

    // Start recording time
    long start_time = getCurrentTimeInMicro();
    __sync_fetch_and_add(&working_worker_num, 1);

    // Get the data from the disk or the cache
    const char* is_cache_hit_info;
    resource* data = (resource*)resource_cache->get(resource_cache, filename);

    if (data == NULL) {
      // from the disk
      logger_debug("[worker %lu] cache miss: %s.\n", info->id, filename);
      is_cache_hit_info = "MISS";

      data = (resource*)malloc(sizeof(resource)); // buffer will free at cache.
      memset(data, 0, sizeof(resource));

      strcpy(data->filename, filename);
      int file_size = getFileSize(data->filename);
      data->data = (char*)malloc(file_size);
      memset(data->data, 0, sizeof(file_size));

      logger_debug("[worker %lu] file: %s, len: %d\n", info->id, data->filename, file_size);

      data->data_len = readFromDisk(data->filename, data->data, file_size);

      resource_cache->push(resource_cache, data->filename, data);
    } else {
      // from the cathe
      logger_debug("[worker %lu] cache hit: %s.\n", info->id, request->filename);
      is_cache_hit_info = "HIT";
    }
    free(request);

    // Stop recording the time
    __sync_fetch_and_add(&working_worker_num, -1);
    long stop_time = getCurrentTimeInMicro();

    // Log the request into the file and terminal`
    // return the result
    if (data->data_len < 0) {
      logger_info("[%lu][%lu][%d][%s][%s][%ld us][%s]\n",
          info->id, info->jobs_counter + 1, fd, data->filename, "404 Not Found", (stop_time - start_time), is_cache_hit_info);
      return_error(fd, "404 Not Found");
    } else {
      logger_info("[%lu][%lu][%d][%s][%d][%ld us][%s]\n",
          info->id, info->jobs_counter + 1, fd, data->filename, data->data_len, (stop_time - start_time), is_cache_hit_info);
      return_result(fd, getContentType(data->filename), data->data, data->data_len);
    }
  return NULL;
}

int filename_cmp(void* filename1, void* filename2) {
  if (!filename1 || !filename2)
    return 0;
  return !strcmp((char*)filename1, (char*)filename2);
}

int main(int argc, char **argv) {

  if(argc != 8){
    printf("usage: %s port path num_dispatcher num_workers dynamic_flag queue_length cache_size\n", argv[0]);
    return -1;
  }

  unsigned short port = atoi(argv[1]);
  const char* root_path = argv[2];
  int num_dispatcher = atoi(argv[3]);
  int num_workers = atoi(argv[4]);
  int dynamic_flag = atoi(argv[5]);
  int queue_len = atoi(argv[6]);
  int cache_size = atoi(argv[7]);

  logger_init("web_server_log");
  logger_set_level(LOGGER_INFO);

  request_queue = init_queue(queue_len);

  chdir(root_path);

  char pwd[BUFF_SIZE] = {0};
  getcwd(pwd, BUFF_SIZE);
  logger_debug("port: %d, root:%s, dispatcher: %d, worker: %d, catch size: %d\n",
      port, pwd, num_dispatcher, num_workers, cache_size);

  init(port);
  resource_cache = cache_lru_init(cache_size, resource_free, filename_cmp);  // cache use LRU.

  job* dispatcher_job = job_create(dispatch, NULL, 0);
  dispatcher_pool = thread_pool_init();
  dispatcher_pool->set_default_job(dispatcher_pool, dispatcher_job);
  dispatcher_pool->add_threads(dispatcher_pool, num_dispatcher);

  job* worker_job = job_create(worker, NULL, 0);
  workers_pool = thread_pool_init();
  workers_pool->set_default_job(workers_pool, worker_job);
  workers_pool->add_threads(workers_pool, num_workers);

  if (dynamic_flag) {
    pthread_t manager_thread;
    pthread_create(&manager_thread, NULL, dynamic_pool_size_update, workers_pool);
  }

  logger_debug("Server started at port %d, root is %s\n", port, pwd);
  logger_debug("Enter a 'q' to exit.\n");

  char ch;
  do {
    ch = getchar();
  } while (ch == 'q');

  logger_debug("Server cleaning up data.\n");
  job_destroy(worker_job);
  workers_pool->destroy(workers_pool);
  job_destroy(dispatcher_job);
  dispatcher_pool->destroy(dispatcher_pool);
  resource_cache->destroy(resource_cache);

  request_queue->destroy(request_queue);

  logger_debug("Server exited.\n");
  return 0;
}
