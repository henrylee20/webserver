//
// Created by henrylee on 18-12-8.
//

#include <malloc.h>
#include <pthread.h>
#include <assert.h>
#include "logger.h"
#include "queue.h"
#include "thread_pool.h"

// private type and func declare
#define THREAD_WORKING 0
#define THREAD_NOT_WORKING 1

// 线程池接口的实现
typedef struct thread_pool_impl_s {
  void (*destroy)(struct thread_pool_s* self);

  void (*set_default_job)(struct thread_pool_s* self, job* default_job);
  void (*push_job)(struct thread_pool_s* self, job* new_job);

  int (*get_total_thread_num)(struct thread_pool_s* self);
  int (*get_working_thread_num)(struct thread_pool_s* self);

  void (*add_threads)(struct thread_pool_s* self, int num);
  void (*reduce_threads)(struct thread_pool_s* self, int num);

  job* default_job;
  queue* job_queue;
  pthread_cond_t job_cond;
  pthread_mutex_t job_mutex;

  queue* worker_queue;
  int working_num;

  unsigned int worker_id_record;
} thread_pool_impl;

// 线程池接口的函数实现
void destroy(struct thread_pool_s* self);
void set_default_job(struct thread_pool_s* self, job* default_job);
void push_job(struct thread_pool_s* self, job* new_job);
int  get_total_thread_num(struct thread_pool_s* self);
int  get_working_thread_num(struct thread_pool_s* self);
void add_threads(struct thread_pool_s* self, int num);
void reduce_threads(struct thread_pool_s* self, int num);

typedef struct {
  pthread_t tid;
  thread_pool_impl* pool;
  int status;
  int is_hope_exit;
  int jobs_count;
  unsigned int worker_id;
} work_thread;

// func impl

// 空工作函数,用于触发线程退出
void* empty_func(void* arg, thread_info* info) {
  return ((void*)0);
}

void worker_thread_func_cleanup(void* arg) {
  work_thread* info = (work_thread*)arg;
  logger_debug("[thread %lu] exiting\n", info->worker_id);
  free(info);
}

// 线程池线程的函数,每个线程都是调用这个函数,在这个函数里再获得具体的工作,调用工作函数,完成需要的工作
void* worker_thread_func(void* arg) {
  work_thread* info = (work_thread*)arg;
  thread_pool_impl* pool = info->pool;
  queue* job_queue = pool->job_queue;

  job* new_job = NULL;

  pthread_cleanup_push(worker_thread_func_cleanup, info);

  do {
    // 1. set status to not working
    info->status = THREAD_NOT_WORKING;

    // 2. run default job or try to get a job from job queue
    if (pool->default_job) {
      new_job = pool->default_job;
      new_job->status = THREAD_JOB_READY;
    } else {
      do {
        pthread_mutex_lock(&pool->job_mutex);
        while (job_queue->len(job_queue) == 0) pthread_cond_wait(&pool->job_cond, &pool->job_mutex);
        new_job = job_queue->pop(job_queue);
        pthread_mutex_unlock(&pool->job_mutex);
      } while (!new_job);
    }

    // 3. set status to working
    assert(new_job);
    assert(new_job->status == THREAD_JOB_READY || new_job == pool->default_job);

    thread_info self_info;
    self_info.id = info->worker_id;
    self_info.jobs_counter = info->jobs_count;
    self_info.is_hope_to_exit = &info->is_hope_exit;

    info->status = THREAD_WORKING;
    new_job->status = THREAD_JOB_RUNNING;
    __sync_fetch_and_add(&(pool->working_num), 1);

    // 4. do the job.
    new_job->func(new_job->job_arg, &self_info);
    if (new_job->finished_callback) {
      new_job->finished_callback(new_job->finished_cb_arg);
    }
    __sync_fetch_and_add(&(info->jobs_count), 1);

    // 5. clean job data after finished job
    __sync_fetch_and_add(&(pool->working_num), -1);
    new_job->status = THREAD_JOB_FINISHED;
    if (new_job->is_auto_destroy) {
      job_destroy(new_job);
    }
    new_job = NULL;

    // 6. back to 1 if not being hoped to exit
    pthread_testcancel();
  } while (!info->is_hope_exit);

  //logger_info("[worker %lu] exiting\n", pthread_self());
  pthread_cleanup_pop(0);
  worker_thread_func_cleanup(info);
  pthread_exit((void*)0);
}

// 线程池构造函数
thread_pool* thread_pool_init() {
  thread_pool_impl* pool = (thread_pool_impl*)malloc(sizeof(thread_pool_impl));

  // set public funcs
  pool->destroy = destroy;
  pool->set_default_job = set_default_job;
  pool->push_job = push_job;
  pool->get_total_thread_num = get_total_thread_num;
  pool->get_working_thread_num = get_working_thread_num;
  pool->add_threads = add_threads;
  pool->reduce_threads = reduce_threads;

  // init private members
  pool->default_job = NULL;

  pool->working_num = 0;
  pool->worker_queue = init_queue(0);

  pool->job_queue = init_queue(0);
  pthread_mutex_init(&pool->job_mutex, NULL);
  pthread_cond_init(&pool->job_cond, NULL);

  pool->worker_id_record = 0;

  return (thread_pool*)pool;
}

// 线程池析构函数
void destroy(struct thread_pool_s* self) {
  thread_pool_impl* pool = (thread_pool_impl*)self;

  int worker_num = pool->worker_queue->len(pool->worker_queue);
  pool->reduce_threads(self, worker_num);

  pthread_cond_destroy(&pool->job_cond);
  pthread_mutex_destroy(&pool->job_mutex);
  pool->job_queue->destroy(pool->job_queue);

  pool->worker_queue->destroy(pool->worker_queue);

  pool->default_job = NULL;

  free(pool);
}

void set_default_job(struct thread_pool_s* self, job* default_job) {
  thread_pool_impl* pool = (thread_pool_impl*)self;
  pool->default_job = default_job;
  pool->default_job->is_auto_destroy = 0;
}

void push_job(struct thread_pool_s* self, job* new_job) {
  thread_pool_impl* pool = (thread_pool_impl*)self;

  pthread_mutex_lock(&pool->job_mutex);
  pool->job_queue->push(pool->job_queue, new_job);
  pthread_mutex_unlock(&pool->job_mutex);
  pthread_cond_signal(&pool->job_cond);
}

int  get_total_thread_num(struct thread_pool_s* self) {
  thread_pool_impl* pool = (thread_pool_impl*)self;
  return pool->worker_queue->len(pool->worker_queue);
}

int  get_working_thread_num(struct thread_pool_s* self) {
  thread_pool_impl* pool = (thread_pool_impl*)self;
  return pool->working_num;
}

void add_threads(struct thread_pool_s* self, int num) {
  thread_pool_impl* pool = (thread_pool_impl*)self;
  work_thread* worker;
  for (; num > 0; num--) {
    worker = (work_thread*)malloc(sizeof(work_thread));
    worker->is_hope_exit = 0;
    worker->status = THREAD_NOT_WORKING;
    worker->pool = pool;
    worker->jobs_count = 0;
    worker->worker_id = __sync_fetch_and_add(&(pool->worker_id_record), 1);

    pool->worker_queue->push(pool->worker_queue, worker);

    if (pthread_create(&worker->tid, NULL, worker_thread_func, (void*)worker) < 0) {
      printf("create thread err\n");
      return;
    }
  }
}

void reduce_threads(struct thread_pool_s* self, int num) {
  thread_pool_impl* pool = (thread_pool_impl*)self;
  queue* worker_queue = pool->worker_queue;

  logger_debug("reduce thread, num is %d, thread left: %d\n", num, worker_queue->len(worker_queue));
  if (worker_queue->len(worker_queue) < num) {
    num = worker_queue->len(worker_queue);
  }

  for (; num > 0; num--) {
    work_thread* worker = (work_thread*)worker_queue->pop(worker_queue);
    // set exit flag so worker will exit after finished last job
    __sync_fetch_and_add(&(worker->is_hope_exit), 1);
    //pthread_cancel(worker->tid);
    logger_debug("Send exit to [thread %lu], flag %d, thread left: %d\n", worker->worker_id, worker->is_hope_exit, worker_queue->len(worker_queue));

    // try to let every worker do a empty job. so the thread a receive exit flag
    for (int i = 0; i < num; i++) {
      pool->push_job(self, job_create(empty_func, NULL, 1));
    }
  }
}

job* job_create(void* (*func)(void* arg, thread_info* info), void* arg, int is_auto_destroy) {
  job* new_job = (job*)malloc(sizeof(job));
  new_job->func = func;
  new_job->job_arg = arg;
  new_job->finished_callback = NULL;
  new_job->finished_cb_arg = NULL;
  new_job->status = THREAD_JOB_READY;
  new_job->is_auto_destroy = is_auto_destroy;
  return new_job;
}

void job_destroy(job* finished_job) {
  if (finished_job) {
    free(finished_job);
  }
}
