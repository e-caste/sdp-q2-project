#ifndef UTILITY_H
    #define UTILITY_H
    
    #include "main.h"

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

    long long unsigned compute_delta_microseconds(struct timespec start, struct timespec end);
    char* get_human_readable_time(long long unsigned microseconds);
    char* get_human_readable_memory_usage(long unsigned kilobytes);
    char* get_rss_virt_mem(void);
#endif //UTILITY_H