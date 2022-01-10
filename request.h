#ifndef __REQUEST_H__
//stat struct
typedef struct stat_t{
    pid_t thread_id; //thread id
    int *num_requests; // total number of requests, a pointer so it can be updated
    int *num_stat; // total number of static requests, a pointer so it can be updated
    int *num_dyn; // total number of dynamic requests, a pointer so it can be updated
    struct timeval time_received;
    struct timeval time_dispatched;
}*stat_t;

void requestHandle(int fd, stat_t stat);// get the strings num of request, static and dynamic by pointer. update them. also revieve the time the thread started working on it and the id

#endif
