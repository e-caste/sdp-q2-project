#define _GNU_SOURCE 1  // allow usage of vasprintf on GNU/Linux
#include "utility.h"

// safe asprintf checks for memory allocation errors
// removes -Wunused-result gcc warnings
void sf_asprintf(char **strp, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    if (vasprintf(strp, fmt, ap) < 0) {
        fprintf(stderr, "Error while allocating memory for asprintf.\n");
        exit(-1);
    }
    va_end(ap);
}

// safe fscanf checks for number of read items
void sf_fscanf(FILE* fp, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    if (vfscanf(fp, fmt, ap) == EOF) {
        fprintf(stderr, "Error while reading variables from file with fscanf.\n");
        exit(-1);
    }
    va_end(ap);
}

// see https://stackoverflow.com/a/10192994
long long unsigned compute_delta_microseconds(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec - start.tv_nsec) / 1000;
}

// asprintf automatically allocates the needed memory - see https://stackoverflow.com/a/23842944
char* get_human_readable_time(long long unsigned microseconds) {
    long long unsigned us, ms, s, m, h;
    char* result;
    h = (long long unsigned) microseconds / US_IN_H;
    m = (long long unsigned) (microseconds - (h * US_IN_H)) / US_IN_M;
    s = (long long unsigned) (microseconds - (h * US_IN_H + m * US_IN_M)) / US_IN_S;
    ms = (long long unsigned) (microseconds - (h * US_IN_H + m * US_IN_M + s * US_IN_S)) / US_IN_MS;
    us = (long long unsigned) microseconds - (h * US_IN_H + m * US_IN_M + s * US_IN_S + ms * US_IN_MS);
    if (microseconds <= 0)
        sf_asprintf(&result, "less than 1 microsecond");
    else if (microseconds > 0 && microseconds < US_IN_MS)
        sf_asprintf(&result, "%llu microseconds", microseconds);
    else if (microseconds >= US_IN_MS && microseconds < US_IN_S)
        sf_asprintf(&result, "%llu milliseconds, %llu microseconds", ms, us);
    else if (microseconds >= US_IN_S && microseconds < US_IN_M)
        sf_asprintf(&result, "%llu seconds, %llu milliseconds, %llu microseconds", s, ms, us);
    else if (microseconds >= US_IN_M && microseconds < US_IN_H)
        sf_asprintf(&result, "%llu minutes, %llu seconds, %llu milliseconds, %llu microseconds", m, s, ms, us);
    else
        sf_asprintf(&result, "%llu hours, %llu minutes, %llu seconds, %llu milliseconds, %llu microseconds", h, m, s, ms, us);
    return result;
}

char* get_human_readable_memory_usage(long unsigned kilobytes) {
    long unsigned kb, mb, gb;
    char* result;
    kilobytes = kilobytes / MEM_SIZE;
    gb = (long unsigned) kilobytes / KB_IN_GB;
    mb = (long unsigned) (kilobytes - (gb * KB_IN_GB)) / KB_IN_MB;
    kb = (long unsigned) kilobytes - (gb * KB_IN_GB + mb * KB_IN_MB);
    if (kilobytes <= 0)
        sf_asprintf(&result, "less than 1 KB");
    else if (kilobytes > 0 && kilobytes < KB_IN_MB)
        sf_asprintf(&result, "%lu KB", kilobytes);
    else if (kilobytes >= KB_IN_MB && kilobytes < KB_IN_GB)
        sf_asprintf(&result, "%lu MB %lu KB", mb, kb);
    else
        sf_asprintf(&result, "%lu GB %lu MB %lu KB", gb, mb, kb);
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
        return "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
    }
    sf_fscanf(stat,
              "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %*u %*u %*d %*d %*d %*d %*d %*d %*u %lu %ld",
              &virt, &rss);
    fclose(stat);
    virt = (long unsigned) virt / 1024;
    rss = (long unsigned) rss * sysconf(_SC_PAGESIZE) / 1024;
    sf_asprintf(&result,
                "Currently used memory (RAM): %s\n"
                 "Currently used virtual memory (included pages): %s\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n",
                 get_human_readable_memory_usage(rss),
                 get_human_readable_memory_usage(virt));
    return result;
}


void exitWithDealloc(bool error, unsigned long num_vertex, FILE * fp_dag, row_g *rows, pthread_t *threads, t_args *args, pthread_mutex_t *roots_mutex, unsigned long *roots, row_l *labels, FILE *fp_query, el_query *queries){
    //1. error in: rows = (row_g *) malloc (num_vertex * sizeof (row_g));
    if(fp_dag)
        fclose(fp_dag);

    //2. error in: threads = (pthread_t *) malloc(num_threads * sizeof(pthread_t));
    if(rows){
        //6. error in: err_code = pthread_create(&threads[i], NULL, scanFile, (void *)&args[i]);
        //7. error in: err_code = pthread_join(threads[j], NULL);
        //8. error in: roots = (int *) malloc(roots_num * sizeof(int));
        for(int i=0; i<num_vertex; i++) {
                free(rows[i].edges);
                if(rows[i].node_mutex){
                    pthread_mutex_destroy(rows[i].node_mutex);
                    free(rows[i].node_mutex);
                }
        }
        free(rows);
    }

    //3. error in: args = (t_args *) malloc(num_threads * sizeof(t_args));
    if(threads)
        free(threads);

    //4. error in: roots_mutex = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
    if(args)
        free(args);

    //5. error in: if(pthread_mutex_init(roots_mutex, NULL) != 0){
    if(roots_mutex){
        pthread_mutex_destroy(roots_mutex);
        free(roots_mutex);
    }

    //9. error in: err_code = pthread_create(&threads[i], NULL, scanRoots, (void *)&args[i]);
    free(roots);

    //10. error in: if ((labels[i].lbl_start == NULL ) || (labels[i].lbl_end == NULL ) || (labels[i].visited == NULL )) {
    if(labels){
        for(int i=0; i<num_vertex; i++){
            if(labels[i].lbl_start)
                free(labels[i].lbl_start);
            if(labels[i].lbl_end)
                free(labels[i].lbl_end);
            if(labels[i].visited)
                free(labels[i].visited);
        }
        free(labels);
    }

    //11. error in: el_query *queries = malloc(sizeof(el_query)*num_query);
    if(fp_query)
        fclose(fp_query);
    
    //12. error in: visited = (bool **) malloc(num_threads * sizeof(bool *));
    if(queries)
        free(queries);

    if(error){
        fprintf(stdout, "Deallocation (with error) successful!\n");
        exit(1);
    }else{
        fprintf(stdout, "Deallocation successful!\n");
        return;
    }
}

//Swap e randomize are utility functions used for randomize the roots
void swap (unsigned long *a, unsigned long *b) {
    unsigned long temp = *a;
    *a = *b;
    *b = temp;
}

void randomize(unsigned long *array, unsigned long n) {
    for(unsigned long i=n-1; i>0; i--) {
        unsigned long j = rand() % (i+1);
        if (i != j)
            swap(&array[i], &array[j]);
    }
}

