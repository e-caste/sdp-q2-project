#include "utility.h"

// Scope of "scanFile" : Read 'file1'
// Each thread will read from inf to sup
// Inf and Sup are the input file row number: 0, 1, ..., 10,...
// In the following code, 'file1' will be splitted
// in NUM_THREADS, such as every thread can read in parallel.

void *scanFile(void *args) {
    t_args *my_data;
    my_data = (t_args *) args;
    FILE *fp;
    unsigned int j, i, k, pos;
    unsigned int sup, inf;
    unsigned int num_threads = my_data->total_threads;
    char *line_buf = NULL;
    ssize_t line_size;
    size_t line_buf_size;
    char a;
    int offset;
    char buffer[10];
    memset(buffer, 0, sizeof(char)*10);
    char space = ' ';
    int val;

    fp = fopen(my_data -> filename, "r");
    if (fp == NULL) {
        fprintf(stderr, "Unable to open file %s for reading, thread number %lu.\n", my_data -> filename, my_data -> id );
        exit(1);
    }

    //Definition of superior limit
    if (my_data->id == num_threads-1) 
        sup = my_data->total_vertex;
    else {
        // with fseek I go in a random position inside the row
        // So, after is done a check until is founded '\n' (10)
        // The first number of the next row will be the SUP.
        
        // NOTE: This is a good strategies since we don't have row with the same edge
        // NOTE: SUP for id=0 is INF for id=1. Range are contiguous so the whole file is guaranteed to be read
        fseek(fp, (long) my_data->size_file*(my_data->id+1)/num_threads, SEEK_SET); // 1Gb / 4Threads = 250Mb. T1=250, T2=500, ...
        while(i != 10) {
            i = getc(fp);
        }

        i=0;

        sf_fscanf(fp, "%lu", &sup);
    }

    //Definition of inferiror limit
    if (my_data->id == 0) {
        fseek(fp, 0L, SEEK_SET);
        sf_fscanf(fp, "%*[^\n]\n");    // avoid first row which contains the vertex number
        inf = 0;
    } else {
        // random position inside a row
        fseek(fp, (long) my_data->size_file*my_data->id/num_threads, SEEK_SET); // 1Gb / 4Thread = 250Mb ciascuno * id

        //search until we found '/n'
        while(i != 10) {    
            i = getc(fp);
        }
        
        pos = ftell(fp);            // save the initial offset of the row

        sf_fscanf(fp, "%lu", &inf);     // read row index number

        fseek(fp, pos, SEEK_SET);   //return at the begin of the row
    }    

    //printf("Thread n. %i con inf: %i e sup: %i\n", my_data->id, inf, sup-1);
    offset = 0;
    k = 0;
    
    for(j=inf; j<sup; j++) {        //row format example: "2: 8 7 3 #"
        sf_fscanf(fp, "%lu: ", &i);     // remove row number and ': '

        i=0;

        line_size = getline(&line_buf, &line_buf_size, fp);

        if(line_size != 2) {        //row format no edge: "3: #"    

            while(offset!=(line_size - 2)) {
                a = line_buf[offset];   //read char from line
                if(!strncmp(&a, &space, 1)){
                    k++;    // just count the number of edge
                }
                offset++;
            }

            my_data -> graph[j].edges = malloc(k*sizeof(unsigned int));
            if(my_data -> graph[j].edges == NULL) {
                printf ("Not enough room for array of edges size\n" );
                exit(1);
            }

            offset = 0;     //reset to begin of line
            k = 0;

            while(offset!=(line_size - 2)) { //insert in array value of edge

                a = line_buf[offset];

                while(strncmp(&a, &space, 1)) { //avoid white space
                    buffer[i] = a;
                    i++;
                    offset++;
                    a = line_buf[offset];
                }

                i=0;
                val = atoi(buffer);
                memset(buffer, 0, sizeof(char)*10);

                my_data -> graph[j].edges[k] = val;

                my_data -> graph[val].not_root = true;

                k++;
                offset++;
            }

            my_data -> graph[j].edge_num = k;

        } else { //The is no edge in the line
            my_data -> graph[j].edges = NULL;
            my_data -> graph[j].edge_num = 0;
        }

        k = 0;
        offset = 0;  // next vertex
    }

    //printf("Thread n. %i e ho finito\n", my_data->id);

    free(line_buf);

    //wait all threads fill shared graph.
    //NOTE: last thread unlock also main thread
    pthread_barrier_wait(my_data -> barrier);

    //START counting the roots

    // Split the vertex in N_thread. 
    // Each thread will count in parallel the roots
    if (my_data->id == num_threads-1) //main is not here
        sup = my_data->total_vertex;
    else
        sup = ((my_data->total_vertex) / num_threads) * (my_data->id + 1);

    if (my_data->id == 0)
        inf = 0;
    else
        inf = ((my_data->total_vertex)/num_threads)*(my_data->id);

    for(i=inf; i<sup; i++) {
        if(!my_data -> graph[i].not_root) { //if (is_root)
            pthread_mutex_lock(my_data->roots_mutex); //global mutex to protect shared var 'roots_num'
                j = (*(my_data -> roots_num));
                *my_data -> roots_num = j + 1;
                //printf("%i - ", *(my_data -> roots_num));
                //fflush(stdout);
            pthread_mutex_unlock(my_data->roots_mutex);
        }
    }

    //wait all threads count the number of roots
    //NOTE: last thread will unlock main thread also
    pthread_barrier_wait(my_data -> barrier);

    //NOTE: threads wait main thread allocate *shared* roots[]
    pthread_barrier_wait(my_data -> barrier);


    // START filling the roots[]
    for(i=inf; i<sup; i++) {
        if(!my_data->graph[i].not_root) {  //if (is root)
            pthread_mutex_lock(my_data->roots_mutex);   // protection for shared index array: 'root_index'
                j = (*(my_data->root_index));   //index
                (*my_data->roots)[j] = i;       //roots[index] = vertex#
                *my_data->root_index = j + 1;   //index++
            pthread_mutex_unlock(my_data->roots_mutex);
        }
    }

    pthread_exit((void *) 0);
}
