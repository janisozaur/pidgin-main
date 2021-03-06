/*
 * purple
 *
 * File: win32dep.h
 *
 * Copyright (C) 2002-2003, Herman Bloggs <hermanator12002@yahoo.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 *
 */
#ifndef _WIN32DEP_H_
#define _WIN32DEP_H_

#include <config.h>

#include <winsock2.h>
#include <windows.h>
#include <shlobj.h>
#include <process.h>
#include "wpurpleerror.h"
#include "libc_interface.h"

G_BEGIN_DECLS

/* the winapi headers don't yet have winhttp.h, so we use the struct from msdn directly */
typedef struct {
  BOOL fAutoDetect;
  LPWSTR lpszAutoConfigUrl;
  LPWSTR lpszProxy;
  LPWSTR lpszProxyBypass;
} WINHTTP_CURRENT_USER_IE_PROXY_CONFIG;

/* rpcndr.h defines small as char, causing problems, so we need to undefine it */
#undef small

/*
 *  PROTOS
 */

/**
 ** win32dep.c
 **/
/* Windows helper functions */
FARPROC wpurple_find_and_loadproc(const char *dllname, const char *procedure);
gboolean wpurple_reg_val_exists(HKEY rootkey, const char *subkey, const char *valname);
gboolean wpurple_read_reg_dword(HKEY rootkey, const char *subkey, const char *valname, LPDWORD result);
char *wpurple_read_reg_string(HKEY rootkey, const char *subkey, const char *valname); /* needs to be g_free'd */
gboolean wpurple_write_reg_string(HKEY rootkey, const char *subkey, const char *valname, const char *value);
char *wpurple_escape_dirsep(const char *filename); /* needs to be g_free'd */

/* Simulate unix pipes by creating a pair of connected sockets */
int wpurple_input_pipe(int pipefd[2]);

/* Determine Purple paths */
gchar *wpurple_get_special_folder(int folder_type); /* needs to be g_free'd */
const char *wpurple_bin_dir(void);
const char *wpurple_data_dir(void);
const char *wpurple_lib_dir(const char *subdir);
const char *wpurple_locale_dir(void);
const char *wpurple_home_dir(void);
const char *wpurple_sysconf_dir(void);

/* init / cleanup */
void wpurple_init(void);
void wpurple_cleanup(void);

G_END_DECLS

#endif /* _WIN32DEP_H_ */

