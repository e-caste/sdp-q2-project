#include "main.h"
#include "buildLabels.h"
#include "readGraph.h"
#include "solveQuery.h"
#include "utility.h"
// TODO: https://en.wikipedia.org/wiki/C_data_types
//      Ottimizzazione delle memoria: sostituire int con short se possibile
//      Quanti nodi al massimo? unsigned int: [0, 65,535] ; unsigned long int: [0, 4,294,967,295]


// argv[1]: file1 (input .gra)
// argv[2]: n (label number)
// argv[3]: file2 (.que)
int main(int argc, char *argv[]) {
    FILE *fp, *fp_query;
    unsigned int num_vertex, num_threads;
    int i, j, size, c=0, k=0, err_code=0, d;
    row_g *rows;
    pthread_t *threads;
    t_args *args;
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
    bool **visited;

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

    // this is needed to prevent an infinite wait
    // when using very small graphs (e.g. 10 vertices) on heavily multithreaded CPUs (e.g. 24 threads)
    num_threads = NUM_THREADS > num_vertex ? num_vertex : NUM_THREADS;

    // only now we know the correct size of these arrays
    threads = (pthread_t *) malloc(num_threads * sizeof(pthread_t));
    args = (t_args *) malloc(num_threads * sizeof(t_args));

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
    for(j=0; j < num_threads; j++) {
        args[j].filename = argv[1];
        args[j].graph = rows;
        args[j].total_vertex = num_vertex;          //facoltativo? TODO
        args[j].total_threads = num_threads;
        args[j].size_file = size;
        args[j].roots_num = &roots_num;             //puntatore alla variabile 'condivisa'
        args[j].roots_mutex = roots_mutex;          //protezione per la variabile 'condivisa'
    }

    printf("Inizio a leggere il file...\n");

    for(i=0; i<num_threads; i++) {    //eventualmente si può unire il for.    TODO
        args[i].id = i;
        err_code = pthread_create(&threads[i], NULL, scanFile, (void *)&args[i]);
        if(err_code) {
            printf ("Errore numero %i nella creazione del thread %i.\n", err_code, i);
            exit(1);
        }
    }

    // Aspettare che i thread finiscano

    for(j=0; j < num_threads; j++) {
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

    // for(i=0; i<num_vertex; i++) {
    //     printf("%i : ", i);
    //     print_list(rows[i].edges_pointer);
    //     //printf(" -----> num_edge: %i\n", rows[i].edge_num);
    //     printf("\n");
    // }

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
    for(i=0; i<num_threads; i++) {
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

    for(j=0; j < num_threads; j++) {
        err_code = pthread_join(threads[j], NULL);
        if(err_code) {
            printf ("Errore numero %i nel joining del thread %i.\n", err_code, j);
            exit(1);
        }
    }
    fprintf(stdout, "Fine ricerca delle radici ...\n");

    // Stampa di prova radici
    // printf("roots: ");
    // for(i=0; i<roots_num; i++){
    //     printf("%i ", roots[i]);
    // }
    // printf("\n");


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
    RandomizedLabelingParallelInit(rows, labels, d, num_vertex, roots, roots_num);
    //RandomizedLabeling(rows, labels, d, num_vertex, roots, roots_num);
    fprintf(stdout, "Fine creazione delle labels...\n");

    fflush(stdout);

    clock_gettime(CLOCK_MONOTONIC_RAW, &labels_generation_finished);
    delta_microseconds = compute_delta_microseconds(section_start, labels_generation_finished);
    asprintf(&stats, "%sGenerated %s labels in %s.\n", stats, argv[2], get_human_readable_time(delta_microseconds));
    asprintf(&stats, "%s%s", stats, get_rss_virt_mem());

    //Stampa delle labels
    /*for(i=0; i<num_vertex; i++){
        printf("Node: %i ", i);
        for(j=0; j<d; j++){
            printf("[%i, %i] ", labels[i].lbl_start[j], labels[i].lbl_end[j]);
        }
        printf("\n");
    }*/

    clock_gettime(CLOCK_MONOTONIC_RAW, &section_start);

    // Lettura query
    fp_query = fopen(argv[3], "r");
    if (fp_query == NULL) {
        fprintf(stderr, "Unable to open file %s for reading.\n", argv[3] );
        exit(1);
    }

    int a, b, num_query=0;

    while (fscanf(fp, "%*[^\n]\n") != -1) {
        num_query++;
    }

    printf("Numero Query: %i\n", num_query);

    el_query *queries = malloc(sizeof(el_query)*num_query);

    fseek(fp_query, 0L, SEEK_SET);

    for(i=0; i<num_query; i++) {
        fscanf(fp_query, "%i %i  \n", &a, &b);
        queries[i].num[0] = a;
        queries[i].num[1] = b;
    }

    fclose(fp_query);

    // Stampa query
    /*for(i=0; i<num_query; i++) {
        printf("Query %i: %i %i\n", i, queries[i].num[0], queries[i].num[1]);
    }*/

    clock_gettime(CLOCK_MONOTONIC_RAW, &file2_read);
    delta_microseconds = compute_delta_microseconds(section_start, file2_read);
    asprintf(&stats, "%sRead query file %s (file2) in %s.\n", stats, argv[3], get_human_readable_time(delta_microseconds));
    asprintf(&stats, "%s%s", stats, get_rss_virt_mem());
    
    //print_list_query(head_query);

    clock_gettime(CLOCK_MONOTONIC_RAW, &section_start);

    // Risoluzione query
    visited = (bool **) malloc(num_threads * sizeof(bool *));

    for(j=0; j<num_threads; j++) {
        visited[j] = (bool *) malloc(num_vertex * sizeof(bool));
        args[j].array_queries = queries;
        args[j].node_visited = visited[j];
        args[j].queries_num = num_query;
        args[j].num_labels = d;
        args[j].array_labels = labels;
        err_code = pthread_create(&threads[j], NULL, solveQuery, (void *)&args[j]);
        if(err_code) {
            printf ("Errore numero %i nella creazione del thread %i.\n", err_code, j);
            exit(1);
        }
    }

    for(j=0; j < num_threads; j++) {
        err_code = pthread_join(threads[j], NULL);
        if(err_code) {
            printf ("Errore numero %i nel joining del thread %i.\n", err_code, j);
            exit(1);
        }
    }

    FILE *fp_res_query;
    fp_res_query = fopen("res_query_prova.txt", "w");

    if (fp_res_query == NULL) {
        fprintf(stderr, "Unable to open file for store the result of the query.\n");
        exit(1);
    }

    for(i=0; i<num_query; i++) {
        fprintf(fp_res_query, "%i %i %d\n", queries[i].num[0], queries[i].num[1], queries[i].can_reach ? 1 : 0);
    }

    fclose(fp_res_query);

    clock_gettime(CLOCK_MONOTONIC_RAW, &reachability_queries_finished);
    delta_microseconds = compute_delta_microseconds(section_start, reachability_queries_finished);
    asprintf(&stats, "%sTested %d reachability queries in %s.\n", stats, num_query, get_human_readable_time(delta_microseconds));
    asprintf(&stats, "%s%s", stats, get_rss_virt_mem());

    // Deallocazione di tutte le risorse

    for(i=0; i<num_vertex; i++) {
        free_list(rows[i].edges_pointer);
        pthread_mutex_destroy(rows[i].node_mutex);
        free(rows[i].node_mutex);
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
    free(queries);

    clock_gettime(CLOCK_MONOTONIC_RAW, &program_finished);
    delta_microseconds = compute_delta_microseconds(program_start, program_finished);
    asprintf(&stats, "%sTotal program duration: %s.\n", stats, get_human_readable_time(delta_microseconds));

    asprintf(&stats, "%sThreads used: %ld.\n", stats, num_threads);

    getrusage(RUSAGE_SELF, &memory);
    asprintf(&stats, "%sMaximum memory usage: %s\n", stats, get_human_readable_memory_usage(memory.ru_maxrss));

    fprintf(stdout, "\n\n------------STATISTICS------------\n%s", stats);

    return 0;
}