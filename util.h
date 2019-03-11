#ifndef _UTIL_H
#define _UTIL_H

void init(int port);
int accept_connection(void);
int get_request(int fd, char *filename);
int return_result(int fd, char *content_type, char *buf, int numbytes);
int return_error(int fd, char *buf);

#endif

