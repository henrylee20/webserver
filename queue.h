//
// Created by henrylee on 18-12-8.
//

#ifndef WEBSERVER_QUEUE_H
#define WEBSERVER_QUEUE_H

typedef struct queue_s {
  // 将数据放入队列头
  void (*push)(struct queue_s* self, void* payload);
  // 从队列尾弹出数据
  void* (*pop)(struct queue_s* self);
  // 获得队列长度
  int (*len)(struct queue_s* self);
  // 析构函数
  void (*destroy)(struct queue_s* self);
} queue;

// 队列构造函数. 实例化一个队列,最大长度为max_len. 当max_len=0时,长度无限制
queue* init_queue(int max_len);

#endif //WEBSERVER_QUEUE_H
