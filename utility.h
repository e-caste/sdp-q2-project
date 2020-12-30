#ifndef UTILITY_H
    #define UTILITY_H
    
    #include <stdio.h>
    #include <stdlib.h>
    #include <pthread.h>
    #include <unistd.h>
    #include <stdbool.h>
    #include <time.h>
    #include <string.h>
    #include <sys/resource.h>
    
    #include "readGraph.h"  //for exitWithDealloc

    #define NUM_THREADS sysconf(_SC_NPROCESSORS_ONLN)

    /*
    * used in get_human_readable_time
    * US = microseconds
    * H = hour, M = minute, S = second, MS = millisecond
    */
    #define US_IN_H 3600000000
    #define US_IN_M 60000000
    #define US_IN_S 1000000
    #define US_IN_MS 1000

    /*
    * used in get_human_readable_memory_usage
    * KB = kilobytes
    * GB = gigabytes, MB = megabytes
    */
    #define KB_IN_GB 1048576  // = 1024 * 1024
    #define KB_IN_MB 1024

    #define _GNU_SOURCE  // allow usage of asprintf on GNU/Linux

    // see https://iq.opengenus.org/detect-operating-system-in-c/
    // used to print the used memory correctly on macOS and GNU/Linux
    #ifdef __APPLE__
        #define MEM_SIZE 1024
    #else
        #define MEM_SIZE 1
    #endif  

    //function for statistics
    long long unsigned compute_delta_microseconds(struct timespec start, struct timespec end);
    char* get_human_readable_time(long long unsigned microseconds);
    char* get_human_readable_memory_usage(long unsigned kilobytes);
    char* get_rss_virt_mem(void);

    //function for correct program exits
    //TODO void exitWithDealloc(bool error, unsigned int num_vertex, FILE * fp_dag, row_g *rows, pthread_t *threads, t_args *args, pthread_mutex_t *roots_mutex, int *roots, row_l *labels, FILE *fp_query, el_query *queries);
#endif //UTILITY_H