//
// Created by henrylee on 19-3-26.
//

#ifndef HTTP_SERVER_THREAD_POOL_H
#define HTTP_SERVER_THREAD_POOL_H

#include <functional>
#include <future>
#include <atomic>
#include <vector>
#include <pthread.h>

#include "queue.h"

class ThreadPool {
private:
  enum workerStatus {
    kNotWorking = 0,
    kWorking
  };

  struct worker {
    pthread_t tid;
    ThreadPool* pool;
    int status;
    std::atomic<size_t> job_count;
    std::atomic_bool is_hope_exit;

    worker(ThreadPool* pool_ptr) :
    tid(0), pool(pool_ptr), status(kNotWorking), job_count(0), is_hope_exit(false) {}

    static void* thread_func(void* arg) {
      auto self = static_cast<worker*>(arg);

      std::function<void()> job;

      do {
        self->status = kNotWorking;

        pthread_mutex_lock(&self->pool->job_mutex);
        if (self->pool->queue.empty()) pthread_cond_wait(&self->pool->job_cond, &self->pool->job_mutex);
        bool pop_result = self->pool->queue.pop(job);
        pthread_mutex_unlock(&self->pool->job_mutex);

        if (pop_result) {
          self->status = kWorking;
          self->pool->working_num++;

          job();

          self->job_count++;
          self->pool->working_num--;
        }

      } while(!self->is_hope_exit.load(std::memory_order_acquire));
      return nullptr;
    }
  };

private:
  std::vector<std::shared_ptr<worker>> threads;
  SafeQueue<std::function<void()>> queue;

  pthread_cond_t job_cond{};
  pthread_mutex_t job_mutex{};

  std::atomic<size_t> working_num;

  bool is_shutdown;

private:

public:
  ThreadPool() : working_num(0), is_shutdown(true) {}

  ~ThreadPool() {
    shutdown();
  }

  ThreadPool(const ThreadPool&) = delete;
  ThreadPool(ThreadPool&&) = delete;

  ThreadPool& operator=(const ThreadPool&) = delete;
  ThreadPool& operator=(ThreadPool&&) = delete;

  template <typename F, typename ...Args>
  auto submit(F&& f, Args&&... args) -> std::future<decltype(f(args...))> {

    auto func = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
    auto task_ptr = std::make_shared<std::packaged_task<decltype(f(args...))()>>(func);

    auto func_caller = [task_ptr]() {
      (*task_ptr)();
    };

    queue.push(func_caller);

    pthread_cond_signal(&job_cond);

    return task_ptr->get_future();
  }

  bool init(size_t thread_num) {
    if (pthread_mutex_init(&job_mutex, nullptr)) return false;
    if (pthread_cond_init(&job_cond, nullptr)) return false;

    return increaseThreadNum(thread_num);
  }

  void shutdown() {
    is_shutdown = true;

    reduceThreadNum(threads.size());

    void* ret;
    for (auto &woker : threads) {
      pthread_join(woker->tid, &ret);
    }

    pthread_cond_destroy(&job_cond);
    pthread_mutex_destroy(&job_mutex);
  }

  size_t workingNum() {
    return working_num.load(std::memory_order_relaxed);
  }

  size_t size() {
    return threads.size();
  }

  bool increaseThreadNum(size_t num) {
    for (int i = 0; i < num; ++i) {
      threads.push_back(std::make_shared<worker>(this));
      if (pthread_create(&(threads.back()->tid), nullptr, worker::thread_func, threads.back().get())) return false;
    }
    return true;
  }

  bool reduceThreadNum(size_t num) {
    num = num > threads.size() ? threads.size() : num;

    for (; num > 0; --num) {
      threads.back()->is_hope_exit = true;
      threads.pop_back();
    }

    pthread_cond_broadcast(&job_cond);

    return true;
  }
};


#endif //HTTP_SERVER_THREAD_POOL_H
