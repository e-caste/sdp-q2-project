#define _GNU_SOURCE  // allow usage of asprintf on GNU/Linux
#include "q2.h"

#define NUM_THREADS sysconf(_SC_NPROCESSORS_ONLN)

// see https://iq.opengenus.org/detect-operating-system-in-c/
// used to print the used memory correctly on macOS and GNU/Linux
#ifdef __APPLE__
    #define MEM_SIZE 1024
#else
    #define MEM_SIZE 1
#endif

// TODO: https://en.wikipedia.org/wiki/C_data_types
//      Ottimizzazione delle memoria: sostituire int con short se possibile
//      Quanti nodi al massimo? unsigned int: [0, 65,535] ; unsigned long int: [0, 4,294,967,295]

// TODO: dividere q2.c in più file (es: main.c, read.c, label.c, query.c)

typedef struct edge_list {
    int num;            //valore del vertice
    struct edge_list *next_num;
} edge;

typedef struct el_list_query {
    int num[2];            //valore del vertice
    struct el_list_query *next_num;
} el_query;

void create_list(edge *head, int val) {
    edge *current = head;
    current -> num = val;
    current -> next_num = NULL;
}

void create_list_query(el_query *head, int val1, int val2) {
    el_query *current = head;
    current -> num[0] = val1;
    current -> num[1] = val2;
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

void push_query(el_query *head, int val1, int val2) {
    el_query *current = head;

    while(current -> next_num != NULL) {
        current = current -> next_num;
    }

    current -> next_num = (el_query *) malloc(sizeof(el_query));
    current -> next_num -> num[0] = val1;
    current -> next_num -> num[1] = val2;
    current -> next_num -> next_num = NULL;
}

void print_list(edge *head) {
    edge *current = head;

    while(current != NULL) {
        printf("%i ", current -> num);
        current = current -> next_num;
    }
}

void print_list_query(el_query *head) {
    el_query *current = head;

    while(current != NULL) {
        printf("%i %i\n", current -> num[0], current -> num[1]);
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

void free_list_query(el_query *head) {
    el_query *tmp;

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

                //not_root_is_set = my_data->graph[i].not_root ? true : false;  // default == 0 == false;

                if(k==0) {
                    create_list(head, i);
                } else {
                    push(head, i);
                }
                //my_data -> graph[i].not_root = true;   //se era a uno lo rimetto a 1

                pthread_mutex_lock(my_data->roots_mutex);
                    not_root_is_set = my_data->graph[i].not_root ? true : false;  // default == 0 == false;
                    my_data -> graph[i].not_root = true;   //se era a uno lo rimetto a 1
                    if ((!not_root_is_set) && (my_data->graph[i].not_root)){
                            *my_data->roots_num = (*(my_data->roots_num) - 1);
                    }
                pthread_mutex_unlock(my_data->roots_mutex);
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

    int indexes[my_data->roots_num];
    
    //Randomizzazione radici (ogni thread il suo)
    //Le radici sono gia' state cercate nel main.

    for(int i=0; i<my_data->roots_num; i++) {
        indexes[i] = i;
    }
    
    randomize(indexes, my_data->roots_num);

    for(int j=0; j< my_data->roots_num; j++) {
        RandomizedVisit(my_data->roots[indexes[j]], my_data->lbl_num, my_data->labels, my_data->graph, &my_data->rank_node, my_data->vertex_num);
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
        sup = ((my_data->total_vertex) / NUM_THREADS) * (my_data->id + 1);      //TODO e' necessario cast(int)?

    if (my_data->id == 0)
        inf = 0;
    else
        inf = ((my_data->total_vertex)/NUM_THREADS)*(my_data->id);

    for(i=inf; i<sup; i++) {
        if(!my_data->graph[i].not_root) {  //if (is root)
            pthread_mutex_lock(my_data->roots_mutex);
                j = (*(my_data->root_index));
                my_data->roots[j] = i;
                *my_data->root_index = j + 1;
            pthread_mutex_unlock(my_data->roots_mutex);

        }
    }

    pthread_exit((void *) 0);
}

bool dfs_search(row_g *graph, int node1, int node2, bool *visited) {
    if(node1 == node2)
        return true;
    if(visited[node1])
        return false;

    int children_num = graph[node1].edge_num;
    bool reachable = false;
    
    if(children_num > 0) {

        int children[children_num];
        int i=0;

        for(edge* next=graph[node1].edges_pointer; next != NULL; next = next->next_num){
            if(next -> num == node2) {
                return true;
            }
            children[i] = next->num;
            i++;
        }

        for(i=0; i<children_num; i++) {
            reachable = dfs_search(graph, children[i], node2, visited);
            if(reachable) {
                return true;
            }
        }

    }

    visited[node1] = true;

    return reachable;
}

// see https://stackoverflow.com/a/10192994
long long unsigned compute_delta_microseconds(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000;
}

// asprintf automatically allocates the needed memory - see https://stackoverflow.com/a/23842944
char* get_human_readable_time(long long unsigned microseconds) {
    long long unsigned us, ms, s, m, h;
    char* result;
    h = (long long unsigned) microseconds / 3600000000;
    m = (long long unsigned) (microseconds - (h * 3600000000)) / 60000000;
    s = (long long unsigned) (microseconds - (h * 3600000000 + m * 60000000)) / 1000000;
    ms = (long long unsigned) (microseconds - (h * 3600000000 + m * 60000000 + s * 1000000)) / 1000;
    us = (long long unsigned) microseconds - (h * 3600000000 + m * 60000000 + s * 1000000 + ms * 1000);
    if (microseconds <= 0)
        asprintf(&result, "less than 0 microseconds");
    else if (microseconds > 0 && microseconds < 1000)
        asprintf(&result, "%llu microseconds", microseconds);
    else if (microseconds >= 1000 && microseconds < 1000000)
        asprintf(&result, "%llu milliseconds, %llu microseconds", ms, us);
    else if (microseconds >= 1000000 && microseconds < 60000000)
        asprintf(&result, "%llu seconds, %llu milliseconds, %llu microseconds", s, ms, us);
    else if (microseconds >= 60000000 && microseconds < 3600000000)
        asprintf(&result, "%llu minutes, %llu seconds, %llu milliseconds, %llu microseconds", m, s, ms, us);
    else
        asprintf(&result, "%llu hours, %llu minutes, %llu seconds, %llu milliseconds, %llu microseconds", h, m, s, ms, us);
    return result;
}

char* get_human_readable_memory_usage(long unsigned kilobytes) {
    long unsigned kb, mb, gb;
    char* result;
    kilobytes = kilobytes / MEM_SIZE;
    gb = (long unsigned) kilobytes / (1024 * 1024);
    mb = (long unsigned) (kilobytes - (gb * 1024 * 1024)) / 1024;
    kb = (long unsigned) kilobytes - (gb * 1024 * 1024 + mb * 1024);
    if (kilobytes <= 0)
        asprintf(&result, "less than 0 KB");
    else if (kilobytes > 0 && kilobytes < 1024)
        asprintf(&result, "%lu KB", kilobytes);
    else if (kilobytes >= 1024 && kilobytes < 1024 * 1024)
        asprintf(&result, "%lu MB %lu KB", mb, kb);
    else
        asprintf(&result, "%lu GB %lu MB %lu KB", gb, mb, kb);
    return result;
}

// see https://www.tutorialspoint.com/how-to-get-memory-usage-at-runtime-using-cplusplus
// and https://man7.org/linux/man-pages/man5/proc.5.html
// and https://en.wikipedia.org/wiki/Resident_set_size
char* get_rss_virt_mem(void) {
    FILE *stat;
    long unsigned rss, virt;
    char* result;
    stat = fopen("/proc/self/stat", "r");
    if (stat == NULL) {
        // assuming on UNIX but not GNU/Linux
        return "";
    }
    fscanf(stat,
           "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %*u %*u %*d %*d %*d %*d %*d %*d %*u %lu %ld",
           &virt, &rss);
    fclose(stat);
    virt = (long unsigned) virt / 1024;
    rss = (long unsigned) rss * sysconf(_SC_PAGESIZE) / 1024;
    asprintf(&result,
             "Currently used memory (RAM): %s\n"
             "Currently used virtual memory (included pages): %s\n",
             get_human_readable_memory_usage(rss),
             get_human_readable_memory_usage(virt));
    return result;
}

// argv[1]: file1 (input .gra)
// argv[2]: n (label number)
// argv[3]: file2 (.que)
int main(int argc, char *argv[]) {

    FILE *fp, *fp_query;
    unsigned int num_vertex;
    int i, j, size, c=0, k=0, err_code=0, d;
    row_g *rows;
    pthread_t threads[NUM_THREADS];
    t_args args[NUM_THREADS];
    int *roots;
    int roots_num, root_index;
    pthread_mutex_t *roots_mutex;
    row_l *labels;
    struct timespec program_start, section_start, file1_read, file2_read, labels_generation_finished, reachability_queries_finished, program_finished;
    long long unsigned delta_microseconds;
    struct rusage memory;
    char* stats;
    // needed for reachability query
    int node1, node2;
    bool dfs;
    bool reachable;
    bool *visited;

    // Controllo sugli argomenti

    if (argc != 4) {
        fprintf(stderr, "Enter only 'file1', 'n' and 'file2' as arguments!\n");
        exit(1);
    }

    d = atoi(argv[2]);

    if (d <=0 ){
        fprintf(stderr, "Please insert valid value for labels number!\n");
        exit(1);
    }

    // Apertura file1

    clock_gettime(CLOCK_MONOTONIC_RAW, &program_start);
    clock_gettime(CLOCK_MONOTONIC_RAW, &section_start);
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

    clock_gettime(CLOCK_MONOTONIC_RAW, &file1_read);
    delta_microseconds = compute_delta_microseconds(section_start, file1_read);
    asprintf(&stats, "Read input file %s (file1) in %s.\n", argv[1], get_human_readable_time(delta_microseconds));
    asprintf(&stats, "%s%s", stats, get_rss_virt_mem());
    fprintf(stdout, "Fine lettura file...\n");

    // Stampa di prova grafo

    for(i=0; i<num_vertex; i++) {
        printf("%i : ", i);
        print_list(rows[i].edges_pointer);
        //printf(" -----> num_edge: %i\n", rows[i].edge_num);
        printf("\n");
    }

    fprintf(stdout, "Ricerca delle radici ...\n");
    //Inizializzazione array Roots

    clock_gettime(CLOCK_MONOTONIC_RAW, &section_start);
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
    fprintf(stdout, "Fine ricerca delle radici ...\n");

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
        memset(labels[i].visited, false, d * sizeof(bool));
    }

    fprintf(stdout, "Creazione delle labels...\n");
    RandomizedLabelingInit(rows, labels, d, num_vertex, roots, roots_num);
    //RandomizedLabeling(rows, labels, d, num_vertex, roots, roots_num);
    fprintf(stdout, "Fine creazione delle labels...\n");

    clock_gettime(CLOCK_MONOTONIC_RAW, &labels_generation_finished);
    delta_microseconds = compute_delta_microseconds(section_start, labels_generation_finished);
    asprintf(&stats, "%sGenerated %s labels in %s.\n", stats, argv[2], get_human_readable_time(delta_microseconds));
    asprintf(&stats, "%s%s", stats, get_rss_virt_mem());

    //Stampa delle labels
    for(i=0; i<num_vertex; i++){
        printf("Node: %i ", i);
        for(j=0; j<d; j++){
            printf("[%i, %i] ", labels[i].lbl_start[j], labels[i].lbl_end[j]);
        }
        printf("\n");
    }

    clock_gettime(CLOCK_MONOTONIC_RAW, &section_start);

    // Lettura query
    fp_query = fopen(argv[3], "r");
    if (fp_query == NULL) {
        fprintf(stderr, "Unable to open file %s for reading.\n", argv[3] );
        exit(1);
    }

    int a, b, num_query=0;
    el_query *head_query = malloc(sizeof(el_query));

    while (fscanf(fp_query, "%i %i  \n", &a, &b) != -1) {
        if(num_query != 0) {
            push_query(head_query, a, b);
            num_query++;
        } else {
            create_list_query(head_query, a, b);
            num_query++;
        }
    }

    fclose(fp_query);

    clock_gettime(CLOCK_MONOTONIC_RAW, &file2_read);
    delta_microseconds = compute_delta_microseconds(section_start, file2_read);
    asprintf(&stats, "%sRead query file %s (file2) in %s.\n", stats, argv[3], get_human_readable_time(delta_microseconds));
    asprintf(&stats, "%s%s", stats, get_rss_virt_mem());
    
    //print_list_query(head_query);

    printf("Numero Query: %i\n", num_query);

    clock_gettime(CLOCK_MONOTONIC_RAW, &section_start);

    // Risoluzione query

    FILE *fp_res_query;
    fp_res_query = fopen("res_query.txt", "w");

    if (fp_res_query == NULL) {
        fprintf(stderr, "Unable to open file for store the result of the query.\n");
        exit(1);
    }

    visited = (bool *) malloc(num_vertex * sizeof(bool));

    for(el_query* next=head_query; next != NULL; next = next->next_num) {
        dfs = true;
        node1 = next -> num[0];
        node2 = next -> num[1];

        for(i=0; i<d; i++) {
            if (labels[node1].lbl_start[i]>labels[node2].lbl_start[i] ||
                labels[node1].lbl_end[i]<labels[node2].lbl_end[i]) {
                fprintf(fp_res_query, "%i %i 0\n", node1, node2);
                dfs = false;
                break;
            }
        }

        if(dfs) {
            memset(visited, false, num_vertex * sizeof(bool));
            reachable = dfs_search(rows, node1, node2, visited);
            fprintf(fp_res_query, "%i %i %d\n", node1, node2, reachable ? 1 : 0);
        }
    }

    fclose(fp_res_query);

    clock_gettime(CLOCK_MONOTONIC_RAW, &reachability_queries_finished);
    delta_microseconds = compute_delta_microseconds(section_start, reachability_queries_finished);
    asprintf(&stats, "%sTested %d reachability queries in %s.\n", stats, num_query, get_human_readable_time(delta_microseconds));
    asprintf(&stats, "%s%s", stats, get_rss_virt_mem());

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
    free_list_query(head_query);

    getrusage(RUSAGE_SELF, &memory);
    asprintf(&stats, "%sMaximum memory usage: %s\n", stats, get_human_readable_memory_usage(memory.ru_maxrss));
    asprintf(&stats, "%s%s", stats, get_rss_virt_mem());

    clock_gettime(CLOCK_MONOTONIC_RAW, &program_finished);
    delta_microseconds = compute_delta_microseconds(program_start, program_finished);
    asprintf(&stats, "%sTotal program duration: %s.\n", stats, get_human_readable_time(delta_microseconds));

    fprintf(stdout, "\n\n------------STATISTICS------------\n%s", stats);

    return 0;
}





// comando per compilare
// gcc -o q2 q2.c -lpthread