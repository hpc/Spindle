#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "ldcs_cobo.h"

int ranks, my_rank;
char** procs;

size_t size = 1024;
size_t buffer_size;
char* sbuffer;
char* rbuffer;

/* fill the buffer with a pattern */
void init_sbuffer(int rank)
{
  size_t i;
  char value;
  for(i = 0; i < buffer_size; i++)
  {
    value = (char) ((i+1)*(rank+1) + i);
    sbuffer[i] = value;
  }
}

/* blank out the receive buffer */
void init_rbuffer(int rank)
{
  memset(rbuffer, 0, buffer_size);
}

/* check the send buffer for any deviation from expected pattern */
void check_sbuffer(char* op)
{
  size_t i;
  char value;
  for(i = 0; i < buffer_size; i++)
  {
    value = (char) ((i+1)*(my_rank+1) + i);
    if (sbuffer[i] != value)
    {
      printf("%d: %s: Send buffer corruption detected at sbuffer[%d]\n",
             my_rank, op, i);
    }
  }
}

/* check the receive buffer for any deviation from expected pattern */
void check_rbuffer(char* buffer, size_t byte_offset, int rank, size_t src_byte_offset, size_t element_count, char* op)
{
  size_t i, j;
  char value;
  buffer += byte_offset;
  for(i = 0, j = src_byte_offset; i < element_count; i++, j++)
  {
    value = (char) ((j+1)*(rank+1) + j);
    if (buffer[i] != value)
    {
      printf("%d: %s: Receive buffer corruption detected at rbuffer[%d] from rank %d\n",
             my_rank, op, byte_offset+i, rank);
    }
  }
}

int main(int argc, char* argv[])
{
  int root = 0;
  int i;

  int num_ports = atoi(argv[1]);

  int* portlist = malloc(num_ports * sizeof(int));
  for (i=0; i<num_ports; i++) {
    portlist[i] = 5000 + i;
  }

/*
  int num_ports = 100;
  int portlist[100] = {
    5000,5100,5200,5300,5400,5500,5600,5700,5800,5900,
    6000,6100,6200,6300,6400,6500,6600,6700,6800,6900,
    7000,7100,7200,7300,7400,7500,7600,7700,7800,7900,
    8000,8100,8200,8300,8400,8500,8600,8700,8800,8900,
    9000,9100,9200,9300,9400,9500,9600,9700,9800,9900,
    5060,5160,5260,5360,5460,5560,5660,5760,5860,5960,
    6060,6160,6260,6360,6460,6560,6660,6760,6860,6960,
    7060,7160,7260,7360,7460,7560,7660,7760,7860,7960,
    8060,8160,8260,8360,8460,8560,8660,8760,8860,8960,
    9060,9160,9260,9360,9460,9560,9660,9760,9860,9960
  };
*/

  /* initialize the client (read environment variables) */
  if (cobo_open(2384932, portlist, num_ports, &my_rank, &ranks) != COBO_SUCCESS) {
    printf("Failed to init\n");
    exit(1);
  }
  printf("Ranks: %d, Rank: %d\n", ranks, my_rank);  fflush(stdout);

#if 1

  buffer_size = ranks * size;
  sbuffer = malloc(buffer_size);
  rbuffer = malloc(buffer_size);

  /* test cobo_barrier */
  if (my_rank == root) { printf("Barrier\n");  fflush(stdout); }
  if (cobo_barrier() != COBO_SUCCESS) {
    printf("Barrier failed\n");
    exit(1);
  }

  /* test cobo_bcast */
  init_sbuffer(my_rank);
  init_rbuffer(my_rank);
  void* buf = (void*) rbuffer;
  if (my_rank == root) { buf = sbuffer; }
  if (my_rank == root) { printf("Bcast\n");  fflush(stdout); }
  if (cobo_bcast(buf, (int) size, root) != COBO_SUCCESS) {
    printf("Bcast failed\n");
    exit(1);
  }
/*  check_sbuffer(); */
  check_rbuffer(buf, 0, root, 0, size, "cobo_bcast");

  /* test cobo_scatter */
  init_sbuffer(my_rank);
  init_rbuffer(my_rank);
  if (my_rank == root) { printf("Scatter\n");  fflush(stdout); }
  if (cobo_scatter(sbuffer, (int) size, rbuffer, root) != COBO_SUCCESS) {
    printf("Scatter failed\n");
    exit(1);
  }
  check_sbuffer("cobo_scatter");
  check_rbuffer(rbuffer, 0, root, my_rank*size, size, "cobo_scatter");

  /* test cobo_gather */
  init_sbuffer(my_rank);
  init_rbuffer(my_rank);
  if (my_rank == root) { printf("Gather\n");  fflush(stdout); }
  if (cobo_gather(sbuffer, (int) size, rbuffer, root) != COBO_SUCCESS) {
    printf("Gather failed\n");
    exit(1);
  }
  check_sbuffer("cobo_gather");
  if (my_rank == root) {
    for (i = 0; i < ranks; i++) {
      check_rbuffer(rbuffer, i*size, i, 0, size, "cobo_gather");
    }
  }

  /* test cobo_allgather */
  init_sbuffer(my_rank);
  init_rbuffer(my_rank);
  if (my_rank == root) { printf("Allgather\n");  fflush(stdout); }
  if (cobo_allgather(sbuffer, (int) size, rbuffer) != COBO_SUCCESS) {
    printf("Allgather failed\n");
    exit(1);
  }
  check_sbuffer("cobo_allgather");
  for (i = 0; i < ranks; i++) {
    check_rbuffer(rbuffer, i*size, i, 0, size, "cobo_allgather");
  }
#endif

#if 0
  /* test cobo_alltoall */

  init_sbuffer(my_rank);
  init_rbuffer(my_rank);
  if (my_rank == root) { printf("Alltoall\n");  fflush(stdout); }
  if (cobo_alltoall(sbuffer, (int) size, rbuffer) != COBO_SUCCESS) {
    printf("Alltoall failed\n");
    exit(1);
  }
  check_sbuffer("cobo_alltoall");
  for (i = 0; i < ranks; i++) {
    check_rbuffer(rbuffer, i*size, i, my_rank*size, size, "cobo_alltoall");
  }


/*
  int max;
  if (cobo_allreducemaxint(&my_rank, &max) != COBO_SUCCESS) {
    printf("Allreducemaxint failed\n");
    exit(1);
  }
  printf("%d: Max int %d\n", my_rank, max);

  char** hosts;
  char*  hostsbuf;
  char   host[255];
  gethostname(host, 255);
  if (cobo_allgatherstr(host, &hosts, &hostsbuf) != COBO_SUCCESS) {
    printf("Allgatherstr failed\n");
    exit(1);
  }
  int i;
  if (my_rank == 0 || my_rank == ranks-1) {
    for (i=0; i<ranks; i++) {
      printf("%d: hosts[%d] = %s\n", my_rank, i, hosts[i]);
    }
  }
  free(hosts);
  free(hostsbuf);
*/

#endif

/* done: */
  /* close connections (close connection with launcher and tear down the TCP tree) */
  if (cobo_close() != COBO_SUCCESS) {
    printf("Failed to close\n");
    exit(1);
  }

  /* needed this sleep so that server prints out all debug info (don't know why yet) */
  sleep(1);

  return 0;
}
