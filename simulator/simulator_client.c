#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 

#include <mpi.h>

#include "ldcs_api.h" 

#include "simulator_client.h" 


#define LONGTEST 1
#define MSG_SIZE 256


int simulator_client_read_requests ( MPI_Comm mycomm, 
				    char **request_data, int *request_data_len,
				     int *num_iter, int *num_requests, int **requests_len) {
  int rc=0;
  
  return(rc);
}



int simulator_client ( MPI_Comm mycomm,
		       char  *location, int locmod, int number ) {
  int rc=0;
  int ldcsid=-1;
  ldcs_message_t message;
  ldcs_message_t end_message;
  char buffer[256];
  char *result=NULL;
  int rank, size;
  MPI_Comm_rank(mycomm,&rank);
  MPI_Comm_size(mycomm,&size);

  MPI_Barrier(mycomm);

  ldcs_msg_init(&message);
  ldcs_msg_init(&end_message);

  if(locmod>0) {
    char buffer[MAX_PATH_LEN];
    debug_printf("multiple server per node add modifier to location mod=%d\n",locmod);
    if(strlen(location)+10<MAX_PATH_LEN) {
      sprintf(buffer,"%s-%02d",location,rank%locmod);
      debug_printf("change location to %s (locmod=%d)\n",buffer,locmod);
      debug_printf("open connection to ldcs %s %d\n",buffer,number);
      ldcsid=ldcs_open_connection(buffer,number);
    } else _error("location path too long");
  } else {
    debug_printf("open connection to ldcs %s %d\n",location,number);
    ldcsid=ldcs_open_connection(location,number);
  }
  if(ldcsid==-1) {
    _error("could not open connection");
  }

  ldcs_send_CWD(ldcsid);
  ldcs_send_HOSTNAME(ldcsid);
  ldcs_send_PID(ldcsid);

  /* query */

#ifdef LONGTEST
  {
    FILE * fp;
    char * line = NULL, *p;
    size_t len = 0;
    ssize_t read;
    
    fp = fopen("searchlist.dat", "r");   if (fp == NULL)  perror("could not open searchlist.dat");

    while( (read = getline(&line, &len, fp)) >= 0) {
      /* printf("CLIENT[%d]: found line: '%s'\n",rank, line); */

      if( (p=strchr(line,'\n')) ) *p='\0';
      if( (p=strchr(line,' ')) ) *p='\0';
      strcpy(buffer,line);

      if( buffer[0]=='#') continue;

      if ( strlen(buffer)==0 ) {
	printf("CLIENT[%d]:unknown line: '%s'\n",rank, buffer);
	continue;
      }

      if( (buffer[0]=='L') && (buffer[1]==':') ) {
	ldcs_send_FILE_QUERY(ldcsid,buffer+2,&result);
      } else if((buffer[0]=='E') && (buffer[1]==':')) {
	ldcs_send_FILE_QUERY_EXACT_PATH(ldcsid,buffer+2,&result);  
      } else { 
	if(strstr(buffer,".so")!=NULL) {
	  ldcs_send_FILE_QUERY(ldcsid,buffer,&result);  
	} else {
	  ldcs_send_FILE_QUERY_EXACT_PATH(ldcsid,buffer,&result);  
	}
      }
      printf("CLIENT[%d]:FILE_QUERY: '%s' -> '%s'\n",rank, buffer, result);
      if(result) {free(result);result=NULL;}
    }
    fclose(fp);
    if (line) free(line);
  }
#elif SHORTTEST
  strcpy(buffer,"/g/g92/frings1/LLNL/LDCS/auditclient/./lib/tls/x86_64/libsampleA.so");
  ldcs_send_FILE_QUERY(ldcsid,buffer,&result);  
  printf("FILE_QUERY: '%s' -> '%s'\n",buffer,result);
  free(result);
  strcpy(buffer,"/g/g92/frings1/LLNL/LDCS/auditclient/lib/libsampleA.so");
  ldcs_send_FILE_QUERY(ldcsid,buffer,&result);  
  printf("FILE_QUERY: '%s' -> '%s'\n",buffer,result);
  free(result);

#else
  /* give all clients a chance to connect to server, before server shutdown */
  sleep(5);
#endif

  end_message.header.type=LDCS_MSG_END;  end_message.header.len=0;end_message.data=NULL;
  ldcs_send_msg(ldcsid,&end_message);

  ldcs_close_connection(ldcsid);

  return(rc);
}
