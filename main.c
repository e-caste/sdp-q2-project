#include "buildLabels.h"
#include "utility.h"

// argv[1]: file1 (input .gra)
// argv[2]: n (label number 'd')
// argv[3]: file2 (.que)
int main(int argc, char *argv[]) {
    FILE *fp = NULL, *fp_query = NULL;
    unsigned int num_threads;
    unsigned int num_vertex;
    unsigned int i, j, size;
    int err_code=0, d;
    row_g *rows = NULL;
    pthread_t *threads = NULL;
    t_args *args = NULL;
    // needed for labels generation
    unsigned int *roots = NULL;
    unsigned int roots_num, root_index;
    pthread_mutex_t *roots_mutex = NULL;
    pthread_barrier_t *barrier = NULL;
    row_l *labels = NULL;
    // needed for time and memory statistics
    struct timespec program_start, section_start, file1_read, file2_read, labels_generation_finished, reachability_queries_finished, program_finished;
    long long unsigned delta_microseconds;
    struct rusage memory;
    char* stats = NULL;
    // needed for reachability query
    int **visited = NULL;
    el_query *queries = NULL;
    unsigned int a, b, num_query=0;

    // Checks on arguments

    if (argc != 4 && argc != 5) {
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

    sf_fscanf(fp,"%u\n", &num_vertex);

    rows = (row_g *) malloc (num_vertex * sizeof (row_g));  //array of lists
    if (rows == NULL ) {
        printf ("Not enough room for this size graph\n" );
        exitWithDealloc(true, num_vertex, fp, rows, threads, args, roots_mutex, roots, labels, fp_query, queries);
    }

    // this is needed to prevent an infinite wait
    // when using very small graphs (e.g. 10 vertices) on heavily multithreaded CPUs (e.g. 24 threads)
    num_threads = NUM_THREADS > num_vertex ? num_vertex : NUM_THREADS;
    if (argc == 5) {
        // the number of threads is set from the command line
        i = atoi(argv[4]);
        num_threads = i > num_vertex ? num_vertex : i;
    }

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

    roots_mutex = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
    if (roots_mutex == NULL ) {
        printf ("Error in creating mutex protection for roots counter \n" );
        exitWithDealloc(true, num_vertex, NULL, rows, threads, args, roots_mutex, roots, labels, fp_query, queries);
    }

    barrier = (pthread_barrier_t *) malloc(sizeof(pthread_barrier_t));
    if (barrier == NULL ) {
        printf ("Error in creating barrier \n" );
        exitWithDealloc(true, num_vertex, NULL, rows, threads, args, roots_mutex, roots, labels, fp_query, queries);
    }

    if(pthread_mutex_init(roots_mutex, NULL) != 0){
        printf ("Error in initializing mutex protection for roots counter \n" );
        exitWithDealloc(true, num_vertex, NULL, rows, threads, args, roots_mutex, roots, labels, fp_query, queries);
    }

    if(pthread_barrier_init(barrier, NULL, num_threads + 1) != 0){
        printf ("Error in initializing barrier \n" );
        exitWithDealloc(true, num_vertex, NULL, rows, threads, args, roots_mutex, roots, labels, fp_query, queries);
    }

    root_index = 0;
    roots_num = 0;

    // Preparing structure for parallel reading of file1 by many threads
    for(j=0; j < num_threads; j++) {
        args[j].filename = argv[1];
        args[j].graph = rows;
        args[j].total_vertex = num_vertex;
        args[j].total_threads = num_threads;
        args[j].size_file = size;
        args[j].roots_num = &roots_num;             // pointer of 'shared' variable
        args[j].roots_mutex = roots_mutex;          // protection for 'shared' variable
        args[j].barrier = barrier;
        args[j].roots = &roots;
        args[j].root_index = &root_index;           // shared variable for roots[] parallel initializzation -> need protection
    }

    printf("Starting the reading of DAG file...\n");

    for(i=0; i<num_threads; i++) {
        args[i].id = i;
        err_code = pthread_create(&threads[i], NULL, scanFile, (void *)&args[i]);
        if(err_code) {
            printf ("Error number %i in thread %u creation.\n", err_code, i);
            exitWithDealloc(true, num_vertex, NULL, rows, threads, args, roots_mutex, roots, labels, fp_query, queries);
        }
    }

    // Wait until all thread end
    pthread_barrier_wait(barrier);

    // Test of Graph print
    /*for(i=0; i<num_vertex; i++) {
        printf("%i : ", i);
        for(j=0; j<rows[i].edge_num; j++){
            printf("%i ", rows[i].edges[j]);
        }
        printf(" -----> num_edge: %i\n", rows[i].edge_num);
        printf("\n");
    }*/

    fprintf(stdout, "Starting roots search...\n");

    // wait threads counts number of roots
    pthread_barrier_wait(barrier);

    roots = (unsigned int *) malloc(roots_num * sizeof(unsigned int));
    if (roots == NULL ) {
        printf ("Error in creating roots struct\n" );
        exitWithDealloc(true, num_vertex, NULL, rows, threads, args, roots_mutex, roots, labels, fp_query, queries);
    }

    //wait all threads initialize the roots[]
    pthread_barrier_wait(barrier);

    for(j=0; j < num_threads; j++) {
        err_code = pthread_join(threads[j], NULL);
        if(err_code) {
            printf ("Error number %i in thread %u joining.\n", err_code, j);
            exitWithDealloc(true, num_vertex, NULL, rows, threads, args, roots_mutex, roots, labels, fp_query, queries);
        }
    }

    if(barrier){
        pthread_barrier_destroy(barrier);
        free(barrier);
    }

    fprintf(stdout, "End of root search...\n");

    clock_gettime(CLOCK_MONOTONIC_RAW, &file1_read);
    delta_microseconds = compute_delta_microseconds(section_start, file1_read);
    sf_asprintf(&stats, "Read input file %s (file1) in %s.\n", argv[1], get_human_readable_time(delta_microseconds));
    sf_asprintf(&stats, "%s%s", stats, get_rss_virt_mem());
    fprintf(stdout, "End of DAG file reading...\n");

    // ignore labels
    if (d == 0) {
        clock_gettime(CLOCK_MONOTONIC_RAW, &program_finished);
        delta_microseconds = compute_delta_microseconds(program_start, program_finished);
        sf_asprintf(&stats, "%sTotal program duration: %s.\n", stats, get_human_readable_time(delta_microseconds));
        sf_asprintf(&stats, "%sThreads used: %u.\n", stats, num_threads);
        getrusage(RUSAGE_SELF, &memory);
        sf_asprintf(&stats, "%sMaximum memory usage: %s\n", stats, get_human_readable_memory_usage(memory.ru_maxrss));
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

    clock_gettime(CLOCK_MONOTONIC_RAW, &section_start);

    labels = (row_l *) malloc (num_vertex * sizeof (row_l));
    if (labels == NULL ) {
        printf ("Not enough room for this size labels\n" );
        exitWithDealloc(true, num_vertex, NULL, rows, threads, args, roots_mutex, roots, labels, fp_query, queries);
    }

    for(i=0; i<num_vertex; i++){
        labels[i].lbl_start = (unsigned int *) malloc (d * sizeof (unsigned int));
        labels[i].lbl_end = (unsigned int *) malloc (d * sizeof (unsigned int));
        labels[i].visited = (bool *) malloc (d * sizeof (bool));
        if ((labels[i].lbl_start == NULL ) || (labels[i].lbl_end == NULL ) || (labels[i].visited == NULL )) {
            printf ("Not enough room for this size labels\n" );
            exitWithDealloc(true, num_vertex, NULL, rows, threads, args, roots_mutex, roots, labels, fp_query, queries);
        }

        //It's not always true that these values are resetted
        memset(labels[i].lbl_start, 0, d * sizeof(unsigned int));
        memset(labels[i].lbl_end, 0, d * sizeof(unsigned int));
        memset(labels[i].visited, false, d * sizeof(bool));

        //for parallelize 1 thread for each children (labels)
        // OR 1 thread for each label + some threads for roots
//        rows[i].node_mutex = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
//        if (rows[i].node_mutex  == NULL ) {
//            printf ("Error in creating mutex protection for node\n" );
//            exitWithDealloc(true, num_vertex, NULL, rows, threads, args, roots_mutex, roots, labels, fp_query, queries);
//        }
//
//        if(pthread_mutex_init(rows[i].node_mutex , NULL) != 0){
//            printf ("Error in initializing mutex protection for node\n" );
//            exitWithDealloc(true, num_vertex, NULL, rows, threads, args, roots_mutex, roots, labels, fp_query, queries);
//        }
    }

    fprintf(stdout, "Starting label creation...\n");
    RandomizedLabelingParallelInit(rows, labels, d, num_vertex, roots, roots_num, num_threads);
    //RandomizedLabelingSequential(rows, labels, d, num_vertex, roots, roots_num);
    fprintf(stdout, "End Label creation...\n");

    fflush(stdout);

    clock_gettime(CLOCK_MONOTONIC_RAW, &labels_generation_finished);
    delta_microseconds = compute_delta_microseconds(section_start, labels_generation_finished);
    sf_asprintf(&stats, "%sGenerated %s labels in %s.\n", stats, argv[2], get_human_readable_time(delta_microseconds));
    sf_asprintf(&stats, "%s%s", stats, get_rss_virt_mem());

    //Test labels print
    // for(i=0; i<num_vertex; i++){
    //     printf("Node: %i ", i);
    //     for(j=0; j<d; j++){
    //         printf("[%i, %i] ", labels[i].lbl_start[j], labels[i].lbl_end[j]);
    //     }
    //     printf("\n");
    // }

    // Deallocation not useful anymore structure
    if(roots_mutex){
        pthread_mutex_destroy(roots_mutex);
        free(roots_mutex);
        roots_mutex = NULL; 
    }

    if(roots){
        free(roots);
        roots = NULL;
    }

    for(i=0; i<num_vertex; i++){
        if(rows && rows[i].node_mutex){
            pthread_mutex_destroy(rows[i].node_mutex);
            free(rows[i].node_mutex);
            rows[i].node_mutex = NULL;
        }
    }

    clock_gettime(CLOCK_MONOTONIC_RAW, &section_start);

    // Query reading
    fp_query = fopen(argv[3], "r");
    if (fp_query == NULL) {
        fprintf(stderr, "Unable to open file %s for reading.\n", argv[3] );
        exitWithDealloc(true, num_vertex, NULL, rows, threads, args, roots_mutex, roots, labels, fp_query, queries);
    }

    while (fscanf(fp, "%*[^\n]\n") != -1) {
        num_query++;
    }

    printf("Query Number: %u\n", num_query);

    queries = malloc(sizeof(el_query)*num_query);
    if(queries == NULL){
        printf ("Not enough room for this size queries\n" );
        exitWithDealloc(true, num_vertex, NULL, rows, threads, args, roots_mutex, roots, labels, fp_query, queries);
    }

    fseek(fp_query, 0L, SEEK_SET);

    for(i=0; i<num_query; i++) {
        sf_fscanf(fp_query, "%u %u  \n", &a, &b);
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
    sf_asprintf(&stats, "%sRead query file %s (file2) in %s.\n", stats, argv[3], get_human_readable_time(delta_microseconds));
    sf_asprintf(&stats, "%s%s", stats, get_rss_virt_mem());


    clock_gettime(CLOCK_MONOTONIC_RAW, &section_start);

    // Query Resolution
    visited = (int **) malloc(num_threads * sizeof(int *));
    if(visited == NULL){
        printf ("Not enough room for this size queries\n" );
        exitWithDealloc(true, num_vertex, NULL, rows, threads, args, roots_mutex, roots, labels, NULL, queries);
    }

    for(j=0; j<num_threads; j++) {
        visited[j] = (int *) malloc(num_vertex * sizeof(int));
        args[j].array_queries = queries;
        args[j].node_visited = visited[j];
        args[j].queries_num = num_query;
        args[j].num_labels = d;
        args[j].array_labels = labels;
        err_code = pthread_create(&threads[j], NULL, solveQuery, (void *)&args[j]);
        if(err_code) {
            printf ("Error number %i in thread %u creation.\n", err_code, j);
            exitWithDealloc(true, num_vertex, NULL, rows, threads, args, roots_mutex, roots, labels, NULL, queries);
        }
    }

    for(j=0; j < num_threads; j++) {
        err_code = pthread_join(threads[j], NULL);
        if(err_code) {
            printf ("Error number %i in thread %u joining.\n", err_code, j);
            exitWithDealloc(true, num_vertex, NULL, rows, threads, args, roots_mutex, roots, labels, NULL, queries);
        }
    }

    clock_gettime(CLOCK_MONOTONIC_RAW, &reachability_queries_finished);
    delta_microseconds = compute_delta_microseconds(section_start, reachability_queries_finished);
    sf_asprintf(&stats, "%sTested %d reachability queries in %s.\n", stats, num_query, get_human_readable_time(delta_microseconds));
    sf_asprintf(&stats, "%s%s", stats, get_rss_virt_mem());

    FILE *fp_res_query;
    fp_res_query = fopen("res_query.txt", "w");

    if (fp_res_query == NULL) {
        fprintf(stderr, "Unable to open file for store the result of the query.\n");
        exitWithDealloc(true, num_vertex, NULL, rows, threads, args, roots_mutex, roots, labels, NULL, queries);
    }

    for(i=0; i<num_query; i++) {
        fprintf(fp_res_query, "%u %u %d\n", queries[i].num[0], queries[i].num[1], queries[i].can_reach ? 1 : 0);
    }

    fclose(fp_res_query);

    // Resource deallocation

    exitWithDealloc(false, num_vertex, NULL, rows, threads, args, roots_mutex, roots, labels, NULL, queries);

    clock_gettime(CLOCK_MONOTONIC_RAW, &program_finished);
    delta_microseconds = compute_delta_microseconds(program_start, program_finished);
    sf_asprintf(&stats, "%sTotal program duration: %s.\n", stats, get_human_readable_time(delta_microseconds));

    sf_asprintf(&stats, "%sThreads used: %u.\n", stats, num_threads);

    getrusage(RUSAGE_SELF, &memory);
    sf_asprintf(&stats, "%sMaximum memory usage: %s\n", stats, get_human_readable_memory_usage(memory.ru_maxrss));

    fprintf(stdout, "\n\n------------STATISTICS------------\n%s", stats);

    return 0;
}