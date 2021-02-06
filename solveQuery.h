#ifndef SOLVE_QUERY_H
    #define SOLVE_QUERY_H
    #include "utility.h"

    bool dfs_search(row_g *graph, unsigned long node1, unsigned long node2, bool *visited);
    void *solveQuery (void *args);
#endif  //SOLVE_QUERY_H