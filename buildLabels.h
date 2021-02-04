#ifndef BUILD_LABELS_H
    #define BUILD_LABELS_H
    #include "utility.h"
    
    // paper par. 3.1: multiple index construction

    typedef struct thread_labels_args {
        int lbl_num;
        int rank_node;
        row_l *labels;

        row_g *graph;
        int vertex_num;

        int *roots;
        int roots_num;
        int *indexes;           //For doing random roots visit

        pthread_mutex_t* rank_mutex;    // version 3: threads split roots
        int id;
        int threads_available;
        int * rank_node_label;
    } t_lbl_args;

    typedef struct thread_child_args {
        int id;
        int node;
        int lbl_num;
        row_l *labels;
        row_g *graph;
        int vertex_num;
        pthread_mutex_t* rank_mutex;    // version i thread for each children
        int *rank_node;
        int rank_children_min;
    } t_child_args;

    // SEQUENTIAL VERSION
    void RandomizedLabelingSequential(row_g * graph, row_l * labels, int num_label, int num_vertex, int * roots, int num_roots);
    void RandomizedVisitSequentialRecursive(int node_num, int lbl_num, row_l* labels, row_g* graph, int* rank_root, int num_vertex);

    //PARALLEL VERSION
    // version 1: A thread for each label
    void RandomizedLabelingParallelInit(row_g * graph, row_l * labels, int label_num, int vertex_num, int * roots, int roots_num, int num_threads);
    void* RandomizedLabelingParallel(void* args);
    // version 2: A thread for each children of the node
    void RandomizedVisitParallelInit(int node_num, int lbl_num, row_l* labels, row_g* graph, int* rank_root, pthread_mutex_t* rank_mutex, int num_vertex);
    void* RandomizedVisitParallel(void* args);

    // version 3: A thread for each label + many threads splits roots_num
    void* RandomizedLabelingRootsParallelInit(void* args);
    void* RandomizedLabelingRootsParallel(void* args);
    void RandomizedVisitParallelRoots(int node_num, int lbl_num, row_l* labels, row_g* graph, int* rank_root, int num_vertex, pthread_mutex_t *rank_mutex);

#endif  //BUILD_LABELS_H