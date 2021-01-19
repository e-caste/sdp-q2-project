#include "readGraph.h"

// Scopo di "scanFile" : Leggere 'file1'
// ogni thread leggerà da inf a sup.
// Inf e Sup sono il numero di riga del file di input 0, 1, ..., 10,...
// Nel seguente codice, si sta per dividere il file da leggere
// in NUM_THREADS parti, così che ogni thread legga in parallelo
// durante la lettura del grafo, si memorizzano alcune informazioni
// necessarie per la creazione successiva delle labels (roots_num)

void *scanFile(void *args) {
    t_args *my_data;
    my_data = (t_args *) args;
    FILE *fp;
    int j, i, k, pos;
    int sup, inf;
    unsigned int num_threads = my_data->total_threads;
    bool not_root_is_set;
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
        fprintf(stderr, "Unable to open file %s for reading, thread number %i.\n", my_data -> filename, my_data -> id );
        exit(1);
    }

    //Definizione estremo superiore
    if (my_data->id == num_threads-1) 
        sup = my_data->total_vertex;
    else {
        //con fseek mi metto in un punto casuale all'interno della riga
        //successivamente controllo finchè non trovo '\n' (10)
        //prendendo un carattere alla volta e "scartandolo"
        //quando trovo la nuova riga come primo intero avrò il SUP
        //NOTA: questa un'ottima strategia in quanto abbiamo righe non omogenee
        // sup for id=0 is inf for id=1, they are contiguous so the whole file is guaranteed to be read
        fseek(fp, (long) my_data->size_file*(my_data->id+1)/num_threads, SEEK_SET); // 1Gb / 4Thread = 250Mb ciascuno * id+1
        while(i != 10) {
            i = getc(fp);
        }

        i=0;

        fscanf(fp, "%i", &sup);
    }

    //Definizione estremo inferiore
    if (my_data->id == 0) {
        fseek(fp, 0L, SEEK_SET);
        fscanf(fp, "%*[^\n]\n");    // salto la prima riga (contiene il numero di vertici totale)
        inf = 0;
    } else {
        fseek(fp, (long) my_data->size_file*my_data->id/num_threads, SEEK_SET); // 1Gb / 4Thread = 250Mb ciascuno * id

        while(i != 10) {    
            i = getc(fp);
        }

        pos = ftell(fp);            //conservo la posizione di inizio riga

        fscanf(fp, "%i", &inf);     // leggo l'indice della riga

        fseek(fp, pos, SEEK_SET);   //torno all'inizio della riga
    }    

    //printf("Thread n. %i con inf: %i e sup: %i\n", my_data->id, inf, sup-1);
    offset = 0;
    
    //adesso per le righe di competenza del thread, si legge il valore del nodo
    //si costruisce e inizializza la struttura apposita e si inserisce in lista
    for(j=inf; j<sup; j++) {
        fscanf(fp, "%i: ", &i);     //scarto il numero della riga (== j) e anche ": "
        edge *head;
        head = malloc(sizeof(edge));
        i=0;

        line_size = getline(&line_buf, &line_buf_size, fp);

        if(line_size != 2) {

            while(offset!=(line_size - 2)) {

                a = line_buf[offset];

                while(strncmp(&a, &space, 1)) {
                    buffer[i] = a;
                    i++;
                    offset++;
                    a = line_buf[offset];
                }

                i=0;
                val = atoi(buffer);
                memset(buffer, 0, sizeof(char)*10);

                if(k==0) {
                    create_list(head, val);
                } else {
                    push(head, val);
                }

                my_data -> graph[val].not_root = true;

                /*pthread_mutex_lock(my_data->roots_mutex);
                    not_root_is_set = my_data->graph[val].not_root ? true : false;  // default == 0 == false;
                    my_data -> graph[val].not_root = true;   //se era a uno lo rimetto a 1
                    if ((!not_root_is_set) && (my_data->graph[val].not_root)){ // E' considerata radice ogni nodo senza genitori (not_root = false)
                            *my_data->roots_num = (*(my_data->roots_num) - 1);
                    }
                pthread_mutex_unlock(my_data->roots_mutex);*/

                k++;
                offset++;
            }

            my_data -> graph[j].edges_pointer = head;
            my_data -> graph[j].edge_num = k;

        } else { //non prendo nessun intero -> lista archi vuota
            my_data -> graph[j].edges_pointer = NULL;
            my_data -> graph[j].edge_num = 0;
        }

        /*my_data->graph[j].node_mutex = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
        if (my_data->graph[j].node_mutex == NULL ) {
            printf ("Error in creating mutex protection for node\n" );
            exit(1);
        }

        if(pthread_mutex_init(my_data->graph[j].node_mutex, NULL) != 0){
            printf ("Error in initializing mutex protection for node\n" );
            exit(1);
        }*/

        k = 0;
        offset = 0;  //vertice successivo
    }

    //printf("Thread n. %i e ho finito\n", my_data->id);

    free(line_buf);

    pthread_exit((void *) 0);

}

void create_list(edge *head, int val) {
    edge *current = head;
    current -> num = val;
    current -> next_num = NULL;
}

void push(edge *head, int val) {
    edge *current = head;

    while(current -> next_num != NULL) {
        current = current -> next_num;
    }

    current -> next_num = (edge *) malloc(sizeof(edge));
    current -> next_num -> num = val;  
    current -> next_num -> next_num = NULL;
}

void print_list(edge *head) {
    edge *current = head;

    while(current != NULL) {
        printf("%i ", current -> num);
        current = current -> next_num;
    }
}

void free_list(edge *head) {
    edge *tmp;

    while(head != NULL) {
        tmp = head;
        head = head -> next_num;
        free(tmp);
    }
}


//Swap e randomize sono funzioni di utilità per randomizzare Roots
void swap (int *a, int *b) {
    int temp = *a;
    *a = *b;
    *b = temp;
}

void randomize(int *array, int n) {
    for(int i=n-1; i>0; i--) {
        int j = rand() % (i+1);
        if (i != j)
            swap(&array[i], &array[j]);
    }
}


//Scopo di "scanRoots": inizializzare l'array di roots
//Questa è una funzione parallela, quindi i thread si
//dividono il grafo da leggere con lo stesso meccanismo
//usato anche in scanFile.
//Tale array verrà successivamente randomizzato ed
//utilizzato per creare le labels

void *scanRoots(void *args) {
    t_args *my_data;
    my_data = (t_args *) args;
    int sup, inf, i, j;
    unsigned int num_threads = my_data->total_threads;

    //Definition of 'Sup' and 'Inf' so that each thread
    //can read in parallel the graph
    //see scanFile for more details
    if (my_data->id == num_threads-1)
        sup = my_data->total_vertex;
    else
        sup = ((my_data->total_vertex) / num_threads) * (my_data->id + 1);      //TODO e' necessario cast(int)?

    if (my_data->id == 0)
        inf = 0;
    else
        inf = ((my_data->total_vertex)/num_threads)*(my_data->id);

    for(i=inf; i<sup; i++) {
        if(!my_data->graph[i].not_root) {  //if (is root)
            pthread_mutex_lock(my_data->roots_mutex);
                j = (*(my_data->root_index));
                my_data->roots[j] = i;
                *my_data->root_index = j + 1;
            pthread_mutex_unlock(my_data->roots_mutex);

        }
    }

    pthread_exit((void *) 0);
}
