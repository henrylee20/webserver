//
// Created by henrylee on 18-12-8.
//

#ifndef WEBSERVER_THREAD_POOL_H
#define WEBSERVER_THREAD_POOL_H

#define THREAD_JOB_READY 0
#define THREAD_JOB_RUNNING 1
#define THREAD_JOB_FINISHED 2

typedef struct {
  int id;
  unsigned int jobs_counter;
  int* is_hope_to_exit;
} thread_info;

typedef struct job_s {
  void* (*func)(void* arg, thread_info* info);
  void* job_arg;
  void (*finished_callback)(void* arg);
  void* finished_cb_arg;
  int status;
  int is_auto_destroy;
} job;


typedef struct thread_pool_s {
  // 析构函数
  void (*destroy)(struct thread_pool_s* self);

  // 设置线程默认工作
  void (*set_default_job)(struct thread_pool_s* self, job* default_job);
  // 往线程池工作队列中添加一项工作
  void (*push_job)(struct thread_pool_s* self, job* new_job);

  // 获得线程总数
  int (*get_total_thread_num)(struct thread_pool_s* self);
  // 获得工作中线程数量
  int (*get_working_thread_num)(struct thread_pool_s* self);

  // 添加线程
  void (*add_threads)(struct thread_pool_s* self, int num);
  // 移除线程
  void (*reduce_threads)(struct thread_pool_s* self, int num);
} thread_pool;

// 线程池构造函数 实例化一个线程池
thread_pool* thread_pool_init();

// 创建一个工作.  func为工作所调用的函数,此函数的参数中,arg为参数,info为线程信息.
//              arg为需要传递的参数,
//              is_auto_destroy决定工作完成后是否自动销毁这项工作
job* job_create(void* (*func)(void* arg, thread_info* info), void* arg, int is_auto_destroy);
// 销毁一个工作
void job_destroy(job* finished_job);

#endif //WEBSERVER_THREAD_POOL_H
