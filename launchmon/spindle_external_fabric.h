#ifndef SAMPLE_EXTERNAL_FABRIC_H
#define SAMPLE_EXTERNAL_FABRIC_H

#ifdef __cplusplus
extern "C" {
#endif

#define HOSTNAME_LEN 100
struct spindle_external_fabric_data_struct
{
  int md_rank;
  int md_size;
  int asession;
};
typedef struct spindle_external_fabric_data_struct spindle_external_fabric_data_t;


/* function which will be called on spindle_server and spindle_fe */
/* spindle_server: called after listening socket is opened
                   input:  hostname, socket port of listening port 
                   output: myrank and size (location in MD world) 
		           rank 0: hostlist and portlist (COBO: port==-1 --> use default)
*/

/* spindle_fe:     called at beginning of fe_md_open
                   input:  -
                   output: myrank=-1 and size (location in MD world) 
                           hostlist and portlist (COBO: port==-1 --> use default)
                           */
int spindle_external_fabric_fe_CB ( char *myhostname, int myport, int *myrank, int *size, char ***hostlist, int **portlist, 
				    void *data );
int spindle_external_fabric_be_CB ( char *myhostname, int myport, int *myrank, int *size, char ***hostlist, int **portlist, 
				    void *data );

#ifdef __cplusplus
}
#endif

#endif
