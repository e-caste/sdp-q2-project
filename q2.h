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

/*
 * used in get_human_readable_time
 * US = microseconds
 * H = hour, M = minute, S = second, MS = millisecond
*/
#define US_IN_H 3600000000
#define US_IN_M 60000000
#define US_IN_S 1000000
#define US_IN_MS 1000
