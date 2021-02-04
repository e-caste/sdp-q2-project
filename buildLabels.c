#include "buildLabels.h"

//SEQUENTIAL FUNCTION

// RandomizedLabelingSequential is the SEQUENTIAL version used for creating N Labels
void RandomizedLabelingSequential(row_g * graph, row_l * labels, int num_label, int num_vertex, int * roots, int num_roots){
    int i, rank_node ;
    int indexes[num_roots];

    for(i=0; i<num_roots; i++) {
        indexes[i] = i;
    }

    //Inizializzation random value for roots randomization
    srand(time(NULL));

    for(i=0; i<num_label; i++){
        rank_node = 1;

        //roots are provided by the main.
        randomize(indexes, num_roots);

        for(int j=0; j<num_roots; j++) {
            RandomizedVisitSequentialRecursive(roots[indexes[j]], i, labels, graph, &rank_node, num_vertex);
        }
    }
}

void RandomizedVisitSequentialRecursive(int node_num, int lbl_num, row_l* labels, row_g* graph, int* rank_root, int num_vertex){
    int rank_children_min = num_vertex, i,children_num = graph[node_num].edge_num;
    int indexes[children_num];

    if(labels[node_num].visited[lbl_num])
        return;

    //For each children of the node, recall the function
    if(children_num > 0){
        i=0;

        for(i=0; i<children_num; i++) {
            indexes[i] = i;
        }

        randomize(indexes, children_num);

        for(int j=0; j<children_num; j++) {//for each children in random order
            RandomizedVisitSequentialRecursive(graph[node_num].edges[indexes[j]], lbl_num, labels, graph, rank_root, num_vertex);
        }
    }    
    
    labels[node_num].visited[lbl_num] = true;
    
    //searchg min value between children node
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
void RandomizedLabelingParallelInit(row_g * graph, row_l * labels, int label_num, int vertex_num, int * roots, int roots_num, int num_threads){
    int i, err_code ;
    pthread_t threads_lbl[label_num];   //1 thread for each label
    t_lbl_args args_lbl[label_num];
    int indexes[roots_num];

    // Scan Roots it once here.
    // In threads code use a shadow created with memcpy -> should be faster
    for(i=0; i<roots_num; i++){
        indexes[i] = i;
    }

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
        args_lbl[i].indexes = indexes;  //*shared* structure -> no protection because work in a shadow copy
        args_lbl[i].threads_available = (num_threads/label_num);

        //MODIFIED VERSION: 3 THREAD FOR EACH LABELS + SOME SUB-THREADS SPLITTING ROOTS
        err_code = pthread_create(&threads_lbl[i], NULL, RandomizedLabelingRootsParallelInit, (void *)&args_lbl[i]);

        //ORIGINAL VERSION: 1 THREAD FOR EACH LABELS
        //err_code = pthread_create(&threads_lbl[i], NULL, RandomizedLabelingParallel, (void *)&args_lbl[i]);
        if(err_code) {
            printf ("RandomizedLabelingInit: Error number %i in creating thread %i.\n", err_code, i);
            exit(1);
        }
    }

    for(i=0; i<label_num; i++) {
        err_code = pthread_join(threads_lbl[i], NULL);
        if(err_code) {
            printf ("RandomizedLabelingInit: Error number %i in joining thread for  %i.\n", err_code, i);
            exit(1);
        }
    }
}

// RandomizedLabelingParallel is the PARALLEL version used for creating N Labels
void* RandomizedLabelingParallel(void* args) {
    t_lbl_args* my_data;
    my_data = (t_lbl_args*) args;
    int indexes[my_data->roots_num];
    pthread_mutex_t rank_mutex = PTHREAD_MUTEX_INITIALIZER;     // protection of rank_node in version 1 thread for each children only
    
    // roots randomization (each thread its own)
    // roots are provided by the main
    // here we work in a shadow copy of original roots
    memcpy(indexes, my_data->indexes, my_data->roots_num*sizeof(int));
    randomize(indexes, my_data->roots_num);

    for(int j=0; j< my_data->roots_num; j++) {
        //RandomizedVisitParallelInit(my_data->roots[indexes[j]], my_data->lbl_num, my_data->labels, my_data->graph, &my_data->rank_node, &rank_mutex, my_data->vertex_num);
        RandomizedVisitSequentialRecursive(my_data->roots[indexes[j]], my_data->lbl_num, my_data->labels, my_data->graph, &my_data->rank_node, my_data->vertex_num);
    }

    pthread_exit((void *) 0);
}

void* RandomizedLabelingRootsParallelInit(void* args) {   //1 thread for each label
    t_lbl_args* my_data;
    my_data = (t_lbl_args*) args;
    int i=0, err_code=0;

    int indexes[my_data->roots_num];
    pthread_mutex_t rank_mutex = PTHREAD_MUTEX_INITIALIZER;
    
    // roots randomization (each thread its own)
    // roots are provided by the main
    // here we work in a shadow copy of original roots
    memcpy(indexes, my_data->indexes, my_data->roots_num*sizeof(int));
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
                printf ("RandomizedLabelingRootsParallelInit: Error number %i in creating thread %i.\n", err_code, i);
                exit(1);
            }
        }

        for(i=0; i<my_data->threads_available; i++) {
            err_code = pthread_join(threads_lbl[i], NULL);
            if(err_code) {
                printf ("RandomizedLabelingRootsParallelInit: Error number %i in joining thread for  %i.\n", err_code, i);
                exit(1);
            }
        }
    }else{
        for(int j=0; j< my_data->roots_num; j++) {
            //version 2: 1 sub-thread for each children
            //RandomizedVisitParallelInit(my_data->roots[indexes[j]], my_data->lbl_num, my_data->labels, my_data->graph, &my_data->rank_node, &rank_mutex, my_data->vertex_num);
            
            //version 1: no threads
            RandomizedVisitSequentialRecursive(my_data->roots[indexes[j]], my_data->lbl_num, my_data->labels, my_data->graph, &my_data->rank_node, my_data->vertex_num);
        }
    }

    pthread_exit((void *) 0);
}

void* RandomizedLabelingRootsParallel(void* args) { // some sub-threads splitting roots
    t_lbl_args* my_data;
    my_data = (t_lbl_args*) args;

    int inf, sup, j;

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

void RandomizedVisitParallelRoots(int node_num, int lbl_num, row_l* labels, row_g* graph, int* rank_root, int num_vertex, pthread_mutex_t *rank_mutex){
    int rank_children_min = num_vertex, i;
    int children_num = graph[node_num].edge_num;
    int indexes[children_num];

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
}



void RandomizedVisitParallelInit(int node_num, int lbl_num, row_l* labels, row_g* graph, int* rank_root, pthread_mutex_t* rank_mutex, int num_vertex){
    int rank_children_min = num_vertex, i,children_num = graph[node_num].edge_num, err_code;
    int indexes[children_num];
    pthread_t threads_child[children_num];   //1 thread for each children of the node
    t_child_args args_child[children_num];

    // For each child of the node, recall the function
    if(children_num > 0){
        i=0;

        for(i=0; i<children_num; i++)
            indexes[i] = i;

        if(children_num > 1)
            randomize(indexes, children_num);

        for(i=0; i<children_num; i++) {
            args_child[i].node = graph[node_num].edges[indexes[i]];
            args_child[i].lbl_num = lbl_num;
            args_child[i].labels = labels;
            args_child[i].graph = graph;
            args_child[i].vertex_num = num_vertex;
            args_child[i].rank_node = rank_root;
            args_child[i].rank_mutex = rank_mutex;
            args_child[i].rank_children_min = num_vertex;
            err_code = pthread_create(&threads_child[i], NULL, RandomizedVisitParallel, (void *)&args_child[i]);
            if(err_code) {
                printf ("RandomizedVisitInit: Error number %i in creating thread %i.\n", err_code, i);
                exit(1);
            }
        }

        for(i=0; i<children_num; i++) { //The node has to wait all children
            err_code = pthread_join(threads_child[i], NULL);
            if(err_code) {
                printf ("RandomizedVisitInit: Error number %i in joining thread for  %i.\n", err_code, i);
                exit(1);
            }
        }
    }

    pthread_mutex_lock(graph[node_num].node_mutex);
        if(labels[node_num].visited[lbl_num]){
            pthread_mutex_unlock(graph[node_num].node_mutex);
            return;
        }

        labels[node_num].visited[lbl_num] = true;

        pthread_mutex_lock(rank_mutex);
            //searching min value between children node
            for(i=0; i<children_num; i++){
                if(labels[graph[node_num].edges[i]].lbl_start[lbl_num] < rank_children_min)
                    rank_children_min = labels[graph[node_num].edges[i]].lbl_start[lbl_num];
            }

            if(*rank_root < rank_children_min)
                labels[node_num].lbl_start[lbl_num] = *rank_root;
            else
                labels[node_num].lbl_start[lbl_num] =  rank_children_min;
        
            labels[node_num].lbl_end[lbl_num] = *rank_root;

            *rank_root = *rank_root + 1 ;
        pthread_mutex_unlock(rank_mutex);
    pthread_mutex_unlock(graph[node_num].node_mutex);
}

void* RandomizedVisitParallel(void* args) {
    t_child_args* my_data;
    my_data = (t_child_args*) args;
    
    //One Thread at time. (Shared nodes require protection!)
    pthread_mutex_lock(my_data->graph[my_data->node].node_mutex);
        if(my_data->labels[my_data->node].visited[my_data->lbl_num]){
            pthread_mutex_unlock(my_data->graph[my_data->node].node_mutex);
            pthread_exit((void *) 0);
        }

        if(my_data->graph[my_data->node].edge_num > 0){ //The node has other child
            pthread_mutex_unlock(my_data->graph[my_data->node].node_mutex);
            RandomizedVisitParallelInit(my_data->node, my_data->lbl_num, my_data->labels, my_data->graph, my_data-> rank_node, my_data->rank_mutex, my_data->vertex_num);
        }else{ 
            //NO OTHER CHILD (LEAF)
            my_data->labels[my_data->node].visited[my_data->lbl_num] = true;

            pthread_mutex_lock(my_data->rank_mutex);
                //searching min value between children node
                for(int i=0; i< my_data->graph[my_data->node].edge_num; i++){
                    if(my_data->labels[my_data->graph[my_data->node].edges[i]].lbl_start[my_data->lbl_num] < my_data->rank_children_min)
                        my_data->rank_children_min = my_data->labels[my_data->graph[my_data->node].edges[i]].lbl_start[my_data->lbl_num];
                }

                if(*my_data->rank_node < my_data->rank_children_min)
                    my_data->labels[my_data->node].lbl_start[my_data->lbl_num] = *my_data->rank_node;
                else 
                    my_data->labels[my_data->node].lbl_start[my_data->lbl_num] = my_data->rank_children_min;
                
                my_data->labels[my_data->node].lbl_end[my_data->lbl_num] = *my_data->rank_node;

                *my_data->rank_node = *my_data->rank_node + 1 ;
            pthread_mutex_unlock(my_data->rank_mutex);
        pthread_mutex_unlock(my_data->graph[my_data->node].node_mutex);
        }
    pthread_exit((void *) 0);
}

