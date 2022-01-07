#include "segel.h"
#include "request.h"

#define CMDLINE 200
pthread_cond_t* work_c;
pthread_cond_t* queue_c;
pthread_mutex_t m;
int cur_queue_size = 0;

//queue implemetaion:
typedef struct node{
    struct node *next;
    int *client_socket;
}node_t;
node_t* head = NULL;
node_t* tail = NULL;
void enqueue(int *client_socket){
    node_t *newnode = malloc(sizeof(node_t));
    newnode->client_socket = client_socket;
    newnode->next = NULL;
    if (tail == NULL){
        head = newnode;
    }else{
        tail->next = newnode;
    }
    cur_queue_size ++;
    tail = newnode;
}
//returns NULL if the queue is empty
//returns the pointer to a client_socket if there is one
int* dequeue(){
    if(head == NULL){
        return NULL;
    }else{
        int *result = head->client_socket;
        node_t *tmp = head;
        head = head->next;
        if(head ==  NULL){tail = NULL;}
        free(tmp);
        cur_queue_size --;
        return(result);
    }
}
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

    node_t* prev = head;
    node_t* cur = head;
    for(int i = 0 ; i < cur_queue_size ; i++)
    {
        if(histogram_to_remove[i]==1)
        {
            if (cur == head)
            {
                node_t* tmp = head;
                head = head->next;
                free(tmp->client_socket);
                free(tmp);
                continue;
            }
            node_t* tmp = cur;
            prev->next = cur->next;
            cur = cur->next;
            free(tmp->client_socket);
            free(tmp);
            continue;
        }
        else
        {
            if(cur != head)
            {
                prev = prev->next;
            }
            cur = cur->next;
        }
    }
};
void* thread_function(void *arg){
    while (1){
        int *pclient;
        pthread_mutex_lock(&m);
        while((pclient = dequeue()) == NULL){
            pthread_cond_wait(&work_c, &m);
        }
        pthread_cond_signal(&queue_c);
        pthread_mutex_unlock(&m);
        int fd = *pclient;
        free(pclient);
        requestHandle(fd);
        Close(fd);
    }
}
// HW3: Parse the new arguments too
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
    pthread_cond_init(work_c, NULL);
    pthread_cond_init(queue_c, NULL);
    pthread_mutex_init(&m, NULL);
    int listenfd, connfd, port, threads_num, max_queue_size, clientlen;
    char schedalg[CMDLINE];
    struct sockaddr_in clientaddr;

    getargs(&port, &threads_num, &max_queue_size, schedalg, argc, argv);
    pthread_t threads[threads_num];
    for(int i=0; i<threads_num; i++){
        pthread_create(&threads[i], NULL, thread_function, NULL);
    }

    listenfd = Open_listenfd(port);

    while (1)
    // for(int i = 0 ; i < 100; i++)
    {
	clientlen = sizeof(clientaddr);
	connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);
    int *pfd = malloc(sizeof (int));
    *pfd = connfd;

    pthread_mutex_lock(&m);
    if(cur_queue_size == max_queue_size) // different policies
    {
        if(strcmp(schedalg, "block") == 0)
        {
            pthread_cond_wait(&queue_c, &m);
        }
        else if(strcmp(schedalg, "drop_head") == 0)
        {
            int* to_free;
            to_free = dequeue();
            free(to_free);
        }
        else if(strcmp(schedalg, "drop_random") == 0)
        {

        }


    }

    enqueue(pfd);
    pthread_cond_signal(&work_c);
    pthread_mutex_unlock(&m);
    }
}
