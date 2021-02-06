#ifndef BUILD_LABELS_H
    #define BUILD_LABELS_H
    #include "utility.h"
    
    // paper par. 3.1: multiple index construction

    typedef struct thread_labels_args {
        int lbl_num;
        unsigned long rank_node;
        row_l *labels;

        row_g *graph;
        unsigned long vertex_num;

        unsigned long *roots;
        unsigned long roots_num;
        unsigned long *indexes;           //For doing random roots visit

        pthread_mutex_t* rank_mutex;    // version 3: threads split roots
        unsigned long id;
        unsigned int threads_available;
        unsigned long * rank_node_label;
    } t_lbl_args;

    // SEQUENTIAL VERSION
    void RandomizedLabelingSequential(row_g * graph, row_l * labels, int num_label, unsigned long num_vertex, unsigned long * roots, unsigned long num_roots);
    void RandomizedVisitSequentialRecursive(unsigned long node_num, int lbl_num, row_l* labels, row_g* graph, unsigned long* rank_root, unsigned long num_vertex);

    //PARALLEL VERSION
    // version 1: A thread for each label
    void RandomizedLabelingParallelInit(row_g * graph, row_l * labels, int label_num, unsigned long vertex_num, unsigned long * roots, unsigned long roots_num, unsigned int num_threads);
    void* RandomizedLabelingParallel(void* args);

    // version 3: A thread for each label + many threads splits roots_num
    void* RandomizedLabelingRootsParallelInit(void* args);
    void* RandomizedLabelingRootsParallel(void* args);
    void RandomizedVisitParallelRoots(unsigned long node_num, int lbl_num, row_l* labels, row_g* graph, unsigned long* rank_root, unsigned long num_vertex, pthread_mutex_t *rank_mutex);

#endif  //BUILD_LABELS_H