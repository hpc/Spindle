#ifndef LDCS_HASH_H
#define LDCS_HASH_H

#define HASH_SIZE (10*1024)
typedef unsigned ldcs_hash_key_t;

typedef enum {
   HASH_ENTRY_STATUS_USED,
   HASH_ENTRY_STATUS_NEW,
   HASH_ENTRY_STATUS_FREE,
   HASH_ENTRY_STATUS_UNKNOWN
} ldcs_hash_entry_status_t;

struct ldcs_hash_entry_t
{
  ldcs_hash_entry_status_t  state;
  int   ostate;
  char *dirname;
  char *filename;
  char *localpath;
  ldcs_hash_key_t hash_val;
  struct ldcs_hash_entry_t *next;
};

int ldcs_hash_init();
ldcs_hash_key_t ldcs_hash_Val(const char *str);
void ldcs_hash_addEntry(char *dirname, char *filename);

struct ldcs_hash_entry_t *ldcs_hash_updateEntryLocalPath(char *filename, char *dirname, char *localpath);
struct ldcs_hash_entry_t *ldcs_hash_updateEntryOState(char *filename, char *dirname, int ostate);

struct ldcs_hash_entry_t *ldcs_hash_Lookup(const char *filename);
struct ldcs_hash_entry_t *ldcs_hash_Lookup_FN_and_DIR(const char *filename, const char *dirname);
struct ldcs_hash_entry_t *ldcs_hash_Lookup_FN_and_DIR_Ostate(const char *filename, const char *dirname, int ostate);
struct ldcs_hash_entry_t *ldcs_hash_Lookup_FN_and_Ostate(const char *filename, int ostate);


void ldcs_hash_dump(char *tofile);
struct ldcs_hash_entry_t *ldcs_hash_getFirstNewEntry();
struct ldcs_hash_entry_t *ldcs_hash_getNextNewEntry();

#endif
