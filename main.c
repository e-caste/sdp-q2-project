#include "buildLabels.h"
#include "readGraph.h"
#include "solveQuery.h"
#include "utility.h"
// TODO: https://en.wikipedia.org/wiki/C_data_types
//      Ottimizzazione delle memoria: sostituire int con short se possibile
//      Quanti nodi al massimo? unsigned int: [0, 65,535] ; unsigned long int: [0, 4,294,967,295]

// argv[1]: file1 (input .gra)
// argv[2]: n (label number 'd')
// argv[3]: file2 (.que)
int main(int argc, char *argv[]) {
    FILE *fp = NULL, *fp_query = NULL;
    unsigned int num_vertex, num_threads;
    int i, j, size, err_code=0, d;
    row_g *rows = NULL;
    pthread_t *threads = NULL;
    t_args *args = NULL;
    // needed for labels generation
    int *roots = NULL;
    int roots_num, root_index;
    pthread_mutex_t *roots_mutex = NULL;
    row_l *labels = NULL;
    // needed for time and memory statistics
    struct timespec program_start, section_start, file1_read, file2_read, labels_generation_finished, reachability_queries_finished, program_finished;
    long long unsigned delta_microseconds;
    struct rusage memory;
    char* stats = NULL;
    // needed for reachability query
    bool **visited = NULL;
    el_query *queries = NULL;

    // Checks on arguments

    if (argc != 4) {
        fprintf(stderr, "Enter only 'file1', 'n' and 'file2' as arguments!\n");
        exit(1);
    }

    d = atoi(argv[2]);

    if (d < 0){
        fprintf(stderr, "Please insert valid value for labels number!\n");
        exit(1);
    }

    // Opening file1

    clock_gettime(CLOCK_MONOTONIC_RAW, &program_start);
    clock_gettime(CLOCK_MONOTONIC_RAW, &section_start);
    fp = fopen(argv[1], "r");

    if (fp == NULL) {
        fprintf(stderr, "Unable to open file %s for reading.\n", argv[1] );
        exit(1);
    }

    // Take vertex number to alloc correct memory

    fscanf(fp,"%i\n", &num_vertex);

    rows = (row_g *) malloc (num_vertex * sizeof (row_g));  //array of lists
    if (rows == NULL ) {
        printf ("Not enough room for this size graph\n" );
        exitWithDealloc(true, num_vertex, fp, rows, threads, args, roots_mutex, roots, labels, fp_query, queries);
    }

    // this is needed to prevent an infinite wait
    // when using very small graphs (e.g. 10 vertices) on heavily multithreaded CPUs (e.g. 24 threads)
    num_threads = NUM_THREADS > num_vertex ? num_vertex : NUM_THREADS;

    // only now we know the correct size of these arrays
    threads = (pthread_t *) malloc(num_threads * sizeof(pthread_t));
    if (threads == NULL ) {
        printf ("Not enough room for this threads size\n" );
        exitWithDealloc(true, num_vertex, fp, rows, threads, args, roots_mutex, roots, labels, fp_query, queries);
    }

    args = (t_args *) malloc(num_threads * sizeof(t_args));
    if (args == NULL ) {
        printf ("Not enough room for thread args size\n" );
        exitWithDealloc(true, num_vertex, fp, rows, threads, args, roots_mutex, roots, labels, fp_query, queries);
    }

    // Take the file size for divide it later by threads

    fseek(fp, 0L, SEEK_END);
    size = ftell(fp);

    fclose(fp);

    // We gain the roots_num as num_vertex - not_roots.
    // In practice, when we will scan file1
    // if we find a not_root, we decrement the counter -> need protection
    roots_num = num_vertex;

    roots_mutex = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
    if (roots_mutex == NULL ) {
        printf ("Error in creating mutex protection for roots counter \n" );
        exitWithDealloc(true, num_vertex, NULL, rows, threads, args, roots_mutex, roots, labels, fp_query, queries);
    }

    if(pthread_mutex_init(roots_mutex, NULL) != 0){
        printf ("Error in initializing mutex protection for roots counter \n" );
        exitWithDealloc(true, num_vertex, NULL, rows, threads, args, roots_mutex, roots, labels, fp_query, queries);
    }

    // Preparing structure for parallel reading of file1 by many threads
    for(j=0; j < num_threads; j++) {
        args[j].filename = argv[1];
        args[j].graph = rows;
        args[j].total_vertex = num_vertex;
        args[j].total_threads = num_threads;
        args[j].size_file = size;
        args[j].roots_num = &roots_num;             // pointer of 'shared' variable
        args[j].roots_mutex = roots_mutex;          // protection for 'shared' variable
    }

    printf("Starting the reading of DAG file...\n");

    for(i=0; i<num_threads; i++) {
        args[i].id = i;
        err_code = pthread_create(&threads[i], NULL, scanFile, (void *)&args[i]);
        if(err_code) {
            printf ("Error number %i in thread %i creation.\n", err_code, i);
            exitWithDealloc(true, num_vertex, NULL, rows, threads, args, roots_mutex, roots, labels, fp_query, queries);
        }
    }

    // Wait until all thread end
    for(j=0; j < num_threads; j++) {
        err_code = pthread_join(threads[j], NULL);
        if(err_code) {
            printf ("Error number %i in thread %i joining.\n", err_code, j);
            exitWithDealloc(true, num_vertex, NULL, rows, threads, args, roots_mutex, roots, labels, fp_query, queries);
        }
    }

    clock_gettime(CLOCK_MONOTONIC_RAW, &file1_read);
    delta_microseconds = compute_delta_microseconds(section_start, file1_read);
    asprintf(&stats, "Read input file %s (file1) in %s.\n", argv[1], get_human_readable_time(delta_microseconds));
    asprintf(&stats, "%s%s", stats, get_rss_virt_mem());
    fprintf(stdout, "End of DAG file reading...\n");

    // Test of Graph print

    // for(i=0; i<num_vertex; i++) {
    //     printf("%i : ", i);
    //     print_list(rows[i].edges_pointer);
    //     //printf(" -----> num_edge: %i\n", rows[i].edge_num);
    //     printf("\n");
    // }

    fprintf(stdout, "Starting roots search...\n");

    roots_num = 0;

    for(i=0; i<num_vertex; i++) {
        if(!rows[i].not_root) {
            roots_num++;
        }
    }

    clock_gettime(CLOCK_MONOTONIC_RAW, &section_start);
    roots = (int *) malloc(roots_num * sizeof(int));
    if (roots == NULL ) {
        printf ("Error in creating roots struct\n" );
        exitWithDealloc(true, num_vertex, NULL, rows, threads, args, roots_mutex, roots, labels, fp_query, queries);
    }

    root_index = 0;  // *shared* variable for Roots parallel inizialization
    
    //TODO 
    // merge this scanRoots thread creation with scanFile so we create just
    // one time the thread. -> merge ScanRoots in ScanFiles
    for(i=0; i<num_threads; i++) {
        args[i].id = i;
        args[i].roots = roots;
        args[i].root_index = &root_index;
        err_code = pthread_create(&threads[i], NULL, scanRoots, (void *)&args[i]);
        if(err_code) {
            printf ("Error number %i in thread %i creation.\n", err_code, i);
            exitWithDealloc(true, num_vertex, NULL, rows, threads, args, roots_mutex, roots, labels, fp_query, queries);
        }
    }

    for(j=0; j < num_threads; j++) {
        err_code = pthread_join(threads[j], NULL);
        if(err_code) {
            printf ("Error number %i in thread %i joining.\n", err_code, j);
            exitWithDealloc(true, num_vertex, NULL, rows, threads, args, roots_mutex, roots, labels, fp_query, queries);
        }
    }

    /*j=0;
    for(i=0; i<num_vertex; i++) {
        if(!rows[i].not_root) {
            roots[j] = i;
            j++;
        }
    }*/

    fprintf(stdout, "End of root search...\n");

    // ignore labels
    if (d == 0) {
        clock_gettime(CLOCK_MONOTONIC_RAW, &program_finished);
        delta_microseconds = compute_delta_microseconds(program_start, program_finished);
        asprintf(&stats, "%sTotal program duration: %s.\n", stats, get_human_readable_time(delta_microseconds));
        asprintf(&stats, "%sThreads used: %u.\n", stats, num_threads);
        getrusage(RUSAGE_SELF, &memory);
        asprintf(&stats, "%sMaximum memory usage: %s\n", stats, get_human_readable_memory_usage(memory.ru_maxrss));
        fprintf(stdout, "\n\n------------STATISTICS------------\n%s", stats);
        exitWithDealloc(true, num_vertex, NULL, rows, threads, args, roots_mutex, roots, labels, fp_query, queries);
    }

    // Test roots print
    // printf("roots: ");
    // for(i=0; i<roots_num; i++){
    //     printf("%i ", roots[i]);
    // }
    // printf("\n");

    // Label bulding
    // labels will follow the same index order of rows (node index).

    labels = (row_l *) malloc (num_vertex * sizeof (row_l));
    if (labels == NULL ) {
        printf ("Not enough room for this size labels\n" );
        exitWithDealloc(true, num_vertex, NULL, rows, threads, args, roots_mutex, roots, labels, fp_query, queries);
    }

    for(i=0; i<num_vertex; i++){
        labels[i].lbl_start = (int *) malloc (d * sizeof (int));
        labels[i].lbl_end = (int *) malloc (d * sizeof (int));
        labels[i].visited = (bool *) malloc (d * sizeof (bool));
        if ((labels[i].lbl_start == NULL ) || (labels[i].lbl_end == NULL ) || (labels[i].visited == NULL )) {
            printf ("Not enough room for this size labels\n" );
            exitWithDealloc(true, num_vertex, NULL, rows, threads, args, roots_mutex, roots, labels, fp_query, queries);
        }

        //It's not always true that these values are resetted
        memset(labels[i].lbl_start, 0, d * sizeof(int));
        memset(labels[i].lbl_end, 0, d * sizeof(int));
        memset(labels[i].visited, false, d * sizeof(bool));
    }

    fprintf(stdout, "Starting label creation...\n");
    RandomizedLabelingParallelInit(rows, labels, d, num_vertex, roots, roots_num);
    //RandomizedLabelingSequential(rows, labels, d, num_vertex, roots, roots_num);
    fprintf(stdout, "End Label creation...\n");

    fflush(stdout);

    clock_gettime(CLOCK_MONOTONIC_RAW, &labels_generation_finished);
    delta_microseconds = compute_delta_microseconds(section_start, labels_generation_finished);
    asprintf(&stats, "%sGenerated %s labels in %s.\n", stats, argv[2], get_human_readable_time(delta_microseconds));
    asprintf(&stats, "%s%s", stats, get_rss_virt_mem());

    //Test labels print
    // for(i=0; i<num_vertex; i++){
    //     printf("Node: %i ", i);
    //     for(j=0; j<d; j++){
    //         printf("[%i, %i] ", labels[i].lbl_start[j], labels[i].lbl_end[j]);
    //     }
    //     printf("\n");
    // }

    clock_gettime(CLOCK_MONOTONIC_RAW, &section_start);

    // Query reading
    fp_query = fopen(argv[3], "r");
    if (fp_query == NULL) {
        fprintf(stderr, "Unable to open file %s for reading.\n", argv[3] );
        exitWithDealloc(true, num_vertex, NULL, rows, threads, args, roots_mutex, roots, labels, fp_query, queries);
    }

    int a, b, num_query=0;

    while (fscanf(fp, "%*[^\n]\n") != -1) {
        num_query++;
    }

    printf("Query Number: %i\n", num_query);

    queries = malloc(sizeof(el_query)*num_query);
    if(queries == NULL){
        printf ("Not enough room for this size queries\n" );
        exitWithDealloc(true, num_vertex, NULL, rows, threads, args, roots_mutex, roots, labels, fp_query, queries);
    }

    fseek(fp_query, 0L, SEEK_SET);

    for(i=0; i<num_query; i++) {
        fscanf(fp_query, "%i %i  \n", &a, &b);
        queries[i].num[0] = a;
        queries[i].num[1] = b;
    }

    fclose(fp_query);

    // Test query print
    /*for(i=0; i<num_query; i++) {
        printf("Query %i: %i %i\n", i, queries[i].num[0], queries[i].num[1]);
    }*/

    clock_gettime(CLOCK_MONOTONIC_RAW, &file2_read);
    delta_microseconds = compute_delta_microseconds(section_start, file2_read);
    asprintf(&stats, "%sRead query file %s (file2) in %s.\n", stats, argv[3], get_human_readable_time(delta_microseconds));
    asprintf(&stats, "%s%s", stats, get_rss_virt_mem());
    
    //print_list_query(head_query);

    clock_gettime(CLOCK_MONOTONIC_RAW, &section_start);

    // Query Resolution
    visited = (bool **) malloc(num_threads * sizeof(bool *));
    if(visited == NULL){
        printf ("Not enough room for this size queries\n" );
        exitWithDealloc(true, num_vertex, NULL, rows, threads, args, roots_mutex, roots, labels, NULL, queries);
    }

    for(j=0; j<num_threads; j++) {
        visited[j] = (bool *) malloc(num_vertex * sizeof(bool));
        args[j].array_queries = queries;
        args[j].node_visited = visited[j];
        args[j].queries_num = num_query;
        args[j].num_labels = d;
        args[j].array_labels = labels;
        err_code = pthread_create(&threads[j], NULL, solveQuery, (void *)&args[j]);
        if(err_code) {
            printf ("Error number %i in thread %i creation.\n", err_code, j);
            exitWithDealloc(true, num_vertex, NULL, rows, threads, args, roots_mutex, roots, labels, NULL, queries);
        }
    }

    for(j=0; j < num_threads; j++) {
        err_code = pthread_join(threads[j], NULL);
        if(err_code) {
            printf ("Error number %i in thread %i joining.\n", err_code, j);
            exitWithDealloc(true, num_vertex, NULL, rows, threads, args, roots_mutex, roots, labels, NULL, queries);
        }
    }

    FILE *fp_res_query;
    fp_res_query = fopen("res_query_prova.txt", "w");

    if (fp_res_query == NULL) {
        fprintf(stderr, "Unable to open file for store the result of the query.\n");
        exitWithDealloc(true, num_vertex, NULL, rows, threads, args, roots_mutex, roots, labels, NULL, queries);
    }

    for(i=0; i<num_query; i++) {
        fprintf(fp_res_query, "%i %i %d\n", queries[i].num[0], queries[i].num[1], queries[i].can_reach ? 1 : 0);
    }

    fclose(fp_res_query);

    clock_gettime(CLOCK_MONOTONIC_RAW, &reachability_queries_finished);
    delta_microseconds = compute_delta_microseconds(section_start, reachability_queries_finished);
    asprintf(&stats, "%sTested %d reachability queries in %s.\n", stats, num_query, get_human_readable_time(delta_microseconds));
    asprintf(&stats, "%s%s", stats, get_rss_virt_mem());

    // Resource deallocation

    exitWithDealloc(false, num_vertex, NULL, rows, threads, args, roots_mutex, roots, labels, NULL, queries);

    clock_gettime(CLOCK_MONOTONIC_RAW, &program_finished);
    delta_microseconds = compute_delta_microseconds(program_start, program_finished);
    asprintf(&stats, "%sTotal program duration: %s.\n", stats, get_human_readable_time(delta_microseconds));

    asprintf(&stats, "%sThreads used: %u.\n", stats, num_threads);

    getrusage(RUSAGE_SELF, &memory);
    asprintf(&stats, "%sMaximum memory usage: %s\n", stats, get_human_readable_memory_usage(memory.ru_maxrss));

    fprintf(stdout, "\n\n------------STATISTICS------------\n%s", stats);

    return 0;
}