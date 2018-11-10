/*
This file is part of Spindle.  For copyright information see the COPYRIGHT 
file in the top level directory, or at 
https://github.com/hpc/Spindle/blob/master/COPYRIGHT

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License (as published by the Free Software
Foundation) version 2.1 dated February 1999.  This program is distributed in the
hope that it will be useful, but WITHOUT ANY WARRANTY; without even the IMPLIED
WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms 
and conditions of the GNU Lesser General Public License for more details.  You should 
have received a copy of the GNU Lesser General Public License along with this 
program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include "ldcs_api.h"
#include "shmcache.h"
#include "sheep.h"
#include "client_heap.h"
#include "shmutil.h"
#include "spindle_debug.h"
#include <string.h>
#include <assert.h>

struct entry_t {
   sheep_ptr_t libname;
   sheep_ptr_t result;
   sheep_ptr_t lru_next;
   sheep_ptr_t lru_prev;
   unsigned int hash_key;
   unsigned int pending_count;
   sheep_ptr_t hash_next;
};

char *in_progress;

#define HASH_SIZE 1024

static char return_name[MAX_PATH_LEN+1];

static sheep_ptr_t *hash_ptr;
static sheep_ptr_t *lru_head;
static sheep_ptr_t *lru_end;
static sheep_ptr_t hash_error;
static size_t heap_limit = 0;
static size_t *heap_used = 0;
static lock_t cache_lock;
static sheep_ptr_t *table;

static shminfo_t *shminfo = NULL;

static void upgrade_to_writelock()
{
}

static void downgrade_to_readlock()
{
}

static void take_writer_lock()
{
   take_lock(&cache_lock);
}

static void release_writer_lock()
{
   release_lock(&cache_lock);
}

static void take_reader_lock()
{
   take_lock(&cache_lock);
}

static void release_reader_lock()
{
   release_lock(&cache_lock);
}

static void take_sheep_lock()
{
   take_heap_lock(shminfo);
}

static void release_sheep_lock()
{
   release_heap_lock(shminfo);
}

static int clean_oldest_entry();
static void *malloc_sheep_cache(size_t size)
{
   void *newalloc;
   size_t alloc_size;

   take_sheep_lock();

   alloc_size = sheep_alloc_size(size);

   while (heap_limit && *heap_used + alloc_size > heap_limit) {
      debug_printf3("Cleaning old entries in shmcache.  heap_limit = %lu, heap_used = %lu, alloc_size = %lu\n",
                    heap_limit, *heap_used, alloc_size);
      if (clean_oldest_entry() == -1) {
         /* A completely cleaned heap does not have enough space */
         release_sheep_lock();
         return NULL;
      }
   }

   for (;;) {
      newalloc = malloc_sheep(size);
      if (newalloc)
         break;
      debug_printf3("shmcache allocation of size %lu failed, cleaning up older entries\n", size);
      if (clean_oldest_entry() == -1) {
         /* A completely cleaned heap does not have enough space */
         release_sheep_lock();
         return NULL;
      }
   }
   
   *heap_used += alloc_size;
   release_sheep_lock();
   return newalloc;
}

static void free_sheep_str(char *str)
{
   size_t alloc_size;
   take_sheep_lock();
   alloc_size = sheep_alloc_size(strlen(str) + 1);
   assert(*heap_used >= alloc_size);
   *heap_used -= alloc_size;
   free_sheep(str);
   release_sheep_lock();
}

static void free_sheep_entry(struct entry_t *entry)
{
   size_t alloc_size;
   take_sheep_lock();
   alloc_size = sheep_alloc_size(sizeof(struct entry_t));
   assert(*heap_used >= alloc_size);
   *heap_used -= alloc_size;
   free_sheep(entry);
   release_sheep_lock();
}

static void mark_recently_used(struct entry_t *entry, int have_write_lock)
{
   struct entry_t *pentry, *nentry;
   if (sheep_ptr_equals(ptr_sheep(entry), *lru_head))
      return;

   if (!have_write_lock)
      upgrade_to_writelock();

   nentry = (struct entry_t *) sheep_ptr(&entry->lru_next);
   pentry = (struct entry_t *) sheep_ptr(&entry->lru_prev);
   
   if (nentry)
      nentry->lru_prev = entry->lru_prev;
   if (pentry)
      pentry->lru_next = entry->lru_next;

   if (sheep_ptr_equals(ptr_sheep(entry), *lru_end)) {
      *lru_end = entry->lru_prev;
   }

   entry->lru_next = *lru_head;
   entry->lru_prev = ptr_sheep(SHEEP_NULL);

   if (!IS_SHEEP_NULL(lru_head)) {
      nentry = (struct entry_t *) sheep_ptr(lru_head);
      nentry->lru_prev = ptr_sheep(entry);
   }
   *lru_head = ptr_sheep(entry);
   if (IS_SHEEP_NULL(lru_end)) {
      set_sheep_ptr(lru_end, entry);
   }
   if (!have_write_lock)
      downgrade_to_readlock();
}

static int clean_oldest_entry()
{
   sheep_ptr_t pentry, i;
   struct entry_t *entry, *prev_hash_entry;

   debug_printf3("Cleaning oldest entries from shmcache for more space\n");
   if (IS_SHEEP_NULL(lru_end))
      return -1;
   entry = (struct entry_t *) sheep_ptr(lru_end);

   if (sheep_ptr(&entry->result) == in_progress) {
      debug_printf("Tried to delete in_progress shmcache entry.  Not cleaning\n");
      return -1;
   }
   if (entry->pending_count) {
      debug_printf("Tried to delete pending shmcache entry.  Not cleaning\n");
      return -1;
   }
   pentry = entry->lru_prev;

   prev_hash_entry = NULL;
   debug_printf3("Cleaning entry %s -> %s\n",
                 ((char *) sheep_ptr(&entry->libname)) ? : "[NULL]", 
                 (((char *) sheep_ptr(&entry->result)) ? 
                  (((char *) sheep_ptr(&entry->result)) == in_progress ? "[IN PROGRESS]" : ((char *) sheep_ptr(&entry->result))) :
                  "[NULL]"));
   for (i = table[entry->hash_key]; sheep_ptr(&i) != (void*) entry; i = prev_hash_entry->hash_next)
      prev_hash_entry = (struct entry_t *) sheep_ptr(&i);

   if (prev_hash_entry)
      prev_hash_entry->hash_next = entry->hash_next;
   else
      table[entry->hash_key] = ptr_sheep(SHEEP_NULL);

   free_sheep_str((char *) sheep_ptr(& entry->libname));
   if (!IS_SHEEP_NULL(&entry->result)) {
      free_sheep_str((char *) sheep_ptr(&entry->result));
   }
   free_sheep_entry(entry);

   if (entry == sheep_ptr(lru_head)) {
      *lru_head = ptr_sheep(SHEEP_NULL);
      *lru_end = ptr_sheep(SHEEP_NULL);
   }
   else {
      *lru_end = pentry;
      ((struct entry_t *) sheep_ptr(lru_end))->lru_next = ptr_sheep(SHEEP_NULL);
   }

   return 0;
}

static unsigned int str_hash(const char *str)
{
   unsigned long hashv = 5381;
   int c;
   while ((c = *str++))
      hashv = ((hashv << 5) + hashv) + c;
   return hashv % HASH_SIZE;
}

static int shmcache_lookup_worker(const char *libname, char **result, int have_write_lock, struct entry_t **oentry)
{
   unsigned int key;
   struct entry_t *entry;
   sheep_ptr_t p;
   char *ent_result;

   debug_printf3("Looking up %s in shmcache\n", libname);
   key = str_hash(libname);
   for (p = table[key]; !IS_SHEEP_NULL(&p); p = entry->hash_next) {
      entry = (struct entry_t *) sheep_ptr(&p);
      if (entry->hash_key != key)
         continue;
      if (IS_SHEEP_NULL(&entry->libname))
         continue;
      if (strcmp((char *) sheep_ptr(&entry->libname), libname) != 0)
         continue;
      break;
   }
   if (IS_SHEEP_NULL(&p)) {
      /* Not found in cache */
      debug_printf3("Didn't find %s in shmcache\n", libname);
      return -1;
   }

   if (oentry) {
      *oentry = entry;
   }
   
   mark_recently_used(entry, have_write_lock);
   ent_result = (char *) sheep_ptr(&entry->result);
   if (!ent_result) {
      /* Entry is a cached negative result */
      *result = NULL;
      debug_printf3("Found %s in shmcache with negative result\n", libname);
      return 0;
   }
   
   /* Entry is cached with postive result */
   if (ent_result == in_progress) {
      debug_printf3("Found %s in shmcache with in_progress entry\n", libname);
      *result = in_progress;
   }
   else {
      debug_printf3("Found %s in shmcache with mapping to %s\n", libname, ent_result);
      *result = ent_result;
   }

   return 0;
}

static int shmcache_add_worker(const char *libname, const char *mapped_name, int update)
{
   char *lookup_result;
   char *libname_str, *mappedname_str = NULL;
   int result;
   size_t libname_len, mappedname_len;
   struct entry_t *entry;

   debug_printf3("%s library %s in shmcache to value %s\n",
                 update ? "Updating" : "Adding", libname,
                 mapped_name == in_progress ? "[IN PROGRESS]" : (mapped_name ? : "[NULL]"));
   if (update) {
      result = shmcache_lookup_worker(libname, &lookup_result, 1, &entry);
      if (result == -1) {
         err_printf("Could not find shmcache entry for %s while updating\n", libname);
         return -1;
      }
      if (sheep_ptr(&entry->result) != in_progress && !IS_SHEEP_NULL(&entry->result))
         free_sheep_str(sheep_ptr(&entry->result));
      if (mapped_name) {
         mappedname_len = strlen(mapped_name) + 1;
         mappedname_str = (char *) malloc_sheep_cache(mappedname_len);
         if (!mappedname_str) {
            free_sheep_str(sheep_ptr(&entry->libname));
            entry->libname = ptr_sheep(SHEEP_NULL);
            entry->result = ptr_sheep(SHEEP_NULL);
            entry->hash_key = 0;
            err_printf("Could not free space in cache for updated entry for %s\n", libname);
            return -1;
         }
         strncpy(mappedname_str, mapped_name, mappedname_len);
      }
      else {
         mappedname_str = NULL;
      }

      entry->result = ptr_sheep(mappedname_str);
      debug_printf3("Successfully updated shmcache entry %s\n", libname);
      return 0;
   }

   entry = (struct entry_t *) malloc_sheep_cache(sizeof(struct entry_t));
   if (!entry)
      return -1;
   libname_len = strlen(libname)+1;
   libname_str = (char *) malloc_sheep_cache(libname_len);
   if (!libname_str) {
      free_sheep_entry(entry);
      return -1;
   }
   strncpy(libname_str, libname, libname_len);

   if (mapped_name == in_progress)
      mappedname_str = in_progress;
   else if (mapped_name) {
      mappedname_len = strlen(mapped_name) + 1;
      mappedname_str = (char *) malloc_sheep_cache(mappedname_len);
      if (!mappedname_str) {
         free_sheep_entry(entry);
         free_sheep_str(libname_str);
      }
      strncpy(mappedname_str, mapped_name, mappedname_len);
   }

   entry->libname = ptr_sheep(libname_str);
   entry->result = mappedname_str ? ptr_sheep(mappedname_str) : ptr_sheep(SHEEP_NULL);
   entry->hash_key = str_hash(libname);
   entry->hash_next = table[entry->hash_key];
   entry->lru_next.val = entry->lru_prev.val = 0;
   entry->pending_count = 0;
   table[entry->hash_key] = ptr_sheep(entry);
   mark_recently_used(entry, 1);
   debug_printf3("Successfully created shmcache entry %s\n", libname);
   return 0;
}

static int init_cache_locks()
{
   cache_lock.lock = shminfo->shared_header->shmcache.locks + 0;
   cache_lock.held_by = shminfo->shared_header->shmcache.locks + 1;
   cache_lock.id = -1;

   return init_heap_lock(shminfo);
}

static int setup_cache_ids()
{
   int result;
   result = setup_ids(shminfo);
   if (result == -1)
      return -1;
   
   cache_lock.id = shminfo->id;
   return 0;
}

static int init_cache(size_t hlimit)
{
   void *newhash;

   if (hlimit == 0)
      return 0;

   hash_ptr = &shminfo->shared_header->shmcache.hash;
   lru_head = &shminfo->shared_header->shmcache.lru_head;
   lru_end = &shminfo->shared_header->shmcache.lru_end;
   heap_limit = hlimit;
   heap_used = &shminfo->shared_header->shmcache.heap_used;

   hash_error = ptr_sheep(((unsigned char *) shminfo->mem) + shminfo->size);
   in_progress = sheep_ptr(&hash_error);

   if (!hlimit) {
      debug_printf("No shm cache space allocated.  Disabling shmcache\n");
      *hash_ptr = hash_error;
      table = NULL;
      return 0;
   }
   
   if (IS_SHEEP_NULL(hash_ptr)) {
      take_writer_lock();
      if (IS_SHEEP_NULL(hash_ptr)) {
         newhash = malloc_sheep_cache(sizeof(sheep_ptr_t) * HASH_SIZE);
         if (!newhash) {
            debug_printf("Not enough shm space to allocate hash table.  Disabling shmcache\n");
            *hash_ptr = hash_error;
            table = NULL;
            release_writer_lock();
            return 0;
         }
         memset(newhash, 0, sizeof(sheep_ptr_t) * HASH_SIZE);
         *hash_ptr = ptr_sheep(newhash);
      }
      release_writer_lock();
   }
   table = sheep_ptr(hash_ptr);

   return 0;
}

int shmcache_lookup_or_add(const char *libname, char **result)
{
   int iresult;
   char *strresult = NULL;
   if (!table)
      return -1;
   take_reader_lock();
   iresult = shmcache_lookup_worker(libname, &strresult, 0, NULL);
   if (iresult == -1) {
      upgrade_to_writelock();
      shmcache_add_worker(libname, in_progress, 0);
      downgrade_to_readlock();
   }

   if (strresult == in_progress)
      *result = in_progress;
   else if (strresult) {
      *result = strncpy(return_name, strresult, sizeof(return_name));
      return_name[sizeof(return_name)-1] = '\0';
   }
   else {
      *result = NULL;
   }
                        
   release_reader_lock();
   return iresult;
}

int shmcache_add(const char *libname, const char *mapped_name)
{
   int result;
   if (!table)
      return 0;

   take_writer_lock();
   result = shmcache_add_worker(libname, mapped_name, 0);
   release_writer_lock();
   return result;
}

int shmcache_update(const char *libname, const char *mapped_name)
{
   int result;
   if (!table)
      return 0;

   take_writer_lock();
   result = shmcache_add_worker(libname, mapped_name, 1);
   release_writer_lock();
   return result;
}

int shmcache_init(const char *tmpdir, int unique_number, size_t shm_size, size_t hlimit)
{
   int result;
   result = init_shm(tmpdir, shm_size, unique_number, &shminfo);
   if (result == -1) {
      err_printf("Error initializing shared memory\n");
      return -1;
   }

   result = init_cache_locks();
   if (result == -1) {
      err_printf("Error from init_cache_locks\n");
      return -1;
   }

   result = setup_cache_ids();
   if (result == -1) {
      err_printf("Error from setup_cache_ids\n");
      return -1;
   }

   result = init_heap(shminfo);
   if (result == -1) {
      err_printf("Error from init_heap\n");
      return -1;
   }

   result = init_cache(hlimit);
   if (result == -1) {
      err_printf("Error from init_cache\n");
      return -1;
   }

   debug_printf2("Successfully initialized shmcache to size %lu with limit %lu\n",
                 shm_size, hlimit);
   return 0;
}

int shmcache_post_fork()
{
   update_shm_id(shminfo);
   return 0;
}

int shmcache_waitfor_update(const char *libname, char **result)
{
   int iresult;
   int set_pending_count = 0;
   char *sresult;
   struct entry_t *entry;
   volatile sheep_ptr_t* entry_result; 
   if (!table)
      return -1;

   take_reader_lock();
   iresult = shmcache_lookup_worker(libname, result, 0, &entry);
   if (iresult == -1) {
      release_reader_lock();
      return -1;
   }
   if (sheep_ptr(&entry->result) == in_progress) {
      upgrade_to_writelock();
      entry->pending_count++;
      downgrade_to_readlock();
      set_pending_count = 1;
   }
   release_reader_lock();

   debug_printf3("Blocking until %s is updated in shmcache\n", libname);
   entry_result = &entry->result;
   while (volatile_sheep_ptr(entry_result) == in_progress);

   sresult = sheep_ptr(&entry->result);
   
   if (sresult == NULL)
      *result = NULL;
   else {
      *result = strncpy(return_name, (char *) sheep_ptr(&entry->result), sizeof(return_name));
      return_name[sizeof(return_name)-1] = '\0';
   }
   
   if (set_pending_count) {
      take_writer_lock();
      entry->pending_count--;
      release_writer_lock();
   }

   return 0;
}
