#include "q2.h"

#define NUM_THREADS sysconf(_SC_NPROCESSORS_ONLN)

// TODO: https://en.wikipedia.org/wiki/C_data_types
//      Ottimizzazione delle memoria: sostituire int con short se possibile
//      Quanti nodi al massimo? unsigned int: [0, 65,535] ; unsigned long int: [0, 4,294,967,295]

// TODO: dividere q2.c in più file (es: main.c, read.c, label.c, query.c)

typedef struct edge_list {
    int num;            //valore del vertice
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
    int edge_num;   //numero totali di vertici in quella direzione
    bool not_root;   //memset per tutto a zero
    edge *edges_pointer;
} row_g;

// 3.1 multiple index construction
typedef struct row_label {
    int* lbl_start;
    int* lbl_end;
    bool* visited;
} row_l;

typedef struct thread_args {
    int id;
    int total_vertex;
    int size_file;
    char *filename;
    row_g *graph;
    int *roots;
    int *roots_num;             //During DAG reading we count the number of roots (with protection)
    int *root_index;            //Shared Index to initialize roots array in parallel way
    pthread_mutex_t *roots_mutex;
} t_args;

typedef struct thread_labels_args {
    int lbl_num;
    int rank_node;
    row_l *labels;

    row_g *graph;
    int vertex_num;

    int *roots;
    int roots_num;
    int * indexes;          //For doing random roots
} t_lbl_args;               //TODO to reduce space allocation, we can reuse thread_struct with extra field


//Swap e randomize sono funzioni di utilità per randomizzare Roots
void swap (int *a, int *b) {
    int temp = *a;
    *a = *b;
    *b = temp;
}

void randomize(int *array, int n) {
    for(int i=n-1; i>0; i--) {
        int j = rand() % (i+1);
        if (i != j)
            swap(&array[i], &array[j]);
    }
}

// Scopo di "scanFile" : Leggere file1
//ogni thread leggerà da inf a sup.
//Inf e Sup sono il numero di riga del file di input 0, 1, ..., 10,...
//Nel seguente codice, si sta per dividere il file da leggere
//in N parti, così che ogni thread legga in parallelo
//durante la lettura del grafo, si memorizzano alcune informazioni
//necessarie per la creazione successiva delle labels (roots_num)

void *scanFile(void *args) {
    t_args *my_data;
    my_data = (t_args *) args;
    FILE *fp;
    int j, i, k, c, pos;
    int sup, inf;
    bool not_root_is_set;

    fp = fopen(my_data -> filename, "r");
    if (fp == NULL) {
        fprintf(stderr, "Unable to open file %s for reading, thread number %i.\n", my_data -> filename, my_data -> id );
        exit(1);
    }

    //Definizione estremo superiore
    if (my_data->id == NUM_THREADS-1) 
        sup = my_data->total_vertex;
    else {
        //con fseek mi metto in un punto casuale all'interno della riga
        //successivamente controllo finchè non trovo '\n' (10)
        //prendendo un carattere alla volta e "scartandolo"
        //quando trovo la nuova riga come primo intero avrò il SUP
        //NOTA: questa un'ottima strategia in quanto abbiamo righe non omogenee
        // sup for id=0 is inf for id=1, they are contiguous so the whole file is guaranteed to be read
        fseek(fp, (long) my_data->size_file*(my_data->id+1)/NUM_THREADS, SEEK_SET); // 1Gb / 4Thread = 250Mb ciascuno * id+1
        while(i != 10) {
            i = getc(fp);
        }

        i=0;

        fscanf(fp, "%i", &sup);
    }

    //Definizione estremo inferiore
    if (my_data->id == 0) {
        fseek(fp, 0L, SEEK_SET);
        fscanf(fp, "%*[^\n]\n");    // salto la prima riga (contiene il numero di vertici totale)
        inf = 0;
    } else {
        fseek(fp, (long) my_data->size_file*my_data->id/NUM_THREADS, SEEK_SET); // 1Gb / 4Thread = 250Mb ciascuno * id

        while(i != 10) {    
            i = getc(fp);
        }

        pos = ftell(fp);            //conservo la posizione di inizio riga

        fscanf(fp, "%i", &inf);     // leggo l'indice della riga

        fseek(fp, pos, SEEK_SET);   //torno all'inizio della riga
    }    

    //printf("Thread n. %i con inf: %i e sup: %i\n", my_data->id, inf, sup-1);
    
    //adesso per le righe di competenza del thread, si legge il valore del nodo
    //si costruisce e inizializza la struttura apposita e si inserisce in lista
    for(j=inf; j<sup; j++) {
        fscanf(fp, "%i: ", &i);     //scarto il numero della riga (== j) e anche ": "
        edge *head;
        head = malloc(sizeof(edge));
        do {
            i = -1;
            if(k !=0) //alla prima iterazione k==0 
                ungetc(c, fp);    //Poichè prima ho letto un carattere che non era "#" (altrimenti usciva dal ciclo) devo tornare indietro!!
            int res = fscanf(fp, "%i ", &i);      // leggo sia il valore che lo spazio. Se ci fosse un "#" non legge nulla, perchè si aspetta un %i
            if(i != -1) { // i vertici non possono essere negativi però meglio testare "res" TODO

                not_root_is_set = my_data->graph[i].not_root ? true : false;  // default == 0 == false;

                if(k==0) {
                    create_list(head, i);
                } else {
                    push(head, i);
                }
                my_data -> graph[i].not_root = true;   //se era a uno lo rimetto a 1

                if ((!not_root_is_set) && (my_data->graph[i].not_root)){
                    pthread_mutex_lock(my_data->roots_mutex);
                        *my_data->roots_num = (*(my_data->roots_num) - 1);
                    pthread_mutex_unlock(my_data->roots_mutex);
                }
            }
            c = fgetc(fp);              //prendo carattere successivo
            k++;
        } while(c != 35);               // se carattere == "#" (fine riga)
        
        if(i==-1) { //non prendo nessun intero -> lista archi vuota
            my_data -> graph[j].edges_pointer = NULL;
            my_data -> graph[j].edge_num = 0;
        } else {
            my_data -> graph[j].edges_pointer = head;
            my_data -> graph[j].edge_num = k;
        }
        k = 0;  //vertice successivo
    }

    //printf("Thread n. %i e ho finito\n", my_data->id);

    pthread_exit((void *) 0);

}

void RandomizedVisit(int node_num, int lbl_num, row_l* labels, row_g* graph, int* rank_root, int num_vertex){

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
            RandomizedVisit(next_node[indexes[j]], lbl_num, labels, graph, rank_root, num_vertex);
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
            RandomizedVisit(roots[indexes[j]], i, labels, graph, &rank_node, num_vertex);
        }
    }
}

void* RandomizedLabelingParallel(void* args) {
    t_lbl_args* my_data;
    my_data = (t_lbl_args*) args;

    for(int j=0; j< my_data->roots_num; j++) {
        RandomizedVisit(my_data->roots[my_data->indexes[j]], my_data->lbl_num, my_data->labels, my_data->graph, &my_data->rank_node, my_data->vertex_num);
    }

    pthread_exit((void *) 0);
}

// La funzione RandomizedLabelingInit prepara la struttura data per ogni
// thread - uno per label per il momento - e avvia i thread
// volendo questa init si può spostare nel main (TODO ?)
void RandomizedLabelingInit(row_g * graph, row_l * labels, int label_num, int vertex_num, int * roots, int roots_num){
    int i, j, err_code ;
    pthread_t threads_lbl[label_num];   //1 thread for each label
    t_lbl_args args_lbl[label_num];

    //inizializzazione per valori random
    srand(time(NULL));

    for(i=0; i<label_num; i++){
        args_lbl[i].rank_node = 1;

        //Inizializzazione indici
        args_lbl[i].indexes = (int *) malloc(roots_num*sizeof(int));
        if(args_lbl[i].indexes == NULL) {
            printf ("RandomizedLabelingInit: Not enough room for these indexes\n" );
            exit(1);
        }

        for(j=0; j<roots_num; j++) {
            args_lbl[i].indexes[j] = j;
        }

        //Le radici sono gia' state cercate nel main.
        randomize(args_lbl[i].indexes, roots_num);

        args_lbl[i].lbl_num = i;
        args_lbl[i].labels = labels;    //it's a pointer so it can modify the object.
                                        //TODO volendo potrei passare direttamente: labels[lbl_num]
        args_lbl[i].graph = graph;
        args_lbl[i].vertex_num = vertex_num;
        args_lbl[i].roots = roots;
        args_lbl[i].roots_num = roots_num;

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

//Scopo di "scanRoots": inizializzare l'array di roots
//Questa è una funzione parallela, quindi i thread si
//dividono il grafo da leggere con lo stesso meccanismo
//usato anche in scanFile.
//Tale array verrà successivamente randomizzato ed
//utilizzato per creare le labels

void *scanRoots(void *args) {
    t_args *my_data;
    my_data = (t_args *) args;
    int sup, inf, i, j;

    //Definition of 'Sup' and 'Inf' so that each thread
    //can read in parallel the graph
    //see scanFile for more details
    if (my_data->id == NUM_THREADS-1)
        sup = my_data->total_vertex - 1;
    else
        sup = ((my_data->total_vertex) / NUM_THREADS) * (my_data->id + 1);

    if (my_data->id == 0)
        inf = 0;
    else
        inf = ((my_data->total_vertex)/NUM_THREADS)*(my_data->id);

    /*printf("Sono il thread %i con inf %i e sup %i.\n", my_data->id, inf, sup-1);
    fflush(stdout);*/

    for(i=inf; i<sup; i++) {
        if(!my_data->graph[i].not_root) {  //if (is root)
            pthread_mutex_lock(my_data->roots_mutex);
                j = (*(my_data->root_index));
                my_data->roots[j] = i;
                *my_data->root_index = j + 1;
            pthread_mutex_unlock(my_data->roots_mutex);

        }
    }

    /*printf("Sono il thread %i e ho finito.\n", my_data->id);
    fflush(stdout);*/

    pthread_exit((void *) 0);
}

// args[1]: file1 (input .gra)
// args[2]: n (label number)
// args[3]: file2 (.que)
int main(int argc, char *argv[]) {

    FILE *fp;
    unsigned int num_vertex;
    int i, j, size, c=0, k=0, err_code=0, d;
    row_g *rows;
    pthread_t threads[NUM_THREADS];
    t_args args[NUM_THREADS];
    int *roots;
    int roots_num, root_index;
    pthread_mutex_t *roots_mutex;
    row_l *labels;

    // Controllo sugli argomenti

    if (argc != 3) {
        fprintf(stderr, "For the moment, enter only 'file1' and 'n' as arguments!\n");
        exit(1);
    }

    d = atoi(argv[2]);

    if (d <=0 ){
        fprintf(stderr, "Please insert valid value for labels number!\n");
        exit(1);
    }

    // Apertura file1

    fp = fopen(argv[1], "r");

    if (fp == NULL) {
        fprintf(stderr, "Unable to open file %s for reading.\n", argv[1] );
        exit(1);
    }

    // Prendere numero vertici per allocare la giusta memoria

    fscanf(fp,"%i\n", &num_vertex);

    rows = (row_g *) malloc (num_vertex * sizeof (row_g));  //array di liste
    if (rows == NULL ) {
        printf ("Not enough room for this size graph\n" );
        exit(1);
    }

    // Prendere grandezza file per dividerlo per fseek

    fseek(fp, 0L, SEEK_END);
    size = ftell(fp);

    fclose(fp);

    // Ottengo roots_num come num_vertex - not_roots.
    // in pratica quando scorro per leggere il file
    // se incontro una 'non radice', decremento il contatore -> richiede protezione
    roots_num = num_vertex;

    roots_mutex = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
    if (roots_mutex == NULL ) {
        printf ("Error in creating mutex protection for roots counter \n" );
        exit(1);
    }

    if(pthread_mutex_init(roots_mutex, NULL) != 0){
        printf ("Error in initializing mutex protection for roots counter \n" );
        exit(1);
    }

    // Creazione thread
    for(j=0; j < NUM_THREADS; j++) {
        args[j].filename = argv[1];
        args[j].graph = rows;
        args[j].total_vertex = num_vertex;          //facoltativo? TODO
        args[j].size_file = size;
        args[j].roots_num = &roots_num;             //puntatore alla variabile 'condivisa'
        args[j].roots_mutex = roots_mutex;          //protezione per la variabile 'condivisa'
    }

    printf("Inizio a leggere il file...\n");

    for(i=0; i<NUM_THREADS; i++) {    //eventualmente si può unire il for.    TODO
        args[i].id = i;
        err_code = pthread_create(&threads[i], NULL, scanFile, (void *)&args[i]);
        if(err_code) {
            printf ("Errore numero %i nella creazione del thread %i.\n", err_code, i);
            exit(1);
        }
    }

    // Aspettare che i thread finiscano

    for(j=0; j < NUM_THREADS; j++) {
        err_code = pthread_join(threads[j], NULL);
        if(err_code) {
            printf ("Errore numero %i nel joining del thread %i.\n", err_code, j);
            exit(1);
        }
    }
    printf("Fine lettura file...\n");
    // Stampa di prova grafo

    for(i=0; i<num_vertex; i++) {
        printf("%i : ", i);
        print_list(rows[i].edges_pointer);
        //printf(" -----> num_edge: %i\n", rows[i].edge_num);
        printf("\n");
    }

    printf("Ricerca delle radici ...\n");
    //Inizializzazione array Roots

    roots = (int *) malloc(roots_num * sizeof(int));
    if (roots == NULL ) {
        printf ("Error in creating roots struct\n" );
        exit(1);
    }

    root_index = 0;     //variabile *condivisa* per inizializzare parallelamente Roots

    // Creazione thread per lettura radici
    //E' considerata radice ogni nodo senza genitori (not_root = false)
    for(i=0; i<NUM_THREADS; i++) {
        args[i].id = i;
        args[i].roots = roots;
        args[i].root_index = &root_index;
        err_code = pthread_create(&threads[i], NULL, scanRoots, (void *)&args[i]);
        if(err_code) {
            printf ("Errore numero %i nella creazione del thread %i.\n", err_code, i);
            exit(1);
        }
    }

    // Aspettare che i thread finiscano

    for(j=0; j < NUM_THREADS; j++) {
        err_code = pthread_join(threads[j], NULL);
        if(err_code) {
            printf ("Errore numero %i nel joining del thread %i.\n", err_code, j);
            exit(1);
        }
    }
    printf("Fine ricerca delle radici ...\n");

    // Stampa di prova radici
    printf("roots: ");
    for(i=0; i<roots_num; i++){
        printf("%i ", roots[i]);
    }
    printf("\n");


    // Allocazione struct per labels
    // labels procederrano di pari ordine con l'indice rows (Indice del nodo).

    labels = (row_l *) malloc (num_vertex * sizeof (row_l));
    if (labels == NULL ) {
        printf ("Not enough room for this size labels\n" );
        exit(1);
    }

    for(i=0; i<num_vertex; i++){
        labels[i].lbl_start = (int *) malloc (d * sizeof (int));
        labels[i].lbl_end = (int *) malloc (d * sizeof (int));
        labels[i].visited = (bool *) malloc (d * sizeof (bool));
        if ((labels[i].lbl_start == NULL ) || (labels[i].lbl_end == NULL ) || (labels[i].visited == NULL )) {
            printf ("Not enough room for this size labels\n" );
            exit(1);
        }

        //Inizializzazione (Non è detto che tutto sia azzerato!)
        memset(labels[i].lbl_start, 0, d * sizeof(int));
        memset(labels[i].lbl_end, 0, d * sizeof(int));
        memset(labels[i].visited, 0, d * sizeof(bool));
    }

    printf("Creazione delle labels...\n");
    //RandomizedLabeling(rows, labels, d, num_vertex, roots, roots_num);    //TODO statistics
    RandomizedLabelingInit(rows, labels, d, num_vertex, roots, roots_num);
    printf("Fine creazione delle labels...\n");

    //Stampa delle labels
    for(i=0; i<num_vertex; i++){
        printf("Node: %i ", i);
        for(j=0; j<d; j++){
            printf("[%i, %i] ", labels[i].lbl_start[j], labels[i].lbl_end[j]);
        }
        printf("\n");
    }

    // Deallocazione di tutte le risorse

    for(i=0; i<num_vertex; i++) {
        free_list(rows[i].edges_pointer);
    }

    pthread_mutex_destroy(roots_mutex);
    free(roots_mutex);
    free(roots);

    for(i=0; i<num_vertex; i++){
        free(labels[i].lbl_start);
        free(labels[i].lbl_end);
        free(labels[i].visited);
    }
    free(labels);

    free(rows);

    return 0;
}





// comando per compilare
// gcc -o q2 q2.c -lpthread