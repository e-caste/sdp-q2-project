#ifndef BUILD_LABELS_H
    #define BUILD_LABELS_H
    #include "main.h"
    #include "readGraph.h"
    
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
    } t_lbl_args;               //TODO to reduce space allocation, we can reuse thread_struct with extra field

    typedef struct thread_child_args {
        int id;
        int node;
        int lbl_num;
        row_l *labels;
        row_g *graph;
        int vertex_num;
        int *rank_node;
        int rank_children_min;
    } t_child_args;

    // SEQUENTIAL VERSION
    void RandomizedVisitRecursive(int node_num, int lbl_num, row_l* labels, row_g* graph, int* rank_root, int num_vertex);
    void RandomizedLabeling(row_g * graph, row_l * labels, int num_label, int num_vertex, int * roots, int num_roots);

    //PARALLEL VERSION
    void* RandomizedVisitParallel(void* args);
    void RandomizedVisitParallelInit(int node_num, int lbl_num, row_l* labels, row_g* graph, int* rank_root, int num_vertex);
    void* RandomizedLabelingParallel(void* args);
    void RandomizedLabelingParallelInit(row_g * graph, row_l * labels, int label_num, int vertex_num, int * roots, int roots_num);
#endif  //BUILD_LABELS_H