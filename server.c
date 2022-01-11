#include "segel.h"
#include "request.h"
#include <sys/syscall.h>
#define _GNU_SOURCE
#define CMDLINE 200
pthread_cond_t work_c;
pthread_cond_t queue_c;
pthread_mutex_t m;
int cur_queue_size = 0; //the maximum amount of requests allowed at any given moment, cur_queue_size+cur_num_jobs <= max_num_jobs
int cur_num_jobs = 0;



// a node tuple, contains the clients socket and time received
typedef struct tuple{
    int client_socket;
    struct timeval time_received;
}*tuple_t;
// queue implementation:
typedef struct node{
    struct node *next;
    int client_socket;
    struct timeval time_received;
}*node_t;

node_t head = NULL;
node_t tail = NULL;

void enqueue(int client_socket, struct timeval *time_received){
    node_t new_node = malloc(sizeof(*new_node));
    new_node->time_received.tv_sec = time_received->tv_sec;
    new_node->time_received.tv_usec = time_received->tv_usec;
    new_node->client_socket = client_socket;
    new_node->next = NULL;
    if (tail == NULL){
        head = new_node;
    }else{
        tail->next = new_node;
    }
    cur_queue_size ++;
    tail = new_node;
}

// removes first element (Head)
// returns -1 if the queue is empty
// returns the client_socket if there is one
//this need to be updated to return a tuple.
int dequeue(int *client_socket, struct timeval *time_received){
    if(head == NULL){
        return -1;
    }else{
        (*client_socket) = head->client_socket;
        (*time_received).tv_usec = head->time_received.tv_usec;
        (*time_received).tv_sec = head->time_received.tv_sec;
        node_t tmp = head;
        head = head->next;
        if(head ==  NULL){tail = NULL;}
        free(tmp);
        cur_queue_size --; // decrementing cur_num_jobs is threads responsibility
        return(0);
    }
}

// randomly remove 50% of the queue
void randomRemove()
{
    int* histogram_to_remove = malloc(cur_queue_size*sizeof(int));
    for (int i=0; i<cur_queue_size ; i++)
    {
        histogram_to_remove[i]=0;
    }
    int amount_to_remove = cur_queue_size/2 + cur_queue_size%2;
    srand(cur_queue_size);
    while(amount_to_remove > 0)
    {
        int index = rand();
        index = index%cur_queue_size;
        if(histogram_to_remove[index] == 0)
        {
            histogram_to_remove[index] = 1;
            amount_to_remove--;
        }
    }

    node_t prev = head;
    node_t cur = head;
    for(int i = 0 ; i < cur_queue_size ; i++)
    {
        if(histogram_to_remove[i]==1)
        {
            if (cur == head)
            {
                node_t tmp = head;
                head = head->next;
//                free(tmp->tup);
                free(tmp);
                cur = head; //need to advance cur and prev if we erased the head
                prev = head;
                cur_queue_size--; //we did not use deque, updating size of queue is our responsibility
                cur_num_jobs--;
                continue;
            }
            node_t tmp = cur;
            prev->next = cur->next;
            cur = cur->next;
//            free(tmp->tup);
            free(tmp);
            cur_queue_size--;
            cur_num_jobs--;
            continue;
        }
        else
        {
            if(cur != head) //if cur is head, than we advance only cur, else advance both, this if is a little unintuitive, it would be more readable if reversed
            {
                prev = prev->next;
            }
            cur = cur->next;
        }
    }
    free(histogram_to_remove);
};

// each thread activates this function for one request at a time
void* thread_function(void *arg){
    int fd;
    struct timeval time_dispatched;
    struct timeval time_elapsed;
    struct timeval time_received;
    pid_t  t_id = syscall(SYS_gettid);
    // stat_t stats = malloc(sizeof( stat_t)) ;  //is this how we should initialize? NO
    stat_t stats = malloc(sizeof(*stats)) ;  //is this how we should initialize? yes
    stats->num_requests = 0;
    stats->num_dyn = 0;
    stats->num_stat = 0;
    stats->thread_id = t_id;
//    tuple_t tup;
    while (1){
        pthread_mutex_lock(&m);
        while(dequeue(&fd, &time_received) == -1){
            pthread_cond_wait(&work_c, &m);
        }
        gettimeofday(&time_dispatched, NULL); //should we check if worked?
        pthread_mutex_unlock(&m);
//        timersub(&time_dispatched, &tup->time_received, &time_elapsed);
        stats->time_received.tv_sec = time_received.tv_sec;
        stats->time_received.tv_usec = time_received.tv_usec;
//        fprintf(stderr, "Stat-Req-Arrival:: %ld.%06ld\r\n", stats->time_received.tv_sec, stats->time_received.tv_usec);
//        fprintf(stderr, "Stat-Req-Dispatch:: %ld.%06ld\r\n", time_dispatched.tv_sec, time_dispatched.tv_usec);
        timersub(&time_dispatched, &time_received, &time_elapsed);
//        fprintf(stderr, "Stat-Req-elapsed:: %ld.%06ld\r\n", time_elapsed.tv_sec, time_elapsed.tv_usec);
//        stats->time_received.tv_usec = tup->time_received.tv_usec;
//        stats->time_received.tv_sec = tup->time_received.tv_sec;
        stats->time_elapsed.tv_sec = time_elapsed.tv_sec;
        stats->time_elapsed.tv_usec = time_elapsed.tv_usec;
//        free(tup);
        requestHandle(fd, stats);
        pthread_mutex_lock(&m);
        cur_num_jobs--;  // decrement the num of jobs in the system
        pthread_cond_signal(&queue_c); // if the main thread was waiting, awaken it
        pthread_mutex_unlock(&m);
        Close(fd);
    }
}

// Parsing the arguments
void getargs(int *port, int *threads, int *queue_size, char* schedalg, int argc, char *argv[])
{
    if (argc != 5) {
	fprintf(stderr, "Usage: %s <port> <threads> <queue_size> <schedalg>\n", argv[0]);
	exit(1);
    }
    *port = atoi(argv[1]);
    *threads = atoi(argv[2]);
    *queue_size = atoi(argv[3]);
    strcpy(schedalg, argv[4]);
}

int main(int argc, char *argv[])
{
    // for some reason, init didnt work outside..
    pthread_cond_init(&work_c, NULL);
    pthread_cond_init(&queue_c, NULL);
    pthread_mutex_init(&m, NULL);
    struct timeval time_received;
    int listenfd, connfd, port, threads_num, max_num_jobs, clientlen;
    char schedalg[CMDLINE];
    struct sockaddr_in clientaddr;
    getargs(&port, &threads_num, &max_num_jobs, schedalg, argc, argv);
    pthread_t threads[threads_num];
    for(int i=0; i<threads_num; i++){
        pthread_create(&threads[i], NULL, thread_function, NULL);
    }

    listenfd = Open_listenfd(port);

    while (1)
    {
	clientlen = sizeof(clientaddr);
	connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);
    gettimeofday(&time_received, NULL);
    // int *pfd = malloc(sizeof (int));
//    tuple_t tup = malloc(sizeof (tup)); //freed by thread_function
//    tup->client_socket = connfd;
//    tup->time_received = time_received;
    pthread_mutex_lock(&m);

    // need to recheck definition of Size
    if(cur_num_jobs == max_num_jobs) // different policies.
    {
        if(strcmp(schedalg, "block") == 0)
        {
            pthread_cond_wait(&queue_c, &m);
        }
        else if(strcmp(schedalg, "dh") == 0)
        {
            int n;
            struct timeval time;
            dequeue(&n, &time);
            cur_num_jobs--;
        }
        else if(strcmp(schedalg, "random") == 0)
        {
            randomRemove();
        }
        else if(strcmp(schedalg, "dt") == 0)
        {
            Close(connfd);
            pthread_mutex_unlock(&m);
            continue;
        }
    }//should we check that the args are legitimate?
    enqueue(connfd, &time_received);
    pthread_cond_signal(&work_c);
    pthread_mutex_unlock(&m);
    }
}
