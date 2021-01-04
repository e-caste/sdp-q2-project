#ifndef READ_GRAPH_H
    #define READ_GRAPH_H
    #include "utility.h"

    //FUNCTIONS FOR READING THE INPUT FILE1 (.GRA)
    void create_list(edge *head, int val);
    void push(edge *head, int val);
    void print_list(edge *head);
    void *scanFile(void *args);

    //function for build an array of roots    
    void *scanRoots(void *args);

#endif //READ_GRAPH_H