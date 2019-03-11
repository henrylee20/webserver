//
// Created by henrylee on 18-12-8.
//

#include <stdio.h>
#include <stdarg.h>
#include <pthread.h>
#include "logger.h"

FILE* log_file = NULL;
int log_level = LOGGER_WARNING;
pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;

void logger_init(const char* filename) {
  if (log_file) {
    return;
  }

  log_file = fopen(filename, "a+");
  if (!log_file) {
    printf("Logger init error. Cannot open log file.");
    return;
  }
}

void logger_set_level(int level) {
  if (level < LOGGER_DEBUG || level > LOGGER_ERROR) {
    return;
  }
  log_level = level;
}

void logger_debug(const char* msg, ...) {
  if (pthread_mutex_lock(&log_lock) < 0) {
    printf("Logger lock error\n");
  }

  if (log_level <= LOGGER_DEBUG) {
    va_list args;

    va_start(args, msg);
    vprintf(msg, args);
    va_end(args);


    va_start(args, msg);
    if (log_file) {
      vfprintf(log_file, msg, args);
    }
    va_end(args);
  }
  if (pthread_mutex_unlock(&log_lock) < 0) {
    printf("Logger unlock error\n");
  }
}
void logger_info(const char* msg, ...) {
  if (pthread_mutex_lock(&log_lock) < 0) {
    printf("Logger lock error\n");
  }

  if (log_level <= LOGGER_INFO) {
    va_list args;

    va_start(args, msg);
    vprintf(msg, args);
    va_end(args);

    va_start(args, msg);
    if (log_file) {
      vfprintf(log_file, msg, args);
    }
    va_end(args);
  }
  if (pthread_mutex_unlock(&log_lock) < 0) {
    printf("Logger unlock error\n");
  }
}

void logger_warning(const char* msg, ...) {
  if (pthread_mutex_lock(&log_lock) < 0) {
    printf("Logger lock error\n");
  }

  if (log_level <= LOGGER_WARNING) {
    va_list args;

    va_start(args, msg);
    vprintf(msg, args);
    va_end(args);

    va_start(args, msg);
    if (log_file) {
      vfprintf(log_file, msg, args);
    }
    va_end(args);
  }
  if (pthread_mutex_unlock(&log_lock) < 0) {
    printf("Logger unlock error\n");
  }
}

void logger_error(const char* msg, ...) {
  if (pthread_mutex_lock(&log_lock) < 0) {
    printf("Logger lock error\n");
  }

  if (log_level <= LOGGER_ERROR) {
    va_list args;

    va_start(args, msg);
    vprintf(msg, args);
    va_end(args);

    va_start(args, msg);
    if (log_file) {
      vfprintf(log_file, msg, args);
    }
    va_end(args);
  }

  if (pthread_mutex_unlock(&log_lock) < 0) {
    printf("Logger unlock error\n");
  }
}
