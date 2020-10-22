#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>

typedef struct edge_list {
    int num;
    struct edge_list *next_num;
} edge;

void create_list(edge *head, int val) {
    edge *current = head;
    current -> num = val;
    current -> next_num = NULL;
}

void push(edge *head, int val) {
    edge *current = head;

    while(current -> next_num != NULL) {
        current = current -> next_num;
    }

    current -> next_num = (edge *) malloc(sizeof(edge));
    current -> next_num -> num = val;  
    current -> next_num -> next_num = NULL;
}

void print_list(edge *head) {
    edge *current = head;

    while(current != NULL) {
        printf("%i ", current -> num);
        current = current -> next_num;
    }
}

void free_list(edge *head) {
    edge *tmp;

    while(head != NULL) {
        tmp = head;
        head = head -> next_num;
        free(tmp);
    }
}

typedef struct row_graph {
    int edge_num;
    edge *edges_pointer;
} row_g;

typedef struct tS{
    int tid;
} thread_struct;

FILE *fp;
row_g *rows;
sem_t* fileProtection;

void* threadRowReadingFunction(void* params){
    thread_struct * threadStruct;
    int i, j, c=0, k=0, status=0;

    threadStruct = (struct tS *) params;

    status = sem_wait(fileProtection);
    if (status != 0){
        printf("unable to wait in fileProtection semaphore\n");
        perror("errNo: ");
        exit(1);
    }

    fscanf(fp, "%i: ", &i);     //NOTA: per grafi molto grandi va bene %i? 

    edge *head;
    head = malloc(sizeof(edge));
    do {
        i = -1;
        if(k !=0) ungetc(c, fp);
        fscanf(fp, "%i ", &i);  //leggo intero successivo (scartando ": ")
        if(i != -1) {
            if(k==0) {
                create_list(head, i);
            } else {
                push(head, i);
            }
        }
        c = fgetc(fp);
        k++;
    } while(c != 35);
    if(i==-1) {
        rows[threadStruct->tid].edges_pointer = NULL;
        rows[threadStruct->tid].edge_num = 0;
    } else {
        rows[threadStruct->tid].edges_pointer = head;
        rows[threadStruct->tid].edge_num = k;
    }

    status = sem_post(fileProtection);
    if (status != 0){
        printf("unable to post fileProtection semaphore\n");
        perror("errNo: ");
        exit(1);
    }
}


int main(int argc, char *argv[]) {    
    unsigned int num_vertex;
    int t=0, status=0;
    thread_struct* pThreadStruct;
    pthread_t* thread;

    // Controllo sugli argomenti

    if (argc != 2) {
        fprintf(stderr, "For the moment, enter only one argument!\n");
        exit(1);
    }

    // Apertura file1

    fp = fopen(argv[1], "r");

    if (fp == NULL) {
        fprintf(stderr, "Unable to open file %s for reading.\n", argv[1] );
        exit(1);
    }

    fileProtection = (sem_t *) malloc(sizeof(sem_t));
	if(fileProtection == NULL){
		printf("unable to allocate memory for fileProtection semaphore\n");
		perror("errNo: ");
		exit(1);
	}

    status = sem_init(fileProtection, 0, 1);
	if(status != 0){
		printf("unable to initialzate fileProtection semaphore\n");
		perror("errNo: ");
		exit(1);
	}

    // Prendere numero vertici per allocare la giusta memoria

    fscanf(fp,"%i\n", &num_vertex);

    rows = (row_g *) malloc (num_vertex * sizeof (row_g));
    if (rows == NULL ) {
        printf ("Not enough room for this size graph\n" );
        return -1;
    }

    pThreadStruct = (thread_struct *) malloc (num_vertex * sizeof (thread_struct));
    if (pThreadStruct == NULL ) {
        printf ("Not enough room for this size graph\n" );
        return -1;
    }

    thread = (pthread_t *) malloc (num_vertex * sizeof (pthread_t));
    if (thread == NULL ) {
        printf ("Not enough room for this size graph\n" );
        return -1;
    }

    for(t=0; t<num_vertex; t++){
        pThreadStruct[t].tid = t;
        int ret = pthread_create(&thread[t], NULL, threadRowReadingFunction, (void *) &pThreadStruct[t] );
        if(ret != 0){
            printf ("Error in creating thread %i \n", t);
            return -1;
        }
    }


    for(t=0; t<num_vertex; t++){
        int ret = pthread_join(thread[t], NULL);
    }

    fclose(fp);

    // // Stampa di prova
    int i = 0;
    for(i=0; i<num_vertex; i++) {
        printf("%i : ", i);
        print_list(rows[i].edges_pointer);
        printf("\n");
    }

    // Deallocazione di tutte le risorse

    for(i=0; i<num_vertex; i++) {
        free_list(rows[i].edges_pointer);
    }
    
    status = sem_destroy(fileProtection);
	if(status != 0){
		printf("unable to destroy fileProtection semaphore\n");
		perror("errNo: ");
		exit(1);
	}

    free(fileProtection);

    free(rows);

    return 0;
}