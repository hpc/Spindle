/*
This file is part of Spindle.  For copyright information see the COPYRIGHT 
file in the top level directory, or at 
https://github.com/hpc/Spindle/blob/master/COPYRIGHT

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License (as published by the Free Software
Foundation) version 2.1 dated February 1999.  This program is distributed in the
hope that it will be useful, but WITHOUT ANY WARRANTY; without even the IMPLIED
WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms 
and conditions of the GNU General Public License for more details.  You should 
have received a copy of the GNU Lesser General Public License along with this 
program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA
*/

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
