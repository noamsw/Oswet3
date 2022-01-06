#include "segel.h"
#include "request.h"
#define CMDLINE 200
// 
// server.c: A very, very simple web server
//
// To run:
//  ./server <portnum (above 2000)>
//
// Repeatedly handles HTTP requests sent to this port number.
// Most of the work is done within routines written in request.c
//
//queue implemetaion:
typedef struct node{
    struct node *next;
    int *client_socket;
}node_t;
node_t* head = NULL;
node_t* tail = NULL;
void enque(int *client_socket){
    node_t *newnode = malloc(sizeof(node_t));
    newnode->client_socket = client_socket;
    newnode->new = NULL;
    if (tail == NULL){
        head = newnode;
    }else{
        tail->next = newnode;
    }
    tail = newnode;
}
//returns NULL if the queue is empty
//returns the pointer to a client_socket if there is one
int* dequeeu{
    if(head == NULL){
        return NULL;
    }else{
        int *result = head->client_socket;
        node_t *tmp = head;
        head = head->next;
        if(head ==  NULL){tail = NULL;}
        free(tmp);
        return(result);
    }
}
// HW3: Parse the new arguments too
void getargs(int *port, int *threads, int *queue_size, int argc, char* schedalg, char *argv[])
{
    if (argc < 2) {
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
    cond_t c;
    mutex_t m;
    int cur_queue_size = 0;
    int listenfd, connfd, port, threads_num, queue_size, clientlen;
    char schedalg[CMDLINE];
    struct sockaddr_in clientaddr;

    getargs(&port, &threads_num, &queue_size, argc, argv);
    pthread_t threads[threads_num];
    for(int i=0; i<threads_num; i++){
        pthread_create(&threads[i], NULL, thread_function, NULL);
    }
    // 
    // HW3: Create some threads...
    //

    listenfd = Open_listenfd(port);
    while (1) {
	clientlen = sizeof(clientaddr);
	connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);

	// 
	// HW3: In general, don't handle the request in the main thread.
	// Save the relevant info in a buffer and have one of the worker threads 
	// do the work. 
	// 
	requestHandle(connfd);

	Close(connfd);
    }

}


    


 
