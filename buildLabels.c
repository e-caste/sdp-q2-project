#include "buildLabels.h"

// Funzione (SEQUENZIALE-RICORSIVA) che richiama tutti i figli di un nodo per costruire le labels
void RandomizedVisitRecursive(int node_num, int lbl_num, row_l* labels, row_g* graph, int* rank_root, int num_vertex){
    int rank_children_min = num_vertex, i,children_num = graph[node_num].edge_num;
    int next_node[children_num];
    int *indexes = NULL;
    
    //printf("RandomVisited-begin: node: %i\n", node_num);

    if(labels[node_num].visited[lbl_num])
        return;

    //Per tutti i figli del nodo, richiamo la funzione
    if(children_num > 0){
        i=0;
        for(edge* next=graph[node_num].edges_pointer; next != NULL; next = next->next_num){
            next_node[i] = next->num;
            i++;
        }

        // mi preparo a randomizzare la visita dei figli del nodo X
        indexes = (int*) malloc(children_num * sizeof(int));
        if(indexes == NULL) {
            printf ("RandomizedVisit: Not enough room for these indexes\n" );
            exit(1);
        }

        for(i=0; i<children_num; i++) {
            indexes[i] = i;
        }

        randomize(indexes, children_num);

        for(int j=0; j<children_num; j++) {
            RandomizedVisitRecursive(next_node[indexes[j]], lbl_num, labels, graph, rank_root, num_vertex);
        }
    }    
    
    labels[node_num].visited[lbl_num] = true;
    
    //cerco minimo tra i figli del nodo
    for(edge* next=graph[node_num].edges_pointer; next != NULL; next = next->next_num)
        if(labels[next->num].lbl_start[lbl_num] < rank_children_min)
            rank_children_min = labels[next->num].lbl_start[lbl_num];
        
    if(*rank_root < rank_children_min)
        labels[node_num].lbl_start[lbl_num] = *rank_root;
    else 
        labels[node_num].lbl_start[lbl_num] =  rank_children_min;
    
    labels[node_num].lbl_end[lbl_num] = *rank_root;

    *rank_root = *rank_root + 1 ;

    //printf("RandomVisited-end: node: %i, lbl: %i, [%i, %i]\n", node_num, lbl_num, labels[node_num].lbl_start[lbl_num], labels[node_num].lbl_end[lbl_num]);
}

// Funzione (SEQUENZIALE) per creare N Labels e avviare la ricerca per ogni radice 
void RandomizedLabeling(row_g * graph, row_l * labels, int num_label, int num_vertex, int * roots, int num_roots){
    int i, rank_node ;

    //Inizializzazione indici
    int *indexes = malloc(num_roots*sizeof(int));
    if(indexes == NULL) {
        printf ("Not enough room for these indexes\n" );
        exit(1);
    }

    for(i=0; i<num_roots; i++) {
        indexes[i] = i;
    }

    //inizializzazione per valori random
    srand(time(NULL));

    for(i=0; i<num_label; i++){
        rank_node = 1;

        //Le radici sono gia' state cercate nel main.
        randomize(indexes, num_roots);

        for(int j=0; j<num_roots; j++) {
            RandomizedVisitRecursive(roots[indexes[j]], i, labels, graph, &rank_node, num_vertex);
        }
    }
}

//INIZIO FUNZIONI PARALLELE (PAR 3.1)
void* RandomizedVisitParallel(void* args) {
    t_child_args* my_data;
    my_data = (t_child_args*) args;
    
    //Un Thread alla volta. (Nodi condivisi richiedono protezione)
    pthread_mutex_lock(my_data->graph[my_data->node].node_mutex);
        if(my_data->labels[my_data->node].visited[my_data->lbl_num]){
            //printf("RandomizedVisitParallel: %i già visitato, termino\n", my_data->node);
            pthread_mutex_unlock(my_data->graph[my_data->node].node_mutex);
            pthread_exit((void *) 0);
        }

        if(my_data->graph[my_data->node].edge_num > 0){ //Il nodo ha altri figli
            pthread_mutex_unlock(my_data->graph[my_data->node].node_mutex);
            RandomizedVisitParallelInit(my_data->node, my_data->lbl_num, my_data->labels, my_data->graph, my_data-> rank_node, my_data->vertex_num);
        }else{ 
            //NON HO ALTRI FIGLI (SONO FOGLIA)
            my_data->labels[my_data->node].visited[my_data->lbl_num] = true;

            //cerco minimo tra i figli del nodo
            for(edge* next= my_data->graph[my_data->node].edges_pointer; next != NULL; next = next->next_num)
                if(my_data->labels[next->num].lbl_start[my_data->lbl_num] < my_data->rank_children_min )
                    my_data->rank_children_min = my_data->labels[next->num].lbl_start[my_data->lbl_num];
            
            if(*my_data->rank_node < my_data->rank_children_min)
                my_data->labels[my_data->node].lbl_start[my_data->lbl_num] = *my_data->rank_node;
            else 
                my_data->labels[my_data->node].lbl_start[my_data->lbl_num] = my_data->rank_children_min;
            
            my_data->labels[my_data->node].lbl_end[my_data->lbl_num] = *my_data->rank_node;

            *my_data->rank_node = *my_data->rank_node + 1 ;
            pthread_mutex_unlock(my_data->graph[my_data->node].node_mutex);
        }
        //printf("RandomizedVisitParallel: Sono nodo: %i, lbl[%i]: [%i, %i]\n", my_data->node, my_data->lbl_num, my_data->labels[my_data->node].lbl_start[my_data->lbl_num], my_data->labels[my_data->node].lbl_end[my_data->lbl_num]);
    pthread_exit((void *) 0);
}

void RandomizedVisitParallelInit(int node_num, int lbl_num, row_l* labels, row_g* graph, int* rank_root, int num_vertex){
    int rank_children_min = num_vertex, i,children_num = graph[node_num].edge_num, err_code;
    int next_node[children_num];
    int indexes[children_num];
    pthread_t threads_child[children_num];   //1 thread for each children of the node
    t_child_args args_child[children_num];

    //printf("Sono il nodo: %i\n", node_num);
    //Per tutti i figli del nodo, richiamo la funzione
    if(children_num > 0){
        i=0;
        for(edge* next=graph[node_num].edges_pointer; next != NULL; next = next->next_num){
            next_node[i] = next->num; //figli del nodo
            indexes[i] = i; //i=0; i<children_num;i++
            i++;
        }

        if(children_num > 1)
            randomize(indexes, children_num);

        for(i=0; i<children_num; i++) {
            args_child[i].node = next_node[indexes[i]];
            args_child[i].lbl_num = lbl_num;
            args_child[i].labels = labels;
            args_child[i].graph = graph;
            args_child[i].vertex_num = num_vertex;
            args_child[i].rank_node = rank_root;
            args_child[i].rank_children_min = num_vertex;
            err_code = pthread_create(&threads_child[i], NULL, RandomizedVisitParallel, (void *)&args_child[i]);
            if(err_code) {
                printf ("RandomizedVisitInit: Error number %i in creating thread %i.\n", err_code, i);
                exit(1);
            }
        }

        for(i=0; i<children_num; i++) { //il nodo deve aspettare tutti i suoi figli
            err_code = pthread_join(threads_child[i], NULL);
            if(err_code) {
                printf ("RandomizedVisitInit: Error number %i in joining thread for  %i.\n", err_code, i);
                exit(1);
            }
        }
    }

    //un po' di duplicazione del codice...
    pthread_mutex_lock(graph[node_num].node_mutex);
        if(labels[node_num].visited[lbl_num]){
            //printf("RandomizedVisitInit: %i già visitato, termino\n", node_num);
            pthread_mutex_unlock(graph[node_num].node_mutex);
            return;
        }

        labels[node_num].visited[lbl_num] = true;

        //cerco minimo tra i figli del nodo
        for(edge* next=graph[node_num].edges_pointer; next != NULL; next = next->next_num)
            if(labels[next->num].lbl_start[lbl_num] < rank_children_min)
                rank_children_min = labels[next->num].lbl_start[lbl_num];
            
        if(*rank_root < rank_children_min)
            labels[node_num].lbl_start[lbl_num] = *rank_root;
        else 
            labels[node_num].lbl_start[lbl_num] =  rank_children_min;
        
        labels[node_num].lbl_end[lbl_num] = *rank_root;

        *rank_root = *rank_root + 1 ;
        //printf("RandomizedVisitInit: Sono nodo: %i, lbl[%i]: [%i, %i]\n", node_num, lbl_num, labels[node_num].lbl_start[lbl_num], labels[node_num].lbl_end[lbl_num]);
    pthread_mutex_unlock(graph[node_num].node_mutex);
}

void* RandomizedLabelingParallel(void* args) {
    t_lbl_args* my_data;
    my_data = (t_lbl_args*) args;
    int indexes[my_data->roots_num];
    
    //Randomizzazione radici (ogni thread il suo)
    //Le radici sono gia' state cercate nel main.

    memcpy(indexes, my_data->indexes, my_data->roots_num*sizeof(int));
    randomize(indexes, my_data->roots_num);

    for(int j=0; j< my_data->roots_num; j++) { //TODO Parallelizzazione
        //RandomizedVisitParallelInit(my_data->roots[indexes[j]], my_data->lbl_num, my_data->labels, my_data->graph, &my_data->rank_node, my_data->vertex_num);
        RandomizedVisitRecursive(my_data->roots[indexes[j]], my_data->lbl_num, my_data->labels, my_data->graph, &my_data->rank_node, my_data->vertex_num);
    }

    pthread_exit((void *) 0);
}

// La funzione RandomizedLabelingInit prepara la struttura data per ogni
// thread - uno per label per il momento - e avvia i thread
void RandomizedLabelingParallelInit(row_g * graph, row_l * labels, int label_num, int vertex_num, int * roots, int roots_num){
    int i, err_code ;
    pthread_t threads_lbl[label_num];   //1 thread for each label
    t_lbl_args args_lbl[label_num];
    int indexes[roots_num];

    for(i=0; i<roots_num; i++){
        indexes[i] = i;
    }

    //inizializzazione per valori random
    srand(time(NULL));

    for(i=0; i<label_num; i++){
        args_lbl[i].rank_node = 1;

        args_lbl[i].lbl_num = i;
        args_lbl[i].labels = labels;    //it's a pointer so it can modify the object.
                                        //TODO volendo potrei passare direttamente: labels[lbl_num]
        args_lbl[i].graph = graph;
        args_lbl[i].vertex_num = vertex_num;
        args_lbl[i].roots = roots;
        args_lbl[i].roots_num = roots_num;
        args_lbl[i].indexes = indexes;  //struttura condivisa. -> no protezione xk lavoro su una copia

        err_code = pthread_create(&threads_lbl[i], NULL, RandomizedLabelingParallel, (void *)&args_lbl[i]);
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
