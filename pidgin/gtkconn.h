/**
 * @file gtkconn.h GTK+ Connection API
 *
 * gaim
 *
 * Pidgin is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef _PIDGINCONN_H_
#define _PIDGINCONN_H_

/**************************************************************************/
/** @name GTK+ Connection API                                             */
/**************************************************************************/
/*@{*/

/**
 * Gets GTK+ Connection UI ops
 *
 * @return UI operations struct
 */
GaimConnectionUiOps *pidgin_connections_get_ui_ops(void);

/*@}*/

/**
 * Returns the GTK+ connection handle.
 *
 * @return The handle to the GTK+ connection system.
 */
void *pidgin_connection_get_handle(void);

/**
 * Initializes the GTK+ connection system.
 */
void pidgin_connection_init(void);

/**
 * Uninitializes the GTK+ connection system.
 */
void pidgin_connection_uninit(void);

#endif /* _PIDGINCONN_H_ */
