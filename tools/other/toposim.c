#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>

#define COBO_SUCCESS (0)

static int cobo_me     = -3;
static int cobo_nprocs = -1;


/* tree data structures */
static int  cobo_parent     = -3;    /* rank of parent */
static int* cobo_child      = NULL;  /* ranks of children */
static int  cobo_num_child  = 0;     /* number of children */
static int* cobo_child_incl = NULL;  /* number of children each child is responsible for (includes itself) */
static int  cobo_num_child_incl = 0; /* total number of children this node is responsible for */

int cobo_compute_children();
int cobo_compute_children_WF();

int main (int argc, char **argv)
{
  int i;

  cobo_nprocs=atoi(argv[1]);
  printf("nprocs=%d\n",cobo_nprocs);

  for(cobo_me=0;cobo_me<cobo_nprocs;cobo_me++) {
    /* cobo_compute_children_WF(); */
    cobo_compute_children();
    if(cobo_num_child>0) {
      printf("rank %2d: [%2d/%2d] parent=%2d --> ",cobo_me,cobo_num_child,cobo_num_child_incl,cobo_parent);
      for(i=0;i<cobo_num_child;i++) {
	printf("%2d ",cobo_child[i]);
      }
      printf("\n");
    }
  }
}



/* given cobo_me and cobo_nprocs, fills in parent and children ranks -- currently implements a binomial tree */
int cobo_compute_children()
{
    /* compute the maximum number of children this task may have */
    int n = 1;
    int max_children = 0;
    while (n < cobo_nprocs) {
        n <<= 1;
        max_children++;
    }

    /* prepare data structures to store our parent and children */
    cobo_parent = 0;
    cobo_num_child = 0;
    cobo_num_child_incl = 0;
    cobo_child      = (int*) malloc(max_children * sizeof(int));
    /* cobo_child_fd    = (int*) cobo_malloc(max_children * sizeof(int), "Child socket fd array"); */
    cobo_child_incl = (int*) malloc(max_children * sizeof(int));

    /* find our parent rank and the ranks of our children */
    int low  = 0;
    int high = cobo_nprocs - 1;
    while (high - low > 0) {
        int mid = (high - low) / 2 + (high - low) % 2 + low;
        if (low == cobo_me) {
            cobo_child[cobo_num_child] = mid;
            cobo_child_incl[cobo_num_child] = high - mid + 1;
            cobo_num_child++;
            cobo_num_child_incl += (high - mid + 1);
        }
        if (mid == cobo_me) { cobo_parent = low; }
        if (mid <= cobo_me) { low  = mid; }
        else                { high = mid-1; }
    }

    return COBO_SUCCESS;
}

/* given cobo_me and cobo_nprocs, fills in parent and children ranks -- currently implements a binomial tree */
int cobo_compute_children_WF()
{
    /* compute the maximum number of children this task may have */
    int n = 1;
    int max_children = 0;

    while (n < cobo_nprocs) {
        n <<= 1;
        max_children++;
    }

    /* prepare data structures to store our parent and children */
    cobo_parent = 0;
    cobo_num_child = 0;
    cobo_num_child_incl = 0;
    cobo_child      = (int*) malloc(max_children * sizeof(int));
    /* cobo_child_fd    = (int*) cobo_malloc(max_children * sizeof(int), "Child socket fd array"); */
    cobo_child_incl = (int*) malloc(max_children * sizeof(int));


    if(cobo_me==0) {
      cobo_child[cobo_num_child] = 1;
      cobo_num_child++;
      return COBO_SUCCESS;
    }

    /* find our parent rank and the ranks of our children */
    int low  = 1;
    int high = cobo_nprocs - 1;
    while (high - low > 0) {
        int mid = (high - low) / 2 + (high - low) % 2 + low;
        if (low == cobo_me) {
            cobo_child[cobo_num_child] = mid;
            cobo_child_incl[cobo_num_child] = high - mid + 1;
            cobo_num_child++;
            cobo_num_child_incl += (high - mid + 1);
        }
        if (mid == cobo_me) { cobo_parent = low; }
        if (mid <= cobo_me) { low  = mid; }
        else                { high = mid-1; }
    }

    return COBO_SUCCESS;
}
