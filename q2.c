#include <stdio.h>
#include <stdlib.h>

#define L     100

typedef struct row_graph {
    int edge_num;
    int *edges_pointer;
} row_g;

int main(int argc, char *argv[]) {

    FILE *fp;
    char name[L];
    unsigned int num_vertex;
    int i, j, c=0, k=0;
    char buffer[100];
    row_g *rows;

    // Controllo sugli argomenti

    if (argc != 2) {
        fprintf(stderr, "For the moment, enter only one argument!\n");
        exit(1);
    }

    // Apertura file1

    fp = fopen(argv[1], "r");

    if (fp == NULL) {
        fprintf(stderr, "Unable to open file %s for reading.\n", name );
        exit(1);
    }

    // Prendere numero vertici per allocare la giusta memoria

    fscanf(fp,"%i\n", &num_vertex);

    rows = (row_g *) malloc (num_vertex * sizeof (row_g));
    if (rows == NULL ) {
        printf ("Not enough room for this size graph\n" );
        return 0;
    }

    // Ciclo per contare gli archi per allocare la giusta memoria 

    for(j=0; j<num_vertex; j++) {
        
        fscanf(fp, "%i: ", &i);
        
        do {
            if(k !=0) ungetc(c, fp);
            fscanf(fp, "%i ", &i);
            c = fgetc(fp);
            k++;
        } while(c != 35);
        
        rows[j].edge_num = k;
        rows[j].edges_pointer = (int *) calloc (rows[j].edge_num, sizeof (int));
        
        if (rows[j].edges_pointer == NULL ) {
            printf ("Not enough room for the edges of the vertex %i.\n", j );
            return 0;
        }
        
        k = 0;
    }

    // Chiusura e apertura del file per poter rileggerlo dall'inizio

    fclose(fp);
    fp = fopen(argv[1], "r");

    if (fp == NULL) {
        fprintf(stderr, "Unable to open file %s for reading.\n", name );
        exit(1);
    }

    // Per saltare la prima riga

    fscanf(fp,"%i\n", &num_vertex);

    // Ciclo for per inserire gli archi

    for(j=0; j<num_vertex; j++) {
        fscanf(fp, "%i: ", &i);
        do {
            if(k !=0) ungetc(c, fp);
            fscanf(fp, "%i ", &i);
            rows[j].edges_pointer[k] = i;
            c = fgetc(fp);
            k++;
        } while(c != 35);
        k = 0;
    }

    fclose(fp);

    // Stampa di prova

    for(i=0; i<num_vertex; i++) {
        printf("%i : ", i);
        for(j=0; j<rows[i].edge_num; j++) {
            printf("%i ", rows[i].edges_pointer[j]);
        }
        printf("\n");
    }

    // Deallocazione di tutte le risorse

    for(i=0; i<num_vertex; i++) {
        free(rows[i].edges_pointer);
    }

    free(rows);

    return 0;
}
