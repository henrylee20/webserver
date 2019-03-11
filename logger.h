//
// Created by henrylee on 18-12-8.
//

#ifndef WEBSERVER_LOGGER_H
#define WEBSERVER_LOGGER_H

#define LOGGER_DEBUG 0
#define LOGGER_INFO 1
#define LOGGER_WARNING 2
#define LOGGER_ERROR 3

// 初始化logger
void logger_init(const char* filename);
// 设置logger等级
void logger_set_level(int level);

// logger输出
void logger_debug(const char* msg, ...);
void logger_info(const char* msg, ...);
void logger_warning(const char* msg, ...);
void logger_error(const char* msg, ...);


#endif //WEBSERVER_LOGGER_H
