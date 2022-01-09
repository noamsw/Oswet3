#include "segel.h"
#include "request.h"

#define CMDLINE 200
pthread_cond_t work_c;
pthread_cond_t queue_c;
pthread_mutex_t m;
int cur_queue_size = 0;

// a node tuple, contains the clients socket and time received
typedef struct tuple{
    int client_socket;
    struct timeval time_received;
}*tuple_t;
// queue implementation:
typedef struct node{
    struct node *next;
    tuple_t tup;
}*node_t;

node_t head = NULL;
node_t tail = NULL;

void enqueue(tuple_t tup){
    node_t new_node = malloc(sizeof(node_t));
    new_node->tup = tup;
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
//this need to be updated to reutn a tuple.
tuple_t dequeue(){
    if(head == NULL){
        return NULL;
    }else{
        tuple_t tup = head->tup;
        node_t tmp = head;
        head = head->next;
        if(head ==  NULL){tail = NULL;}
        free(tmp);
        cur_queue_size --;
        return(tup);
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
                free(tmp->tup);
                free(tmp);
                cur = head; //need to advance cur and prev if we erased the head
                prev = head;
                continue;
            }
            node_t tmp = cur;
            prev->next = cur->next;
            cur = cur->next;
            free(tmp->tup);
            free(tmp);
            continue;
        }
        else
        {
            if(cur != head) //if cur is head, than we advance only cur, else advance both, this if is a little unintuitive
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
    int num_requests = 0;
    int num_stat = 0;
    int num_dyn = 0;
    struct timeval time_dispatched;
    pid_t  t_id = gettid();
    while (1){
        tuple_t tup;
        pthread_mutex_lock(&m);
        while((tup = dequeue()) == NULL){
            pthread_cond_wait(&work_c, &m);
        }
        gettimeofday(&time_dispatched, NULL); //should we check if worked?
        pthread_cond_signal(&queue_c);
        pthread_mutex_unlock(&m);
        int fd = tup->client_socket;
        stat_t stat;
        stat->time_received = tup->time_received;
        stat->time_dispatched = time_dispatched;
        stat->num_requests = &num_requests;
        stat->num_stat = &num_stat;
        stat->num_dyn = &num_dym;
        stat->thread_id = t_id;
        free(tup);
        requestHandle(fd, stat);
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
    gettimeofday(&time_received, NULL);
    // int *pfd = malloc(sizeof (int));
    tuple_t tup = malloc(sizeof (tup)); //freed by thread_function
    tup->client_socket = pfd;
    tup->time_received = time_received;
    pthread_mutex_lock(&m);

    // need to recheck definition of Size
    if(cur_queue_size == max_queue_size) // different policies.
    {
        if(strcmp(schedalg, "block") == 0)
        {
            pthread_cond_wait(&queue_c, &m);
        }
        else if(strcmp(schedalg, "drop_head") == 0)
        {
            dequeue();
        }
        else if(strcmp(schedalg, "drop_random") == 0)
        {
            randomRemove();
        }
    }
    enqueue(tup);
    pthread_cond_signal(&work_c);
    pthread_mutex_unlock(&m);
    }
}
