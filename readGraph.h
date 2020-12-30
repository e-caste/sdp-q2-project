#ifndef READ_GRAPH_H
    #define READ_GRAPH_H
    #include "main.h"

    typedef struct row_label {
        int* lbl_start;
        int* lbl_end;
        bool* visited;
    } row_l;    //TODO e' in condivisione tra LETTURA file1 e creazione LABELS

    typedef struct edge_list {
        int num;            //valore del vertice
        struct edge_list *next_num;
    } edge;

    typedef struct row_graph {
        int edge_num;   //numero totali di vertici in quella direzione
        bool not_root;   //memset per tutto a zero
        edge *edges_pointer;
        pthread_mutex_t *node_mutex;
    } row_g;

    typedef struct el_list_query {
        int num[2];            //valore del vertice
        bool can_reach;
    } el_query; //TODO

    typedef struct thread_args {
        int id;
        int total_vertex;  // should be unsigned long
        unsigned int total_threads;
        int size_file;
        char *filename;
        row_g *graph;
        int *roots;
        int *roots_num;             //During DAG reading we count the number of roots (with protection)
        int *root_index;            //Shared Index to initialize roots array in parallel way
        pthread_mutex_t *roots_mutex;
        int queries_num;
        el_query * array_queries;
        bool *node_visited;
        int num_labels;
        row_l *array_labels;
    } t_args;

    //FUNCTIONS FOR READING THE INPUT FILE1 (.GRA)
    void create_list(edge *head, int val);
    void push(edge *head, int val);
    void print_list(edge *head);
    void *scanFile(void *args);
    void free_list(edge *head);

    //UTILITIES FUNCTION FOR BUILD AND RANDOMIZE AN ARRAY OF ROOTS
    void swap (int *a, int *b);
    void randomize(int *array, int n);
    void *scanRoots(void *args);

#endif //READ_GRAPH_H