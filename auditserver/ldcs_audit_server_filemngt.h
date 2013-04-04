/*
This file is part of Spindle.  For copyright information see the COPYRIGHT 
file in the top level directory, or at 
<TODO:URL>.

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

#ifndef LDCS_AUDIT_SERVER_FILEMNGT_H
#define LDCS_AUDIT_SERVER_FILEMNGT_H


int ldcs_audit_server_filemngt_init (char* location);

int ldcs_audit_server_filemngt_read_file ( char *filename, char *dirname, char *fullname, 
					   int domangle, ldcs_message_t* msg );
int ldcs_audit_server_filemngt_store_file ( ldcs_message_t* msg, char **filename, 
					    char **dirname, char **localpath, int *domangle );

int ldcs_audit_server_filemngt_clean();

#endif
