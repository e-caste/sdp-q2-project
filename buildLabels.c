#include "buildLabels.h"

//SEQUENTIAL FUNCTION

// RandomizedLabelingSequential is the SEQUENTIAL version used for creating N Labels
void RandomizedLabelingSequential(row_g * graph, row_l * labels, int num_label, unsigned int num_vertex, unsigned int * roots, unsigned int num_roots){
    unsigned int i, j, rank_node ;
    unsigned int* indexes = (unsigned int *)malloc(num_roots*sizeof(unsigned int));
    
    for(i=0; i<num_roots; i++) {
        indexes[i] = i;
    }

    //Inizializzation random value for roots randomization
    srand(time(NULL));

    for(i=0; i<num_label; i++){
        rank_node = 1;

        //roots are provided by the main.
        randomize(indexes, num_roots);

        for(j=0; j<num_roots; j++) {
            RandomizedVisitSequentialRecursive(roots[indexes[j]], i, labels, graph, &rank_node, num_vertex);
        }
    }

    if(indexes)
        free(indexes);
}

void RandomizedVisitSequentialRecursive(unsigned int node_num, int lbl_num, row_l* labels, row_g* graph, unsigned int* rank_root, unsigned int num_vertex){
    unsigned int rank_children_min = num_vertex, i, j, children_num = graph[node_num].edge_num;
    unsigned int indexes[children_num];

    if(labels[node_num].visited[lbl_num])
        return;

    //For each children of the node, recall the function
    if(children_num > 0){
        i=0;

        for(i=0; i<children_num; i++) {
            indexes[i] = i;
        }

        randomize(indexes, children_num);

        for(j=0; j<children_num; j++) {//for each children in random order
            RandomizedVisitSequentialRecursive(graph[node_num].edges[indexes[j]], lbl_num, labels, graph, rank_root, num_vertex);
        }
    }    
    
    labels[node_num].visited[lbl_num] = true;
    
    //searching min value between children node
    for(i=0; i<children_num; i++)
        if(labels[graph[node_num].edges[i]].lbl_start[lbl_num] < rank_children_min)
            rank_children_min = labels[graph[node_num].edges[i]].lbl_start[lbl_num];
        
    if(*rank_root < rank_children_min)
        labels[node_num].lbl_start[lbl_num] = *rank_root;
    else 
        labels[node_num].lbl_start[lbl_num] =  rank_children_min;
    
    labels[node_num].lbl_end[lbl_num] = *rank_root;

    *rank_root = *rank_root + 1 ;
}

//PARALLEL FUNCTION

// RandomizedLabelingParallelInit prepares data structure for each thread and run them
void RandomizedLabelingParallelInit(row_g * graph, row_l * labels, int label_num, unsigned int vertex_num, unsigned int * roots, unsigned int roots_num, unsigned int num_threads){
    int err_code ;
    unsigned int i;
    pthread_t threads_lbl[label_num];   //1 thread for each label
    t_lbl_args args_lbl[label_num];

    //Inizializzation random value for roots randomization
    srand(time(NULL));

    for(i=0; i<label_num; i++){
        args_lbl[i].rank_node = 1;

        args_lbl[i].lbl_num = i;
        args_lbl[i].labels = labels;    //it's a pointer so it can modify the object.
        args_lbl[i].graph = graph;
        args_lbl[i].vertex_num = vertex_num;
        args_lbl[i].roots = roots;
        args_lbl[i].roots_num = roots_num;
        args_lbl[i].threads_available = (num_threads/label_num); //todo some test for decide if let scheduler decide is better

        //MODIFIED VERSION: 3 THREAD FOR EACH LABELS + SOME SUB-THREADS SPLITTING ROOTS
        //err_code = pthread_create(&threads_lbl[i], NULL, RandomizedLabelingRootsParallelInit, (void *)&args_lbl[i]);

        //ORIGINAL VERSION: 1 THREAD FOR EACH LABELS
        err_code = pthread_create(&threads_lbl[i], NULL, RandomizedLabelingParallel, (void *)&args_lbl[i]);
        if(err_code) {
            printf ("RandomizedLabelingInit: Error number %i in creating thread %u.\n", err_code, i);
            exit(1);
        }
    }

    for(i=0; i<label_num; i++) {
        err_code = pthread_join(threads_lbl[i], NULL);
        if(err_code) {
            printf ("RandomizedLabelingInit: Error number %i in joining thread for %u.\n", err_code, i);
            exit(1);
        }
    }
}

// RandomizedLabelingParallel is the PARALLEL version used for creating N Labels
void* RandomizedLabelingParallel(void* args) {
    t_lbl_args* my_data;
    my_data = (t_lbl_args*) args;    
    unsigned int* indexes = (unsigned int *)malloc(my_data->roots_num*sizeof(unsigned int));

    // roots randomization (each thread its own)
    // roots are provided by the main
    for(unsigned int i=0; i<my_data->roots_num; i++){
        indexes[i] = i;
    }

    randomize(indexes, my_data->roots_num);

    for(int j=0; j< my_data->roots_num; j++) {
        RandomizedVisitSequentialRecursive(my_data->roots[indexes[j]], my_data->lbl_num, my_data->labels, my_data->graph, &my_data->rank_node, my_data->vertex_num);
    }

    if(indexes)
        free(indexes);

    pthread_exit((void *) 0);
}

void* RandomizedLabelingRootsParallelInit(void* args) {   //1 thread for each label
    t_lbl_args* my_data;
    my_data = (t_lbl_args*) args;
    int err_code=0;
    unsigned int i=0, j=0;
    pthread_mutex_t rank_mutex = PTHREAD_MUTEX_INITIALIZER;
    unsigned int* indexes = (unsigned int *)malloc(my_data->roots_num*sizeof(unsigned int));

    // roots randomization (each thread its own)
    // roots are provided by the main
    // here we work in a shadow copy of original roots
    memcpy(indexes, my_data->indexes, my_data->roots_num*sizeof(unsigned int));
    randomize(indexes, my_data->roots_num);

    if(my_data->threads_available > 1){//1 thread for each label + will run remaining thread splitting roots
        pthread_t threads_lbl[my_data->threads_available];
        t_lbl_args args_lbl[my_data->threads_available];

        for(i=0; i<my_data->threads_available; i++){
            args_lbl[i].rank_node_label = &my_data->rank_node;

            args_lbl[i].lbl_num = my_data->lbl_num;
            args_lbl[i].labels = my_data->labels;
            args_lbl[i].graph = my_data->graph;
            args_lbl[i].vertex_num = my_data->vertex_num;
            args_lbl[i].roots = my_data->roots;
            args_lbl[i].roots_num = my_data->roots_num;
            args_lbl[i].indexes = indexes; //already randomized

            args_lbl[i].threads_available = my_data->threads_available;
            args_lbl[i].id = i;
            args_lbl[i].rank_mutex = &rank_mutex;

            err_code = pthread_create(&threads_lbl[i], NULL, RandomizedLabelingRootsParallel, (void *)&args_lbl[i]);
            if(err_code) {
                printf ("RandomizedLabelingRootsParallelInit: Error number %i in creating thread %u.\n", err_code, i);
                exit(1);
            }
        }

        for(i=0; i<my_data->threads_available; i++) {
            err_code = pthread_join(threads_lbl[i], NULL);
            if(err_code) {
                printf ("RandomizedLabelingRootsParallelInit: Error number %i in joining thread for  %u.\n", err_code, i);
                exit(1);
            }
        }
    }else{
        for(j=0; j< my_data->roots_num; j++) {
            RandomizedVisitSequentialRecursive(my_data->roots[indexes[j]], my_data->lbl_num, my_data->labels, my_data->graph, &my_data->rank_node, my_data->vertex_num);
        }
    }

    if(indexes)
        free(indexes);

    pthread_exit((void *) 0);
}

void* RandomizedLabelingRootsParallel(void* args) { // some sub-threads splitting roots
    t_lbl_args* my_data;
    my_data = (t_lbl_args*) args;
    unsigned int inf, sup, j;

    if (my_data->id == my_data->threads_available - 1)
        sup = my_data->roots_num;
    else
        sup = ((my_data->roots_num) / (my_data->threads_available)) * (my_data->id + 1);

    if (my_data->id == 0)
        inf = 0;
    else
        inf = ((my_data->roots_num)/ (my_data->threads_available)) * (my_data->id);

    for(j=inf; j<sup; j++) {//if two roots are distance enough, they will proceed separatelly otherwise mutex
        RandomizedVisitParallelRoots(my_data->roots[my_data->indexes[j]], my_data->lbl_num, my_data->labels, my_data->graph, my_data->rank_node_label, my_data->vertex_num, my_data->rank_mutex);
    }

    pthread_exit((void *) 0);
}

void RandomizedVisitParallelRoots(unsigned int node_num, int lbl_num, row_l* labels, row_g* graph, unsigned int* rank_root, unsigned int num_vertex, pthread_mutex_t *rank_mutex){
    unsigned int rank_children_min = num_vertex, i;
    unsigned int children_num = graph[node_num].edge_num;
    unsigned int* indexes = (unsigned int *)malloc(children_num*sizeof(unsigned int));

    pthread_mutex_lock(graph[node_num].node_mutex);
    
    if(labels[node_num].visited[lbl_num]) {
        pthread_mutex_unlock(graph[node_num].node_mutex);
        return;
    }

    //For each children of the node, recall the function
    if(children_num > 0){
        i=0;
        
        for(i=0; i<children_num; i++) {
            indexes[i] = i;
        }

        randomize(indexes, children_num);

        for(int j=0; j<children_num; j++) {//for each children in random order
            RandomizedVisitParallelRoots(graph[node_num].edges[indexes[j]], lbl_num, labels, graph, rank_root, num_vertex, rank_mutex);
        }
    }
    
    labels[node_num].visited[lbl_num] = true;

        pthread_mutex_lock(rank_mutex);
        
        //searching min value between children node
        for(i=0; i<children_num; i++)
            if(labels[graph[node_num].edges[i]].lbl_start[lbl_num] < rank_children_min)
                rank_children_min = labels[graph[node_num].edges[i]].lbl_start[lbl_num];
            
        if(*rank_root < rank_children_min)
            labels[node_num].lbl_start[lbl_num] = *rank_root;
        else 
            labels[node_num].lbl_start[lbl_num] =  rank_children_min;
        
        labels[node_num].lbl_end[lbl_num] = *rank_root;

        *rank_root = *rank_root + 1 ;

        pthread_mutex_unlock(rank_mutex);

    pthread_mutex_unlock(graph[node_num].node_mutex);

    if(indexes)
        free(indexes);

}


