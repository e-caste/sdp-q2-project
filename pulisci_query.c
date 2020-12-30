#include <stdio.h>
#include <stdlib.h>

int main (int argc, char *argv[]) {
    if(argc != 2) {
        printf("Inserire soltanto un argomento!!!\n");
        return 0;
    }
    FILE *fp;
    fp = fopen(argv[1], "r");
    if(fp == NULL) {
        printf("Errore nell'aprire il file!!!\n");
        return 0;
    }

    int num_query = 0;

    while (fscanf(fp, "%*[^\n]\n") != -1) {
        num_query++;
    }

    int queries[num_query][2];
    int i;
    int a, b, c;

    fseek(fp, 0L, SEEK_SET);

    for (i=0; i<num_query; i++) {
        fscanf(fp, "%i %i %i\n", &a, &b, &c);
        queries[i][0] = a;
        queries[i][1] = b;
    }

    fclose(fp);

    fopen(argv[1], "w+");

    for(i=0; i<num_query; i++) {
        fprintf(fp, "%i %i\n", queries[i][0], queries[i][1]);
    }

    fclose(fp);

    return 0;

}