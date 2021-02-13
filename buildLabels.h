#ifndef BUILD_LABELS_H
    #define BUILD_LABELS_H
    #include "utility.h"
    
    // paper par. 3.1: multiple index construction

    typedef struct thread_labels_args {
        int lbl_num;
        unsigned int rank_node;
        row_l *labels;

        row_g *graph;
        unsigned int vertex_num;

        unsigned int *roots;
        unsigned int roots_num;
        unsigned int *indexes;           //For doing random roots visit

        pthread_mutex_t* rank_mutex;    // version 3: threads split roots
        unsigned int id;
        unsigned int threads_available;
        unsigned int * rank_node_label;
    } t_lbl_args;

    // SEQUENTIAL VERSION
    void RandomizedLabelingSequential(row_g * graph, row_l * labels, int num_label, unsigned int num_vertex, unsigned int * roots, unsigned int num_roots);
    void RandomizedVisitSequentialRecursive(unsigned int node_num, int lbl_num, row_l* labels, row_g* graph, unsigned int* rank_root, unsigned int num_vertex);

    //PARALLEL VERSION
    // version 1: A thread for each label
    void RandomizedLabelingParallelInit(row_g * graph, row_l * labels, int label_num, unsigned int vertex_num, unsigned int * roots, unsigned int roots_num, unsigned int num_threads);
    void* RandomizedLabelingParallel(void* args);

    // version 3: A thread for each label + many threads splits roots_num
    void* RandomizedLabelingRootsParallelInit(void* args);
    void* RandomizedLabelingRootsParallel(void* args);
    void RandomizedVisitParallelRoots(unsigned int node_num, int lbl_num, row_l* labels, row_g* graph, unsigned int* rank_root, unsigned int num_vertex, pthread_mutex_t *rank_mutex);

#endif  //BUILD_LABELS_H