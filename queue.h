//
// Created by henrylee on 19-3-6.
//

#ifndef HTTP_SERVER_QUEUE_H
#define HTTP_SERVER_QUEUE_H

#include <atomic>

template <typename T>
class SafeQueue {
private:
  struct node {
    std::atomic<node*> next;
    T data;

    node() : next(nullptr) {}
    node(T const & val) : next(nullptr), data(val) {
      //TODO avoid ABA
    }
  };

private:
  std::atomic<node*> head;
  std::atomic<node*> tail;
  std::atomic<size_t> len;

public:
  SafeQueue() : len(0) {
    node* dummy_node = new node();
    head.store(dummy_node, std::memory_order_relaxed);
    tail.store(dummy_node, std::memory_order_release);
  }

  SafeQueue(SafeQueue&) = delete;
  SafeQueue(SafeQueue&&) = delete;

  ~SafeQueue() {
    T dummy;
    while (pop(dummy));
    delete head.load(std::memory_order_relaxed);
  }

  size_t size() {
    return len.load(std::memory_order_relaxed);
  }

  bool empty() {
    return head.load() == tail.load();
  }

  bool push(T const & val) {
    node* new_node = new node(val);

    for (;;) {
      node* tail_ptr = tail.load(std::memory_order_acquire);
      node* next_ptr = tail_ptr->next.load(std::memory_order_acquire);

      node* tail_ptr2 = tail.load(std::memory_order_acquire);

      if (tail_ptr == tail_ptr2) {
        if (next_ptr == nullptr) {
          if (tail_ptr->next.compare_exchange_weak(next_ptr, new_node)) {
            tail.compare_exchange_strong(tail_ptr, new_node);
            return true;
          }
        } else {
          tail.compare_exchange_strong(tail_ptr, next_ptr);
        }
      }
    }
  }

  bool pop(T& ret) {
    for (;;) {
      node* head_ptr = head.load(std::memory_order_acquire);
      node* tail_ptr = tail.load(std::memory_order_acquire);
      node* next_ptr = head_ptr->next.load(std::memory_order_acquire);

      node* head_ptr2 = head.load(std::memory_order_acquire);

      if (head_ptr == head_ptr2) {
        if (head_ptr == tail_ptr) {
          if (next_ptr == nullptr) {
            return false;
          }
          tail.compare_exchange_strong(tail_ptr, next_ptr);
        } else {
          ret = next_ptr->data;

          if (head.compare_exchange_weak(head_ptr, next_ptr)) {
            delete head_ptr;
            return true;
          }
        }
      }
    }
  }
};

#endif //HTTP_SERVER_QUEUE_H
