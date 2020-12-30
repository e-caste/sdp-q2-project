#include "utility.h"

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
        asprintf(&result, "less than 0 microseconds");
    else if (microseconds > 0 && microseconds < US_IN_MS)
        asprintf(&result, "%llu microseconds", microseconds);
    else if (microseconds >= US_IN_MS && microseconds < US_IN_S)
        asprintf(&result, "%llu milliseconds, %llu microseconds", ms, us);
    else if (microseconds >= US_IN_S && microseconds < US_IN_M)
        asprintf(&result, "%llu seconds, %llu milliseconds, %llu microseconds", s, ms, us);
    else if (microseconds >= US_IN_M && microseconds < US_IN_H)
        asprintf(&result, "%llu minutes, %llu seconds, %llu milliseconds, %llu microseconds", m, s, ms, us);
    else
        asprintf(&result, "%llu hours, %llu minutes, %llu seconds, %llu milliseconds, %llu microseconds", h, m, s, ms, us);
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
        asprintf(&result, "less than 0 KB");
    else if (kilobytes > 0 && kilobytes < KB_IN_MB)
        asprintf(&result, "%lu KB", kilobytes);
    else if (kilobytes >= KB_IN_MB && kilobytes < KB_IN_GB)
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
        return "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
    }
    fscanf(stat,
           "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %*u %*u %*d %*d %*d %*d %*d %*d %*u %lu %ld",
           &virt, &rss);
    fclose(stat);
    virt = (long unsigned) virt / 1024;
    rss = (long unsigned) rss * sysconf(_SC_PAGESIZE) / 1024;
    asprintf(&result,
             "Currently used memory (RAM): %s\n"
             "Currently used virtual memory (included pages): %s\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n",
             get_human_readable_memory_usage(rss),
             get_human_readable_memory_usage(virt));
    return result;
}
