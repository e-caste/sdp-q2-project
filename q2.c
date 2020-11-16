#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>

#define N 4

// TODO: https://en.wikipedia.org/wiki/C_data_types
//      Ottimizzazione delle memoria: sostituire int con short se possibile
//      Quanti nodi al massimo? unsigned int: [0, 65,535] ; unsigned long int: [0, 4,294,967,295]

typedef struct edge_list {
    int num;            //valore del vertice
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
    int edge_num;       //numero totali di vertici in quella direzione
    edge *edges_pointer;
} row_g;

// 3.1 multiple index construction
typedef struct row_label {
    int* lbl_start;
    int* lbl_end;
    bool* visited;
} row_l;

typedef struct thread_args {
    int id;
    int total_vertex;
    int size_file;
    char *filename;
    row_g *graph;
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

    //ogni thread leggerà da inf a sup. 
    //Inf e Sup sono il numero di riga del file di input 0, 1, ..., 10,...
    //Nel seguente codice, si sta per dividere il file da leggere
    //in N parti, così che ogni thread legga in parallelo

    //Definizione estremo superiore
    if (my_data->id == N-1) 
        sup = my_data->total_vertex;
    else {
        //con fseek mi metto in un punto casuale all'interno della riga
        //successivamente controllo finchè non trovo '\n' (10)
        //prendendo un carattere alla volta e "scartandolo"
        //quando trovo la nuova riga come primo intero avrò il SUP
        //NOTA: questa un'ottima strategia in quanto abbiamo righe non omogenee
        fseek(fp, my_data->size_file*(my_data->id+1)/N, SEEK_SET); // 1Gb / 4Thread = 250Mb ciascuno * id+1
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
        fseek(fp, my_data->size_file*my_data->id/N, SEEK_SET); // 1Gb / 4Thread = 250Mb ciascuno * id

        while(i != 10) {    
            i = getc(fp);
        }

        pos = ftell(fp);            //conservo la posizione di inizio riga

        fscanf(fp, "%i", &inf);     // leggo l'indice della riga

        fseek(fp, pos, SEEK_SET);   //torno all'inizio della riga
    }    

    //printf("Thread n. %i con inf: %i e sup: %i\n", my_data->id, inf, sup-1);
    
    //adesso per le righe di competenza del thread, si legge il valore del nodo
    //si costruisce e inizializza la struttura apposita e si inserisce in lista
    for(j=inf; j<sup; j++) {
        fscanf(fp, "%i: ", &i);     //scarto il numero della riga (== j) e anche ": "
        edge *head;
        head = malloc(sizeof(edge));
        do {
            i = -1;
            if(k !=0) //alla prima iterazione k==0 
                ungetc(c, fp);    //Poichè prima ho letto un carattere che non era "#" (altrimenti usciva dal ciclo) devo tornare indietro!!
            int res = fscanf(fp, "%i ", &i);      // leggo sia il valore che lo spazio. Se ci fosse un "#" non legge nulla, perchè si aspetta un %i
            if(i != -1) { // i vertici non possono essre negativi, meglio testare "res"
                if(k==0) {
                    create_list(head, i);
                } else {
                    push(head, i);
                }
            }
            c = fgetc(fp);              //prendo carattere successivo
            k++;
        } while(c != 35);               // se carattere == "#" (fine riga)
        
        if(i==-1) { //non prendo nessun intero -> lista archi vuota
            my_data -> graph[j].edges_pointer = NULL;
            my_data -> graph[j].edge_num = 0;
        } else {
            my_data -> graph[j].edges_pointer = head;
            my_data -> graph[j].edge_num = k;
        }
        k = 0;  //vertice successivo
    }

    pthread_exit((void *) 0);

}

void RandomizedVisit(int node_num, int lbl_num, row_l* labels, row_g* graph, int* rank_root, int num_vertex){
    int rank_children_min = num_vertex;

    //printf("RandomVisited-begin: node: %i\n", node_num);

    if(labels[node_num].visited[lbl_num])
        return;

    //Per tutti i figli del nodo, richiamo la funzione
    //TODO Random Order
    for(edge* next=graph[node_num].edges_pointer; next != NULL; next = next->next_num)
        RandomizedVisit(next->num, lbl_num, labels, graph, rank_root, num_vertex);
    labels[node_num].visited[lbl_num] = true;
    
    //cerco minimo tra i figli del nodo
    for(edge* next=graph[node_num].edges_pointer; next != NULL; next = next->next_num)
        if(labels[next->num].lbl_start[lbl_num] < rank_children_min)
            rank_children_min = labels[next->num].lbl_start[lbl_num];
        
    if(*rank_root < rank_children_min)
        labels[node_num].lbl_start[lbl_num] = *rank_root;
    else 
        labels[node_num].lbl_start[lbl_num] =  rank_children_min;
    
    labels[node_num].lbl_end[lbl_num] = *rank_root;

    *rank_root = *rank_root + 1 ;

    //printf("RandomVisited-end: node: %i, lbl: %i, [%i, %i]\n", node_num, lbl_num, labels[node_num].lbl_start[lbl_num], labels[node_num].lbl_end[lbl_num]);
}

void RandomizedLabeling(row_g * graph, row_l * labels, int num_label, int num_vertex){
    int i, rank_node, j ;
    for(i=0; i<num_label; i++){
        rank_node = 1;
        //devo cercare le radici.
        //E' considerato radice ogni nodo con almeno un figlio.
        for(j=0; j<num_vertex; j++){    //TODO Random Order
            if(graph[j].edge_num >= 1)
                RandomizedVisit(j, i, labels, graph, &rank_node, num_vertex);
        }
    }
}

// args[1]: file1 (input .gra)
// args[2]: n (label number)
// args[3]: file2 (.que)

int main(int argc, char *argv[]) {

    FILE *fp;
    unsigned int num_vertex;
    int i, j, size, c=0, k=0, err_code=0, d;
    row_g *rows;
    row_l *labels;
    pthread_t threads[N];
    t_args args[N];

    // Controllo sugli argomenti

    if (argc != 3) {
        fprintf(stderr, "For the moment, enter only 'file1' and 'n' as arguments!\n");
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

    rows = (row_g *) malloc (num_vertex * sizeof (row_g));  //array di liste
    if (rows == NULL ) {
        printf ("Not enough room for this size graph\n" );
        return 0;
    }

    // Prendere grandezza file per dividerlo per fseek

    fseek(fp, 0L, SEEK_END);
    size = ftell(fp);

    fclose(fp);

    // Creazione thread
    for(j=0; j<N; j++) {
        args[j].filename = argv[1];
        args[j].graph = rows;
        args[j].total_vertex = num_vertex;  //facoltativo?
        args[j].size_file = size;
    }

    for(i=0; i<N; i++) {    //eventualmente si può unire il for.
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

    // Stampa di prova

    for(i=0; i<num_vertex; i++) {
        printf("%i : ", i);
        print_list(rows[i].edges_pointer);
        //printf(" -----> num_edge: %i\n", rows[i].edge_num);
        printf("\n");
    }

    // Allocazione struct per labels
    // labels procederrano di pari ordine con l'indice rows (Indice del nodo).

    d = atoi(argv[2]);
    labels = (row_l *) malloc (num_vertex * sizeof (row_l));
    if (labels == NULL ) {
        printf ("Not enough room for this size labels\n" );
        return -1;
    }

    for(i=0; i<num_vertex; i++){
        labels[i].lbl_start = (int *) malloc (d * sizeof (int));
        labels[i].lbl_end = (int *) malloc (d * sizeof (int));
        labels[i].visited = (bool *) malloc (d * sizeof (bool));
        if ((labels[i].lbl_start == NULL ) || (labels[i].lbl_end == NULL ) || (labels[i].visited == NULL )) {
            printf ("Not enough room for this size labels\n" );
            return -1;
        }

        //Inizializzazione (Non è detto che tutto sia azzerato!)
        for(j=0; j<d; j++){
            labels[i].lbl_start[j] = 0;
            labels[i].lbl_end[j] = 0;
            labels[i].visited[j] = false;
        }

    }

    RandomizedLabeling(rows, labels, d, num_vertex);

    //Stampa delle labels
    for(i=0; i<num_vertex; i++){
        printf("Node: %i ", i);
        for(j=0; j<d; j++){
            printf("[%i, %i] ", labels[i].lbl_start[j], labels[i].lbl_end[j]);
        }
        printf("\n");
    }

    // Deallocazione di tutte le risorse

    for(i=0; i<num_vertex; i++) {
        free_list(rows[i].edges_pointer);
    }

    for(i=0; i<num_vertex; i++){
        free(labels[i].lbl_start);
        free(labels[i].lbl_end);
        free(labels[i].visited);
    }

    free(labels);

    free(rows);

    return 0;
}