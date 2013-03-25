#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/inotify.h>
#include <errno.h>

#include "ldcs_api.h"

/* client description structure */
typedef enum {
   LDCS_LISTEN_STATUS_ACTIVE,
   LDCS_LISTEN_STATUS_FREE,
} ldcs_listen_data_item_status_t;


struct ldcs_listen_data_item_struct
{
   int                            fd;
   int                            id;
   int                            (*cb_func) ( int fd, int id, void *data );
   void*                          data;
   ldcs_listen_data_item_status_t state;
};
typedef struct ldcs_listen_data_item_struct ldcs_listen_data_item_t;

struct ldcs_listen_data_struct
{
   int state;
   int item_table_size;
   int item_table_used;
   ldcs_listen_data_item_t* item_table;
   int signal_end;
};

typedef struct ldcs_listen_data_struct ldcs_listen_data_t;

static ldcs_listen_data_t ldcs_listen_data = {0, 0, 0, NULL, 0};

static int (*loop_exit_cb) ( int num_fds, void *data ) = NULL;
static void *loop_exit_cb_data = NULL;

static int do_exit = 0;

int ldcs_listen_register_exit_loop_cb( int cb_func ( int num_fds, void *data ), 
                                       void * data) {
   int rc=0;
   loop_exit_cb=cb_func;
   loop_exit_cb_data=data;
   return(rc);
}

int ldcs_listen_register_fd( int fd, 
                             int id, 
                             int cb_func ( int fd, int id, void *data ), 
                             void * data) {
   int rc=0;
   int c;

   /* icrease size of list if needed */
   if (ldcs_listen_data.item_table_used >= ldcs_listen_data.item_table_size) {
      ldcs_listen_data.item_table = realloc(ldcs_listen_data.item_table, 
                                            (ldcs_listen_data.item_table_used + 16) * sizeof(ldcs_listen_data_item_t)
         );
      ldcs_listen_data.item_table_size = ldcs_listen_data.item_table_used + 16;
      for(c=ldcs_listen_data.item_table_used;(c<ldcs_listen_data.item_table_used + 16);c++) {
         ldcs_listen_data.item_table[c].state=LDCS_LISTEN_STATUS_FREE;
      }
   }
   for(c=0;(c<ldcs_listen_data.item_table_size);c++) {
      if (ldcs_listen_data.item_table[c].state==LDCS_LISTEN_STATUS_FREE) break;
   }
   if(c==ldcs_listen_data.item_table_size) _error("internal error with item table (table full)");

   /* store information of new item */
   ldcs_listen_data.item_table_used++;
   ldcs_listen_data.item_table[c].state = LDCS_LISTEN_STATUS_ACTIVE;
   ldcs_listen_data.item_table[c].fd    = fd;
   ldcs_listen_data.item_table[c].id    = id;
   ldcs_listen_data.item_table[c].data  = data;
   ldcs_listen_data.item_table[c].cb_func = cb_func;

   debug_printf3("registered fd %d id=%d  c=%d\n",fd,id,c);

   return(rc);
}

int ldcs_listen_unregister_fd( int fd ) {
   int rc=0;
   int c;
   debug_printf3("unregister fd %d ..\n",fd);
   for(c=0;(c<ldcs_listen_data.item_table_size);c++) {
      if ( ( ldcs_listen_data.item_table[c].state == LDCS_LISTEN_STATUS_ACTIVE ) &&
           ( ldcs_listen_data.item_table[c].fd==fd) ) break;
   }
   if(c<ldcs_listen_data.item_table_size) {
      debug_printf3("unregister fd %d c=%d\n",fd,c);
      ldcs_listen_data.item_table[c].state = LDCS_LISTEN_STATUS_FREE;
      ldcs_listen_data.item_table_used--;
   } else {
      printf("ldcs_listen_unregister_fd: entry not found\n");
      rc=-1;
   } 

   return(rc);
}

int ldcs_listen_signal_end_listen_loop( ) {
   int rc=0;
   ldcs_listen_data.signal_end=1;
   return(rc);
}

#undef max
#define max(x,y) ((x) > (y) ? (x) : (y))

int ldcs_listen() {
   int rc=-1;
   int r, nfds, fd, c;
   fd_set rd, wr, er;
   int do_listen=0;

   debug_printf2("Listening for data\n");
   do_listen=(ldcs_listen_data.item_table_used>0);
   while(do_listen && !do_exit) {
      nfds = 0;
      FD_ZERO(&rd);
      FD_ZERO(&wr);
      FD_ZERO(&er);
    
      /* collect all fds */
      for(c=0;c<ldcs_listen_data.item_table_size;c++) {
         if ( ldcs_listen_data.item_table[c].state == LDCS_LISTEN_STATUS_ACTIVE ) {
            fd     = ldcs_listen_data.item_table[c].fd;
            debug_printf3("add fd to FDSET %d\n",fd);
            FD_SET(fd, &rd);
            nfds = max(nfds, fd);  
         }
      }
    
      /* do select if sckets avail */
      if(nfds>0) {
         debug_printf3("start select nfds=%d\n",nfds);
         r = select(nfds + 1, &rd, &wr, &er, NULL);
         debug_printf3("after select r=%d\n",r);
      
         /* signal caught, do nothing */
         if (r == -1 && errno == EINTR) {
            continue;
         }
      
         /* error happened */
         if (r == -1)  _error("in listen");
      
         /* call callback function for all active fds */
         for(c=0;c<ldcs_listen_data.item_table_size;c++) {
            if ( ldcs_listen_data.item_table[c].state == LDCS_LISTEN_STATUS_ACTIVE ) {
               fd     = ldcs_listen_data.item_table[c].fd;
               if(FD_ISSET(fd, &rd)) {
                  debug_printf3("calling callback for fd %d id=%d\n",fd, ldcs_listen_data.item_table[c].id);
                  ldcs_listen_data.item_table[c].cb_func(ldcs_listen_data.item_table[c].fd,
                                                         ldcs_listen_data.item_table[c].id,
                                                         ldcs_listen_data.item_table[c].data);
               }
            }
         }
      }

      do_listen=(ldcs_listen_data.item_table_used>0);
    
      /* callback will unregister fd is not needed */
      if(loop_exit_cb) {
         loop_exit_cb(ldcs_listen_data.item_table_used, loop_exit_cb_data);
         do_listen=(ldcs_listen_data.item_table_used>0);
         debug_printf3("after check loop_exit_cb do_listen=%d\n",do_listen);
      }
      /* check if external signal is set */
      if(ldcs_listen_data.signal_end==1) {
         ldcs_listen_data.signal_end=0;
         do_listen=0;
         debug_printf3("after check signal_end do_listen=%d\n",do_listen);
      }

   } /* while */

   return(rc);
}

void mark_exit()
{
   do_exit = 1;
}
