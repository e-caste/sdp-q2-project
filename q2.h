//
// Created by Enrico Castelli on 28/11/2020.
//

#ifndef SDP_Q2_PROJECT_Q2_H
#define SDP_Q2_PROJECT_Q2_H

#endif //SDP_Q2_PROJECT_Q2_H

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>
#include <sys/resource.h>

#define _GNU_SOURCE  // allow usage of asprintf on GNU/Linux
#define NUM_THREADS sysconf(_SC_NPROCESSORS_ONLN)

// see https://iq.opengenus.org/detect-operating-system-in-c/
// used to print the used memory correctly on macOS and GNU/Linux
#ifdef __APPLE__
    #define MEM_SIZE 1024
#else
    #define MEM_SIZE 1
#endif

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