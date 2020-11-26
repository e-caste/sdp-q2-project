#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>

#define N 4

typedef struct edge_list {
    int num;
    struct edge_list *next_num;
} edge;

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

void fake_push(edge *head, int val) {
    edge *next = head -> next_num;

    head -> next_num = (edge *) malloc(sizeof(edge));
    head -> next_num -> num = val;  
    head -> next_num -> next_num = next;
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

typedef struct row_graph {
    int edge_num;
    int not_root;
    edge *edges_pointer;
} row_g;

typedef struct thread_args {
    int id;
    int total_vertex;
    int size_file;
    char *filename;
    row_g *graph;
    edge *roots;
} t_args;

void *scanFile(void *args) {
    t_args *my_data;
    my_data = (t_args *) args;
    FILE *fp;
    int j, i, k, c, pos;
    int sup, inf;

    fp = fopen(my_data -> filename, "r");
    if (fp == NULL) {
        fprintf(stderr, "Unable to open file %s for reading, thread number %i.\n", my_data -> filename, my_data -> id );
        exit(1);
    }

    if (my_data->id == N-1) sup = my_data->total_vertex;

    else {
        fseek(fp, my_data->size_file*(my_data->id+1)/N, SEEK_SET);
        while(i != 10) {
            i = getc(fp);
        }

        i=0;

        fscanf(fp, "%i", &sup);
    }


    if (my_data->id == 0) {
        fseek(fp, 0L, SEEK_SET);
        fscanf(fp, "%*[^\n]\n");
        inf = 0;
    } else {
        fseek(fp, my_data->size_file*my_data->id/N, SEEK_SET);

        while(i != 10) {
            i = getc(fp);
        }

        pos = ftell(fp);

        fscanf(fp, "%i", &inf);

        fseek(fp, pos, SEEK_SET);
    }    

    //printf("Thread n. %i con inf: %i e sup: %i\n", my_data->id, inf, sup-1);

    for(j=inf; j<sup; j++) {
        fscanf(fp, "%i: ", &i);
        /*printf("%i ", j);
        fflush(stdout);*/
        edge *head;
        head = malloc(sizeof(edge));
        do {
            i = -1;
            if(k !=0) ungetc(c, fp);
            fscanf(fp, "%i ", &i);
            if(i != -1) {
                if(k==0) {
                    create_list(head, i);
                    my_data -> graph[i].not_root = 1;
                } else {
                    push(head, i);
                    my_data -> graph[i].not_root = 1;
                }
            }
            c = fgetc(fp);
            k++;
        } while(c != 35);
        if(i==-1) {
            my_data -> graph[j].edges_pointer = NULL;
            my_data -> graph[j].edge_num = 0;
        } else {
            my_data -> graph[j].edges_pointer = head;
            my_data -> graph[j].edge_num = k;
        }
        k = 0;
    }

    //printf("Thread n. %i e ho finito\n", my_data->id);

    pthread_exit((void *) 0);

}

void *scanRoots(void *args) {
    t_args *my_data;
    my_data = (t_args *) args;
    int sup, inf, i;

    if (my_data->id == N-1) sup = my_data->total_vertex - 1;

    else
        sup = ((my_data->total_vertex)/N)*(my_data->id+1);


    if (my_data->id == 0) {
        inf = 0;
    } else {
        inf = ((my_data->total_vertex)/N)*(my_data->id);
    }    

    /*printf("Sono il thread %i con inf %i e sup %i.\n", my_data->id, inf, sup-1);
    fflush(stdout);*/

    for(i=sup; i>inf; i--) {
        if(my_data->graph[i].not_root == 0) {
            fake_push(my_data->roots, i);
        }
    }

    /*printf("Sono il thread %i e ho finito.\n", my_data->id);
    fflush(stdout);*/

    pthread_exit((void *) 0);
}

int main(int argc, char *argv[]) {

    FILE *fp;
    unsigned int num_vertex;
    int i, j, size, c=0, k=0, err_code=0;
    row_g *rows;
    pthread_t threads[N];
    t_args args[N];
    edge* roots;
    

    // Controllo sugli argomenti

    if (argc != 2) {
        fprintf(stderr, "For the moment, enter only one argument!\n");
        exit(1);
    }

    // Apertura file1

    fp = fopen(argv[1], "r");

    if (fp == NULL) {
        fprintf(stderr, "Unable to open file %s for reading.\n", argv[1] );
        exit(1);
    }

    // Prendere numero vertici per allocare la giusta memoria

    fscanf(fp,"%i\n", &num_vertex);

    rows = (row_g *) malloc (num_vertex * sizeof (row_g));
    if (rows == NULL ) {
        printf ("Not enough room for this size graph\n" );
        return 0;
    }

    // Prendere grandezza file per dividerlo per fseek

    fseek(fp, 0L, SEEK_END);
    size = ftell(fp);

    fclose(fp);

    roots = malloc(sizeof(edge));

    if (roots == NULL ) {
        printf ("Not enough room for this size graph\n" );
        return 0;
    }
    create_list(roots, 0);

    // Creazione thread
    for(j=0; j<N; j++) {
        args[j].filename = argv[1];
        args[j].graph = rows;
        args[j].total_vertex = num_vertex;
        args[j].size_file = size;
        args[j].roots = roots;
    }

    for(i=0; i<N; i++) {
        args[i].id = i;
        err_code = pthread_create(&threads[i], NULL, scanFile, (void *)&args[i]);
        if(err_code) {
            printf ("Errore numero %i nella creazione del thread %i.\n", err_code, i);
            return 0;
        }
    }

    // Aspettare che i thread finiscano

    for(j=0; j<N; j++) {
        err_code = pthread_join(threads[j], NULL);
        if(err_code) {
            printf ("Errore numero %i nel joining del thread %i.\n", err_code, j);
            return 0;
        }
    }


    // Creazione thread per lettura radici

    for(i=0; i<N; i++) {
        args[i].id = i;
        err_code = pthread_create(&threads[i], NULL, scanRoots, (void *)&args[i]);
        if(err_code) {
            printf ("Errore numero %i nella creazione del thread %i.\n", err_code, i);
            return 0;
        }
    }

    // Aspettare che i thread finiscano

    for(j=0; j<N; j++) {
        err_code = pthread_join(threads[j], NULL);
        if(err_code) {
            printf ("Errore numero %i nel joining del thread %i.\n", err_code, j);
            return 0;
        }
    }

    // Stampa di prova radici
    
    /*printf("Le radici sono: ");
    print_list(roots);
    printf("\n");*/

    // Stampa di prova

    /*for(i=0; i<num_vertex; i++) {
        printf("%i : ", i);
        print_list(rows[i].edges_pointer);
        printf("\n");
    }*/

    // Deallocazione di tutte le risorse

    free_list(roots);

    for(i=0; i<num_vertex; i++) {
        free_list(rows[i].edges_pointer);
    }

    free(rows);

    return 0;
}