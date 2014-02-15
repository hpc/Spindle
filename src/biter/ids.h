#if !defined(IDS_H_)
#define IDS_H_

int biterc_get_job_id();
int biterc_get_unique_number(int *mrank, int *num_set, void *session);
unsigned int biterc_get_rank();

#endif
