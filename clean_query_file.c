#include <stdio.h>

int main (int argc, char *argv[]) {
    if(argc != 2) {
        fprintf(stderr, "Only one file argument expected, read %d instead.\n", argc);
        return 0;
    }
    FILE *fp;
    fp = fopen(argv[1], "r");
    if(fp == NULL) {
        fprintf(stderr, "Error opening file %s\n", argv[1]);
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