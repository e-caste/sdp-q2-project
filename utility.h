#ifndef UTILITY_H
    #define UTILITY_H
    
    #include <stdio.h>
    #include <stdlib.h>
    #include <pthread.h>
    #include <unistd.h>
    #include <stdbool.h>
    #include <time.h>
    #include <string.h>
    #include <sys/resource.h>
    #include <stdarg.h>
    
    #define NUM_THREADS sysconf(_SC_NPROCESSORS_ONLN)

    /*
    * used in get_human_readable_time
    * US = microseconds
    * H = hour, M = minute, S = second, MS = millisecond
    */
    #define US_IN_H 3600000000
    #define US_IN_M 60000000
    #define US_IN_S 1000000
    #define US_IN_MS 1000

    /*
    * used in get_human_readable_memory_usage
    * KB = kilobytes
    * GB = gigabytes, MB = megabytes
    */
    #define KB_IN_GB 1048576  // = 1024 * 1024
    #define KB_IN_MB 1024

    // see https://iq.opengenus.org/detect-operating-system-in-c/
    // used to print the used memory correctly on macOS and GNU/Linux
    #ifdef __APPLE__
        #define MEM_SIZE 1024
    #else
        #define MEM_SIZE 1
    #endif

    //STRUCTURES used in common phase (readGraph, Label, Queries)

    typedef struct row_label {
        unsigned int* lbl_start;
        unsigned int* lbl_end;
        bool* visited;
    } row_l;

    typedef struct row_graph {
        unsigned int edge_num;   //total number of vertex in this direction
        bool not_root;
        unsigned int *edges;
        pthread_mutex_t *node_mutex;  //for parallelize 1 thread for each children in labels
    } row_g;

    typedef struct el_list_query {
        unsigned int num[2];
        bool can_reach;
    } el_query;

    typedef struct thread_args {
        unsigned int id;
        unsigned int total_vertex;
        unsigned int total_threads;
        unsigned int size_file;
        char *filename;
        row_g *graph;
        unsigned int **roots;
        unsigned int *roots_num;             //During DAG reading we count the number of roots (with protection)
        unsigned int *root_index;            //Shared Index to initialize roots array in parallel way
        pthread_mutex_t *roots_mutex;
        pthread_barrier_t *barrier;
        unsigned int queries_num;
        el_query * array_queries;
        int *node_visited;
        int num_labels;
        row_l *array_labels;
    } t_args;

    // safe asprintf used for printing statistics
    void sf_asprintf(char **strp, const char *fmt, ...);

    // safe fscanf checks for return value
    void sf_fscanf(FILE* fp, const char *fmt, ...);

    //function for statistics
    long long unsigned compute_delta_microseconds(struct timespec start, struct timespec end);
    char* get_human_readable_time(long long unsigned microseconds);
    char* get_human_readable_memory_usage(long unsigned kilobytes);
    char* get_rss_virt_mem(void);

    //function from readGraph
    void *scanFile(void *args);

    //function from solveQuery
    bool dfs_search(row_g *graph, unsigned int node1, unsigned int node2, int *visited, int num_labels, row_l *array_labels, int query_num);
    void *solveQuery (void *args);
    bool contains(int node1, int node2, int num_labels, row_l *array_labels);

    //utilities functions for build and randomize an array of roots
    void swap (unsigned int *a, unsigned int *b);
    void randomize(unsigned int *array, unsigned int n);

    //function for correct program exits
    void exitWithDealloc(bool error, unsigned int num_vertex, FILE * fp_dag, row_g *rows, pthread_t *threads, t_args *args, pthread_mutex_t *roots_mutex, unsigned int *roots, row_l *labels, FILE *fp_query, el_query *queries);
#endif //UTILITY_H