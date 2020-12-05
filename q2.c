#define _GNU_SOURCE  // allow usage of asprintf on GNU/Linux
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>
#include <sys/resource.h>

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
    int *roots_num;
    int *root_index;
    pthread_mutex_t *roots_mutex;
} t_args;

// Scopo di "scanFile" : Leggere file1
//ogni thread leggerà da inf a sup.
//Inf e Sup sono il numero di riga del file di input 0, 1, ..., 10,...
//Nel seguente codice, si sta per dividere il file da leggere
//in N parti, così che ogni thread legga in parallelo
//durante la lettura del grafo, si memorizzano alcune informazioni
//necessarie per la creazione successiva delle labels (roots_num)

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

    int rank_children_min = num_vertex, i, j, children_num = graph[node_num].edge_num, node;
    bool stop = false;
    bool random_visited[children_num];
    int next_node[children_num];
    
    memset(random_visited, false, children_num * sizeof(bool));
    
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

        while(!stop){
            do{
                node = rand() % children_num; //da 0 a children - 1
            }while(random_visited[node]);

            random_visited[node] = true;

            RandomizedVisit(next_node[node], lbl_num, labels, graph, rank_root, num_vertex);

            stop = true;
            for(j=0; j < children_num; j++){
                if (!random_visited[j]){
                    stop = false;
                    break;
                }
            }
        }

        // memset(random_visited, 0, num_vertex * sizeof(bool));
        // stop=false;
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

// TODO
// Scegliere le radici in modo completamente random va bene? 
// Devo considerare il caso in cui due iterazioni successive sono completamente uguali ?
void RandomizedLabeling(row_g * graph, row_l * labels, int num_label, int num_vertex, int * roots, int num_roots){
    int i, rank_node, j, node ;
    bool stop = false;
    bool random_visited[num_vertex];

    memset(random_visited, 0, num_vertex * sizeof(bool));

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
        randomize(indexes, num_roots);
        rank_node = 1;

        /*while(!stop){
            
            do{
                node = roots[i]; //da 0 a num_vertex - 1
            }while(random_visited[node]);

            random_visited[node] = true;*/

            //devo cercare le radici.
            //E' considerato radice ogni nodo con almeno un figlio.
            //TODO: ERRORE!!! è considerato radice ogni nodo senza genitori!
            for(int j=0; j<num_roots; j++) {
                RandomizedVisit(roots[indexes[j]], i, labels, graph, &rank_node, num_vertex);
            }

            /*stop = true;
            for(j=0; j < num_vertex; j++){
                if (!random_visited[j]){
                    stop = false;
                    break;
                }
            }
        }*/
        /*memset(random_visited, 0, num_vertex * sizeof(bool));
        stop=false;*/
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

// see https://stackoverflow.com/a/10192994
long long unsigned compute_delta_microseconds(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000;
}

// asprintf automatically allocates the needed memory - see https://stackoverflow.com/a/23842944
char* get_human_readable_time(long long unsigned microseconds) {
    long us, ms, s, m, h;
    char* result;
    h = (long) microseconds / 3600000000;
    m = (long) microseconds / 60000000;
    s = (long) microseconds / 1000000;
    ms = (long) microseconds / 1000;
    us = (long) microseconds - (h * 3600000000 + m * 60000000 + s * 1000000 + ms * 1000);
    if (microseconds <= 0)
        asprintf(&result, "less than 0 microseconds");
    else if (microseconds > 0 && microseconds < 1000)
        asprintf(&result, "%llu microseconds", microseconds);
    else if (microseconds >= 1000 && microseconds < 1000000)
        asprintf(&result, "%ld milliseconds, %ld microseconds", ms, us);
    else if (microseconds >= 1000000 && microseconds < 60000000)
        asprintf(&result, "%ld seconds, %ld milliseconds, %ld microseconds", s, ms, us);
    else if (microseconds >= 60000000 && microseconds < 3600000000)
        asprintf(&result, "%ld minutes, %ld seconds, %ld milliseconds, %ld microseconds", m, s, ms, us);
    else
        asprintf(&result, "%ld hours, %ld minutes, %ld seconds, %ld milliseconds, %ld microseconds", h, m, s, ms, us);
    return result;
}

char* get_human_readable_memory_usage(long unsigned kilobytes) {
    long unsigned kb, mb, gb;
    char* result;
    kilobytes = kilobytes / MEM_SIZE;
    gb = (long unsigned) kilobytes / (1024 * 1024);
    mb = (long unsigned) kilobytes / 1024;
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
    struct timespec start, file1_read, file2_read, labels_generation_finished, reachability_queries_finished;
    long long unsigned delta_microseconds;
    struct rusage memory;
    char* stats;

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

    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
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
    delta_microseconds = compute_delta_microseconds(start, file1_read);
    asprintf(&stats, "Read input file %s (file1) in %s.\n", argv[1], get_human_readable_time(delta_microseconds));
    asprintf(&stats, "%s%s", stats, get_rss_virt_mem());

    // Stampa di prova grafo

    for(i=0; i<num_vertex; i++) {
        printf("%i : ", i);
        print_list(rows[i].edges_pointer);
        //printf(" -----> num_edge: %i\n", rows[i].edge_num);
        printf("\n");
    }

    //Inizializzazione array Roots

    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    roots = (int *) malloc(roots_num * sizeof(int));
    if (roots == NULL ) {
        printf ("Error in creating roots struct\n" );
        exit(1);
    }

    root_index = 0;     //variabile *condivisa* per inizializzare parallelamente Roots

    // Creazione thread per lettura radici
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

    RandomizedLabeling(rows, labels, d, num_vertex, roots, roots_num);

    clock_gettime(CLOCK_MONOTONIC_RAW, &labels_generation_finished);
    delta_microseconds = compute_delta_microseconds(start, labels_generation_finished);
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

    getrusage(RUSAGE_SELF, &memory);
    asprintf(&stats, "%sMaximum memory usage: %s\n", stats, get_human_readable_memory_usage(memory.ru_maxrss));
    asprintf(&stats, "%s%s", stats, get_rss_virt_mem());

    fprintf(stdout, "\n\n------------STATISTICS------------\n%s", stats);

    return 0;
}





// comando per compilare
// gcc -o q2 q2.c -lpthread