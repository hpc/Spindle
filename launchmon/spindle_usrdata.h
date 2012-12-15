#ifndef SPINDLE_USRDATA_H
#define SPINDLE_USRDATA_H

#ifdef __cplusplus
extern "C" {
#endif

#define HOSTNAME_LEN 100

struct ldcs_host_port_list_struct
{
  int size;
  char* hostlist;
  int*  portlist;
};
typedef struct ldcs_host_port_list_struct ldcs_host_port_list_t;

int packbefe_cb ( void* udata, 
		  void* msgbuf, 
		  int msgbufmax, 
		  int* msgbuflen );

int unpackfebe_cb  ( void* udatabuf, 
		     int udatabuflen, 
		     void* udata );

int packfebe_cb ( void *udata, 
		  void *msgbuf, 
		  int msgbufmax, 
		  int *msgbuflen );

int unpackbefe_cb  ( void* udatabuf, 
		     int udatabuflen, 
		     void* udata );

#ifdef __cplusplus
}
#endif

#endif
