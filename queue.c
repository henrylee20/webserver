//
// Created by henrylee on 18-12-8.
//

#include <stdio.h>
#include <malloc.h>
#include <assert.h>
#include <pthread.h>
#include "queue.h"

typedef struct list_node_s {
  void* payload;
  struct list_node_s* next;
} list_node;

// queue接口的实现(class)
typedef struct queue_backend_s {
  void (*push)(struct queue_s* self, void* payload);
  void* (*pop)(struct queue_s* self);
  int (*len)(struct queue_s* self);
  void (*destroy)(struct queue_s* self);

  list_node* head;
  list_node* tail;
  int list_len;
  int max_len;
  pthread_mutex_t lock_tail;
  pthread_mutex_t lock_head;
} queue_backend;

void print_list(queue_backend* root) {
  printf("head: %p, tail:%p, list_len: %d\n", root->head, root->tail, root->list_len);
  for (list_node* p = root->head; p; p = p->next) {
    printf("\taddr: %p, payload: %p\n", p, p->payload);
    printf("\t\t\tnext: %p\n", p->next);
  }
}

// Queue接口的实现函数
void queue_push(struct queue_s* self, void* payload);
void* queue_pop(struct queue_s* self);
int queue_len(struct queue_s* self);
void destroy_queue(queue* self);

queue* init_queue(int max_len) {
  queue_backend* q = (queue_backend*)malloc(sizeof(queue_backend));
  if (!q) {
    return NULL;
  }

  q->push = queue_push;
  q->pop = queue_pop;
  q->len = queue_len;
  q->destroy = destroy_queue;

  list_node* sentinel_node = (list_node*)malloc(sizeof(list_node));
  sentinel_node->next = NULL;
  sentinel_node->payload = NULL;

  q->head = sentinel_node;
  q->tail = sentinel_node;
  q->list_len = 0;
  q->max_len = max_len;

  if (pthread_mutex_init(&q->lock_head, NULL) < 0) {
    printf("Failed to init head lock\n");
  }
  if (pthread_mutex_init(&q->lock_tail, NULL) < 0) {
    printf("Failed to init tail lock\n");
  }

  return (queue*)q;
}

void destroy_queue(queue* self) {
  assert(self);
  queue_backend* queue_root = (queue_backend*)self;

  assert(queue_root->head);
  assert(queue_root->tail);

  while (queue_root->list_len) {
    self->pop(self);
  }

  assert(queue_root->head);
  assert(queue_root->head == queue_root->tail);

  if (pthread_mutex_destroy(&queue_root->lock_head) < 0) {
    printf("Failed to destroy head lock\n");
  }
  if (pthread_mutex_destroy(&queue_root->lock_tail) < 0) {
    printf("Failed to destroy tail lock\n");
  }

  free(queue_root->head);
  free(queue_root);
}

void queue_push(struct queue_s* self, void* payload) {
  assert(self);
  queue_backend* queue_root = (queue_backend*)self;

  list_node* node = (list_node*)malloc(sizeof(list_node));
  node->payload = payload;
  node->next = NULL;

  if (pthread_mutex_lock(&queue_root->lock_tail) < 0) {
    printf("Failed to lock queue while push\n");
    return;
  }

  if (queue_root->max_len > 0 && queue_root->max_len <= queue_root->list_len) {
    printf("Queue full\n");
    return;
  }

  assert(queue_root->tail);
  assert(!queue_root->tail->next);

  queue_root->tail->next = node;
  queue_root->tail = node;
  __sync_fetch_and_add(&queue_root->list_len, 1); // atom list_len++

  assert(queue_root->tail);
  assert(!queue_root->tail->next);

  if (pthread_mutex_unlock(&queue_root->lock_tail) < 0) {
    printf("Failed to unlock queue while push\n");
    return;
  }

}

void* queue_pop(struct queue_s* self) {
  assert(self);

  queue_backend* queue_root = (queue_backend*)self;

  assert(queue_root->head);
  list_node* data_node = queue_root->head->next;
  if (!data_node) {
    return NULL;  //empty queue
  }


  if (pthread_mutex_lock(&queue_root->lock_tail) < 0) {
    printf("Failed to lock queue while pop\n");
    return NULL;
  }

  queue_root->head->next = data_node->next;
  __sync_fetch_and_add(&queue_root->list_len, -1); // atom list_len--

  if (!queue_root->list_len) {
    queue_root->tail = queue_root->head;
  }

  if (pthread_mutex_unlock(&queue_root->lock_tail) < 0) {
    printf("Failed to unlock queue while pop\n");
    return NULL;
  }

  void* data = data_node->payload;
  data_node->next = NULL;
  free(data_node);

  return data;
}

int queue_len(struct queue_s* self) {
  assert(self);
  return ((queue_backend*)self)->list_len;
}
