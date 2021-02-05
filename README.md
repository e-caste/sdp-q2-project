# Presentation Document 

Authors: Enrico Alberti, Enrico Castelli, Benedetto Giulivi

Date: 2021-01-30

Version: 1

## Contents

- [Presentation Document](#presentation-document)
- [Contents](#contents)
- [Abstract](#abstract)
- [Schema of the code](#code-schema)
- [Main data Structures](#main-data-structures)
	- [```graph => row_g[vertex_num]```](#graph--row_gvertex_num)
	- [```labels => row_l[vertex_num].field[labels_num]```](#labels--row_lvertex_numfieldlabels_num)
	- [```queries => queries[queries_num].vertex[source, dest]```](#queries--queriesqueries_numvertexsource-dest)
- [What we parallelize and how](#what-we-parallelize-and-how)
	- [DAG read](#dag-read)
	- [Label build](#label-build)
	- [Query resolution](#query-resolution)
- [Time and Memory usage](#time-and-memory-usage)
- [How to run](#how-to-run)

## Abstract
Our group developed the Q2 project which consists of implementing an algorithm named **GRAIL** used for Scalable Reachability Index for Large Graphs. The project could be divided in three main steps:
- **Reading** the Directed Acyclic graph (**DAG**)
- Generation of the **labels** required
- Test **query reachability** by means of labels


## Code schema

### TODO: specify which kind of protection where
### TODO: more specific schema, following GRAIL paper

The main thread (**main.c**) prepares the thread's data structure for **DAG** reading. In particular, it reads the first line of the input file DAG which contains the vertex number size used for allocate the right memory. The DAG files, is read (**readGraph.c**) from the maximum number of threads that the processor is able to support avoiding the scheduling of them. For instance if a processor is quad core and it's able to support 8 threads, eight is the number of threads that we create. The idea is to split the file into `NUM_THREAD` equal parts and let each thread read (without protection because there are no problems in reading) and fill its own part of the *shared* dag structure. Moreover, just after the building of the DAG structure, with some *synchronization*, each thread counts how many **roots** there are in its 'local' fragment and then all together (with some *protection*) fill a *shared* array containing all roots. These roots will be used later for label generation.

In the next step, the main thread re-initializes the threads data structure for **labels** generation (**buildLabels.c**). In general, we simply follow the paper structure. To be more specific, we run in parallel *one thread for each label* to generate. Each of these threads *visits* in random order the *roots*, and recursively visits -in random order- their *children* as described by the GRAIL algorithm. We tried to implement a mutex basic version in which each label-thread also generates one *thread for each child* and with the required protection, they explore the graph to generate the label. We removed this implementation due to the limit of the maximum number of threads we are able to create (`PTHREAD_THREADS_MAX`) which is easily reached in a very large graph.

Finally, in the last step the main thread re-initializes the threads data structure for the **query** reachability test. In particular, the main thread counts the number of queries to allocate the right memory. Then the query resolution begins (**solveQuery.c**). The idea is to split the number of queries into `NUM_THREADS` equal parts (so that all the cores work without scheduling) and let each thread solve its 'local' subset applying the logic described in the GRAIL paper algorithm (without the use of the exception list). At the end a log file is created to report the results. As a reminder, given two vertices (V1, V2), V1's thread checks if its labels is contained in V2's labels ([a, b] < [a, d]). If it is true V1 can not reach V2, otherwise we need to do a DFS with pruning such that for each children C of V1 that has v1's label contained C's labels execute a reachability test.

Overall in each of these main phases, the main thread evaluates the **time** and **memory usage** needed to execute the code.


## Main data Structures

### ```graph => row_g[vertex_num]```
```c
typedef struct row_graph {
	int edge_num;   //total number of vertex in this direction
	bool not_root;
	int *edges;
} row_g;
```
So we will have something like: 
- vertex 0 has children x, y
- vertex 1 has children h, k

| Structure  						| Description 				| 
|:----------------------------------|:--------------------------|
| row_g[0]   						|vertex number 0 			|
| row_g[1]   						|vertex number 1 			|
| row_g[0].edges_pointer.num 		|  children 'x' of vertex 0 |
| row_g[0].edges_pointer->next.num 	| children 'y' of vertex 0 	| 

---


### ```labels => row_l[vertex_num].field[labels_num]```
```c
typedef struct row_label {
	int* lbl_start;
	int* lbl_end;
	bool* visited;
} row_l;
```
So we will have something like:
- vertex 0 has 2 labels -> [x, y], [h, k]
- vertex 1 has 2 labels -> [v, w], [z, a]

| Structure  						| Description 				| Interval		|
|:----------------------------------|:--------------------------| :------------:|
| row_l[0]   						| labelS of vertex number 0 |				|
| row_l[0].lbl_start[0]				| begin label 0 of vertex 0	|	[x,			|
| row_l[0].lbl_end[0]				| end label 0 of vertex 0	|	y]			|
| row_l[0].lbl_start[1]				| begin label 1 of vertex 0	|	[h,			|
| row_l[0].lbl_end[1]				| end label 1 of vertex 0	|	  k]		|
| row_l[1].lbl_start[0]				| begin label 0 of vertex 1	|	[v,			|
| row_l[1].lbl_end[0]				| end label 0 of vertex 1	|	  w]		|
| row_l[1].lbl_start[1]				| begin label 1 of vertex 1	|	[z,			|
| row_l[1].lbl_end[1]				| end label 1 of vertex 1	|	  a]		|

---

### ```queries => queries[queries_num].vertex[source, dest]```
```c
typedef struct el_list_query {
	int num[2];
	bool can_reach;
} el_query;
```
So if we have 2 vertex (v1, v2) and two queries, we will have something like:
- el_query[0] = Query 1 for V1 -> V2 
- el_query[1] = Query 2 for V1 -> V2
- el_query[0].num[0] = V1 in Query 1
- el_query[0].num[1] = V2 in Query 1
- el_query[0].can_reach = true/false based of V1 -> V2

---

## What we parallelize and how

### DAG read
- MAX threads allowed by processors without scheduling
	- file split in function of its size
	- threads read in a range from inf to sup defined accurately (read -> no protection)
	- threads build shared array of roots -> mutex on shared array and shared index

### Label build
- 1 thread for each label		: current version
	- each thread runs its own label generation. Many labels at time
- MAX threads allowed by processors without scheduling for each root
	- divide the roots number by many threads. 1 label at time.
- 1 thread for each child	: limit of threads exceeded (20'000)
	- protection in children that enter the same node with mutex

### Query resolution
- MAX threads allowed by processors without scheduling
	- query file read with main thread because of its limited size
	- queries split in equal parts within the number of threads -> no protection because each thread write in its local part of the structure (array_queries[j].can_reach)
	- dfs search recursive and not parallel


## Time and Memory usage

### Time

|Test name 	| Sequential Version (ms) 	| Parallel Version (ms) |
|:----------|:--------------------------|:----------------------|
|		   	| 						 	|						|

### Memory Usage

|Test name 	| Sequential Version (ms) 	| Parallel Version (ms) |
|:----------|:--------------------------|:----------------------|
|		   	| 						 	|						|

## How to run

We have created a Bash wrapper (`run.sh`) around our C program that allows for a quick execution of all of its parts and provides some added functionalities.

```
Use -h or --help to show this help.
Usage:
./run.sh ([-d|--download] [-g|--generate] [-r|--run [-l|--labels <number of labels>] [benchmark|test|<path to graph file>]])
[] mean an argument is optional
() mean an argument is mandatory
| means the argument on its left is equivalent to the argument on its right
It is required to give at least 1 argument to let the script know what is expected of it.

There are 4 run modes available in this script:
- 'specific': if you give a path to a .gra file, it will run our program against it
- 'test': it will run our program against all the generated DAG files in data/gen-dags (it is required to run ./run.sh -g|--generate at least once in order to use this mode)
- 'benchmark': it will run our program against all the downloaded DAG files in data/grail-dags (it is required to run ./run.sh -d|--download at least once in order to use this mode)
- 'all': it will run our program against all the DAGs in data/gen-dags and data/grail-dags. It is equivalent to running both modes above.
The default run mode is 'all'.
The default number of labels is 5, but it can be specified with the -l|--labels <num> option.

You need the following packages to run this script:
wget gzip tar make gcc
```

So if you want to download all the needed DAG files that come with the GRAIL repository, you can simply use:  
`./run.sh --download`  
If you want to generate all the DAG files with prof. Quer's code, use:  
`./run.sh --generate`  
If you want to run our program against a single DAG file:  
`./run.sh --run --labels 2 data/gen-dags/v100e10.gra`  
If you want to run our program against all benchmark (GRAIL) DAG files:  
`./run.sh --run benchmark`.

See the help message above for all the other options.

Note: this script always recompiles the code if `make` detects a change.
