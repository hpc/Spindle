#if !defined(BITERC_H_)
#define BITERC_H_

#if defined(__cplusplus)
extern "C" {
#endif

extern int biterc_newsession(const char *tmpdir, size_t shm_size);
extern int biterc_read(int biter_session, void *buf, size_t size);
extern int biterc_write(int biter_session, void *buf, size_t size);
extern int biterc_get_id(int biter_session);
extern unsigned int biterc_get_rank(int session_id);
extern const char *biterc_lasterror_str();

#if defined(__cplusplus)
}
#endif

#endif
