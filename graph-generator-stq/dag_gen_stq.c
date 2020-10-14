#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdlib.h>

#define DEBUG   0
#define L     100

typedef struct e_s {
  int e_n;
  int v_depth;
  int *e_p;
} n_t;

static void print_graph (n_t *, int, int, int, char *);
static void generate_query (int, int, char *);
static void seed_ran (void);
static int ran (int, int);
 
int main(int argc, char *argv[]) {
  char name[L];
  int i, j, jj, k, r, v_n, e_max, depth_max, e_tot, query_n, stop;
  n_t *row;

  if (argc != 5) {
    fprintf (stderr, "Run as: %s p1 p2 p3 p4\n", argv[0]);
    fprintf (stderr, "Where : p1 = number of vertex\n");
    fprintf (stderr, "        p2 = max number of edges\n");
    fprintf (stderr, "        p3 = number of queries\n" );
    fprintf (stderr, "        p4 = output file with no extension\n" );
    fprintf (stderr, "             generate graph file (.gra) and query file (.que)\n" );
    exit (1);
  }

  for (i=0; i<argc; i++) {
    printf ("%s ", argv[i]);
  }
  printf ("\n");

  seed_ran ();
  
  v_n = atoi(argv[1]);
  e_max = atoi(argv[2]);
  query_n = atoi(argv[3]);

  row = (n_t *) malloc (v_n * sizeof (n_t));
  if (row == NULL ) {
    printf ("Not enough room for this size graph\n" );
    return 0;
  }

  for (i=0; i<v_n; i++) {
    row[i].v_depth = -1;
  }
  
  for (i=0; i<v_n; i++) {
    if (row[i].v_depth == (-1)) {
      row[i].v_depth = 0;
    }
    
    r = ran (0, e_max+1);
    row[i].e_n = (r <= (v_n-i-1)) ? r : (v_n-i-1);
#if DEBUG
    if (i%1000 == 0)
      fprintf (stdout, "Vertex=%d n_edges=%d\n", i, row[i].e_n);
#endif
    row[i].e_p = (int *) calloc (row[i].e_n, sizeof (int));
    if (row[i].e_p == NULL) {
      printf( "Not enough room for this size graph\n" );
      return 0;
    }

    for (j=0; j<row[i].e_n; j++) {
#if DEBUG
      printf ("  edge=%d -> ", j);
#endif
      do {
        stop = 1;
        k = ran (i+1, v_n);
#if DEBUG
        printf ("%d ", k);
#endif
        for (jj=0; jj<j && stop==1; jj++)
	  if (k==row[i].e_p[jj])
	    stop = 0;
      } while (stop==0);
#if DEBUG
      printf ("\n");
#endif

      row[i].e_p[j] = k;
      if (row[k].v_depth < (row[i].v_depth+1)) {
        row[k].v_depth = row[i].v_depth+1;
      }
    }
  }

  e_tot = depth_max = (-1);
  for (i=0; i<v_n; i++) {
    if (row[i].v_depth > depth_max) {
      depth_max = row[i].v_depth;
    }
    e_tot = e_tot + row[i].e_n;
  }

  sprintf (name, "%s.gra", argv[4]);
  print_graph (row, v_n, e_tot, depth_max, name);

  for (i=0; i<v_n; i++) {
    free (row[i].e_p);
  }
  free (row);

  sprintf (name, "%s.que", argv[4]);
  generate_query (v_n, query_n, name);

  return (1);
}

static void generate_query (
  int v_n, int query_n, char *name
  )
{
   int i, v1, v2;
   FILE *fp;

   fp = fopen (name, "w");
   if (fp == NULL) {
      printf( "Unable to open file %s for writing.\n", name );
      return;
   }
   fprintf (stdout, "Writing (%d) queries to file %s\n", query_n, name);

   for (i=0; i<query_n; i++) {
     v1 = ran (0, v_n-1);
     v2 = ran (0, v_n-1);
     if (v2>v1) {
       fprintf (fp, "%d %d\n", v1, v2);
     } else {
       fprintf (fp, "%d %d\n", v2, v1);
     }
   }

   fclose (fp);

   return;
}

static void print_graph (
  n_t *row, int v_n, int e_tot, int depth_max, char *name
  )
{
   int i, j;
   FILE *fp;

   fp = fopen (name, "w");
   if (fp == NULL) {
      printf( "Unable to open file %s for writing.\n", name );
      return;
   }
   fprintf (stdout, "Writing graph to file %s\n", name);
   fprintf (stdout, "   |V| = %d\n", v_n);
   fprintf (stdout, "   |E| = %d\n", e_tot);
   fprintf (stdout, "   |Eavg| = %f\n", ((float) ((float) e_tot)/((float) v_n)));
   fprintf (stdout, "   Depth Max = %d\n", depth_max);

   fprintf( fp, "%d\n", v_n);

#if DEBUG
   fprintf (stdout, "   v(depth) = ");
#endif
   for (i=0; i<v_n; i++) {
#if DEBUG
     fprintf (stdout, "%d(%d) ", i, row[i].v_depth);
#endif
     fprintf (fp, "%d: ", i);
     for (j=0; j<row[i].e_n; j++) {
       fprintf (fp, "%d ", row[i].e_p[j]);
     }
     fprintf( fp, "#\n");
   }

#if DEBUG
   fprintf (stdout, "\n");
#endif
   fclose (fp);

   return;
}

static void seed_ran (void) {
   srand( ( unsigned short ) time( NULL ) );
}

/* Return a random integer between n1 and n2-1 inclusive. */
static int ran (int n1, int n2) {
   int k, r;
   k = n2 - n1;
   r = n1 + rand() % k;
   return r;
}
