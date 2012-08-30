/* Wolfgang, */
/* While it's on my mind, here is the code to implement a bcast where any */
/* rank can be the root.  I pulled this from my light-weight group library, */
/* but it'd be easy to convert this to sockets.  This implements a binomial */
/* tree.  The ranks are arranged in a ring, and each rank records the list */
/* of log(N) ranks to its left side, and the list of log(N) ranks to its */
/* right side. */


/* implements a binomail tree */
int lwgrp_logring_bcast(
			void* buffer,
			int count,
			MPI_Datatype datatype,
			int root,
			const lwgrp_ring* group,
			const lwgrp_logring* list)
{
  /* get ring info */
  MPI_Comm comm  = group->comm;
  int rank       = group->group_rank;
  int ranks      = group->group_size;

  /* adjust our rank by setting the root to be rank 0 */
  int treerank = rank - root;
  if (treerank < 0) {
    treerank += ranks;
  }

  /* get largest power-of-two strictly less than ranks */
  int pow2, log2;
  lwgrp_largest_pow2_log2_lessthan(ranks, &pow2, &log2);

  /* run through binomial tree */
  int parent = 0;
  int received = (rank == root) ? 1 : 0;
  while (pow2 > 0) {
    /* check whether we need to receive or send data */
    if (! received) {
      /* if we haven't received a message yet, see if the parent for
       * this step will send to us */
      int target = parent + pow2;
      if (treerank == target) {
        /* we're the target, get the real rank and receive data */
        MPI_Status status;
        int src = list->left_list[log2];
        MPI_Recv(buffer, count, datatype, src, LWGRP_MSG_TAG_0, comm,
		 &status);
        received = 1;
      } else if (treerank > target) {
        /* if we are in the top half of the subtree set our new
         * potential parent */
        parent = target;
      }
    } else {
      /* we have received the data, so if we have a child, send data */
      if (treerank + pow2 < ranks) {
        int dst = list->right_list[log2];
        MPI_Send(buffer, count, datatype, dst, LWGRP_MSG_TAG_0, comm);
      }
    }

    /* cut the step size in half and keep going */
    log2--;
    pow2 >>= 1;
  }

  return 0;
}

/* It shouldn't be too difficult to code this into your active message */
/* framework.  Basically, I think you could listen on all 2 * log(N) */
/* sockets.  Each packet would be labeled with the root of the tree it */
/* belongs to and a flag specifying whether it's headed up or down the */
/* tree.  When each message comes in, you read this header to identify the */
/* tree and its direction, then... */

/* If it's headed up, you compute the offset of the parent socket, and send */
/* it, e.g., */

/* adjust our rank by setting the root to be rank 0 */
int treerank = rank - root;
if (treerank < 0) {
  treerank += ranks;
 }

  /* get largest power-of-two strictly less than ranks */
  int pow2, log2;
  lwgrp_largest_pow2_log2_lessthan(ranks, &pow2, &log2);

  if (treerank > 0) {
    int parent = 0;
    while (pow2 > 0) {
      /* see if the parent for this step is our parent */
      int target = parent + pow2;
      if (treerank == target) {
        /* we found our parent, send and break */
        int dest = list->left_list[log2];
        MPI_Send(buffer, count, datatype, dest, LWGRP_MSG_TAG_0, comm);
        break;
      } else if (treerank > target) {
        /* if we are in the top half of the subtree, set our new
potential parent */
        parent = target;
      }

      /* cut the step size in half and keep going */
      log2--;
      pow2 >>= 1;
    }
  } else {
    /* message has reached the root */
  }

/* If it's headed down, you compute the children, and forward the message */
/* out the corresponding sockets, e.g., */

  /* adjust our rank by setting the root to be rank 0 */
  int treerank = rank - root;
  if (treerank < 0) {
    treerank += ranks;
  }

  /* get largest power-of-two strictly less than ranks */
  int pow2, log2;
  lwgrp_largest_pow2_log2_lessthan(ranks, &pow2, &log2);

  /* run through binomial tree */
  int parent = 0;
  int received = (treerank == 0);
  while (pow2 > 0) {
    /* check whether we need to receive or send data */
    if (! received) {
      /* determine which step we'll receive data from our parent */
      int target = parent + pow2;
      if (treerank == target) {
        /* we've identified our parent, now start sending */
        received = 1;
      } else if (treerank > target) {
        /* if we are in the top half of the subtree, set our new
potential parent */
        parent = target;
      }
    } else {
      /* we have received the data, so if we have a child, send data */
      if (treerank + pow2 < ranks) {
        int dst = list->right_list[log2];
        MPI_Send(buffer, count, datatype, dst, LWGRP_MSG_TAG_0, comm);
      }
    }

    /* cut the step size in half and keep going */
    log2--;
    pow2 >>= 1;
  }

/* find largest power of two that is less than or equal to ranks */
int lwgrp_largest_pow2_log2_lte(int ranks, int* outpow2, int* outlog2)
{
  int pow2 = 1;
  int log2 = 0;
  while (pow2 <= ranks) {
    pow2 <<= 1;
    log2++;
  }
  pow2 >>= 1;
  log2--;

  *outpow2 = pow2;
  *outlog2 = log2;

  return LWGRP_SUCCESS;
}

/* find largest power of two strictly less than ranks */
int lwgrp_largest_pow2_log2_lessthan(int ranks, int* outpow2, int* outlog2)
{
  int pow2, log2;
  lwgrp_largest_pow2_log2_lte(ranks, &pow2, &log2);
  if (pow2 == ranks) {
    pow2 >>= 1;
    log2--;
  }

  *outpow2 = pow2;
  *outlog2 = log2;

  return LWGRP_SUCCESS;
}


/* In your case, the left and right lists would have connected sockets */
/* instead of MPI ranks, and of course you'd replace the MPI calls with */
/* socket reads/writes.  It might also be worthwhile to implement a binary */
/* tree instead of a binomial tree, since binary trees do a better job of */
/* pipelining data. */


