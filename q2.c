#include <stdio.h>
#include <stdlib.h>

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
    edge *edges_pointer;
} row_g;

int main(int argc, char *argv[]) {

    FILE *fp;
    unsigned int num_vertex;
    int i, j, c=0, k=0;
    row_g *rows;

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

    // Ciclo for per inserire gli archi

    for(j=0; j<num_vertex; j++) {
        fscanf(fp, "%i: ", &i);
        edge *head;
        head = malloc(sizeof(edge));
        do {
            i = -1;
            if(k !=0) ungetc(c, fp);
            fscanf(fp, "%i ", &i);
            if(i != -1) {
                if(k==0) {
                    create_list(head, i);
                } else {
                    push(head, i);
                }
            }
            c = fgetc(fp);
            k++;
        } while(c != 35);
        if(i==-1) {
            rows[j].edges_pointer = NULL;
            rows[j].edge_num = 0;
        } else {
            rows[j].edges_pointer = head;
            rows[j].edge_num = k;
        }
        k = 0;
    }

    //fclose(fp);

    // Stampa di prova

    for(i=0; i<num_vertex; i++) {
        printf("%i : ", i);
        print_list(rows[i].edges_pointer);
        printf("\n");
    }

    // Deallocazione di tutte le risorse

    for(i=0; i<num_vertex; i++) {
        free_list(rows[i].edges_pointer);
    }

    free(rows);

    return 0;
}