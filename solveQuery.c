#include "solveQuery.h"

bool dfs_search(row_g *graph, int node1, int node2, bool *visited) {
    if(node1 == node2)
        return true;
    if(visited[node1])
        return false;

    int children_num = graph[node1].edge_num;
    bool reachable = false;
    
    if(children_num > 0) {

        int children[children_num];
        int i=0;

        for(edge* next=graph[node1].edges_pointer; next != NULL; next = next->next_num){
            if(next -> num == node2) {
                return true;
            }
            children[i] = next->num;
            i++;
        }

        for(i=0; i<children_num; i++) {
            reachable = dfs_search(graph, children[i], node2, visited);
            if(reachable) {
                return true;
            }
        }

    }

    visited[node1] = true;

    return reachable;
}

void *solveQuery (void *args) {
    t_args *my_data;
    my_data = (t_args *) args;
    unsigned int num_threads = my_data->total_threads;

    int i, j, inf, sup, node1, node2;
    bool dfs;

    if (my_data->id == num_threads-1)
        sup = my_data->queries_num;
    else
        sup = ((my_data->queries_num) / num_threads) * (my_data->id + 1);      //TODO e' necessario cast(int)?

    if (my_data->id == 0)
        inf = 0;
    else
        inf = ((my_data->queries_num) / num_threads) * (my_data->id);

    //printf("Thread n. %i con inf: %i e sup: %i\n", my_data->id, inf, sup-1);

    for(j=inf; j<sup; j++) {
        dfs = true;
        node1 = my_data -> array_queries[j].num[0];
        node2 = my_data -> array_queries[j].num[1];

        for(i=0; i<my_data->num_labels; i++) {
            if (my_data->array_labels[node1].lbl_start[i]>my_data->array_labels[node2].lbl_start[i] ||
                my_data->array_labels[node1].lbl_end[i]<my_data->array_labels[node2].lbl_end[i]) { // line 1-2 alg. 2 paper
                my_data -> array_queries[j].can_reach = false;
                //fprintf(fp_res_query, "%i %i 0\n", node1, node2);
                dfs = false;
                break;
            }
        }

        if(dfs) {
            memset(my_data->node_visited, false, my_data->total_vertex * sizeof(bool));
            my_data -> array_queries[j].can_reach = dfs_search(my_data->graph, node1, node2, my_data->node_visited);

            //fprintf(fp_res_query, "%i %i %d\n", node1, node2, reachable ? 1 : 0);
        }
    }

    pthread_exit((void *) 0);
}
