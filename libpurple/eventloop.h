/**
 * @file eventloop.h Purple Event Loop API
 * @ingroup core
 */

/* purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */
#ifndef _PURPLE_EVENTLOOP_H_
#define _PURPLE_EVENTLOOP_H_

#include <glib.h>

/**
 * PurpleInputCondition:
 * @PURPLE_INPUT_READ:  A read condition.
 * @PURPLE_INPUT_WRITE: A write condition.
 *
 * An input condition.
 */
typedef enum
{
	PURPLE_INPUT_READ  = 1 << 0,
	PURPLE_INPUT_WRITE = 1 << 1

} PurpleInputCondition;

/**
 * PurpleInputFunction:
 *
 * The type of callbacks to handle events on file descriptors, as passed to
 * purple_input_add().  The callback will receive the @user_data passed to
 * purple_input_add(), the file descriptor on which the event occurred, and the
 * condition that was satisfied to cause the callback to be invoked.
 */
typedef void (*PurpleInputFunction)(gpointer, gint, PurpleInputCondition);

typedef struct _PurpleEventLoopUiOps PurpleEventLoopUiOps;

/**
 * PurpleEventLoopUiOps:
 *
 * An abstraction of an application's mainloop; libpurple will use this to
 * watch file descriptors and schedule timed callbacks.  If your application
 * uses the glib mainloop, there is an implementation of this struct in
 * <tt>libpurple/example/nullclient.c</tt> which you can use verbatim.
 */
struct _PurpleEventLoopUiOps
{
	/**
	 * Should create a callback timer with an interval measured in
	 * milliseconds.  The supplied @function should be called every
	 * @interval seconds until it returns %FALSE, after which it should not
	 * be called again.
	 *
	 * Analogous to g_timeout_add in glib.
	 *
	 * Note: On Win32, this function may be called from a thread other than
	 * the libpurple thread.  You should make sure to detect this situation
	 * and to only call "function" from the libpurple thread.
	 *
	 * @interval: the interval in <em>milliseconds</em> between calls
	 *                 to @function.
	 * @data:     arbitrary data to be passed to @function at each
	 *                 call.
	 * @todo Who is responsible for freeing @data ?
	 *
	 * Returns: a handle for the timeout, which can be passed to
	 *         #timeout_remove.
	 *
	 * @see purple_timeout_add
	 **/
	guint (*timeout_add)(guint interval, GSourceFunc function, gpointer data);

	/**
	 * Should remove a callback timer.  Analogous to g_source_remove in glib.
	 * @handle: an identifier for a timeout, as returned by
	 *               #timeout_add.
	 * Returns:       %TRUE if the timeout identified by @handle was
	 *               found and removed.
	 * @see purple_timeout_remove
	 */
	gboolean (*timeout_remove)(guint handle);

	/**
	 * Should add an input handler.  Analogous to g_io_add_watch_full in
	 * glib.
	 *
	 * @fd:        a file descriptor to watch for events
	 * @cond:      a bitwise OR of events on @fd for which @func
	 *                  should be called.
	 * @func:      a callback to fire whenever a relevant event on
	 *                  @fd occurs.
	 * @user_data: arbitrary data to pass to @fd.
	 * Returns:          an identifier for this input handler, which can be
	 *                  passed to #input_remove.
	 *
	 * @see purple_input_add
	 */
	guint (*input_add)(int fd, PurpleInputCondition cond,
	                   PurpleInputFunction func, gpointer user_data);

	/**
	 * Should remove an input handler.  Analogous to g_source_remove in glib.
	 * @handle: an identifier, as returned by #input_add.
	 * Returns:       %TRUE if the input handler was found and removed.
	 * @see purple_input_remove
	 */
	gboolean (*input_remove)(guint handle);


	/**
	 * If implemented, should get the current error status for an input.
	 *
	 * Implementation of this UI op is optional. Implement it if the UI's
	 * sockets or event loop needs to customize determination of socket
	 * error status.  If unimplemented, <tt>getsockopt(2)</tt> will be used
	 * instead.
	 *
	 * @see purple_input_get_error
	 */
	int (*input_get_error)(int fd, int *error);

	/**
	 * If implemented, should create a callback timer with an interval
	 * measured in seconds.  Analogous to g_timeout_add_seconds in glib.
	 *
	 * This allows UIs to group timers for better power efficiency.  For
	 * this reason, @interval may be rounded by up to a second.
	 *
	 * Implementation of this UI op is optional.  If it's not implemented,
	 * calls to purple_timeout_add_seconds() will be serviced by
	 * #timeout_add.
	 *
	 * @see purple_timeout_add_seconds()
	 **/
	guint (*timeout_add_seconds)(guint interval, GSourceFunc function,
	                             gpointer data);

	/*< private >*/
	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

G_BEGIN_DECLS

/**************************************************************************/
/** @name Event Loop API                                                  */
/**************************************************************************/
/*@{*/
/**
 * purple_timeout_add:
 * @interval:	The time between calls of the function, in
 *                      milliseconds.
 * @function:	The function to call.
 * @data:		data to pass to @function.
 *
 * Creates a callback timer.
 *
 * The timer will repeat until the function returns %FALSE. The
 * first call will be at the end of the first interval.
 *
 * If the timer is in a multiple of seconds, use purple_timeout_add_seconds()
 * instead as it allows UIs to group timers for power efficiency.
 *
 * Returns: A handle to the timer which can be passed to
 *         purple_timeout_remove() to remove the timer.
 */
guint purple_timeout_add(guint interval, GSourceFunc function, gpointer data);

/**
 * purple_timeout_add_seconds:
 * @interval:	The time between calls of the function, in
 *                      seconds.
 * @function:	The function to call.
 * @data:		data to pass to @function.
 *
 * Creates a callback timer.
 *
 * The timer will repeat until the function returns %FALSE. The
 * first call will be at the end of the first interval.
 *
 * This function allows UIs to group timers for better power efficiency.  For
 * this reason, @interval may be rounded by up to a second.
 *
 * Returns: A handle to the timer which can be passed to
 *         purple_timeout_remove() to remove the timer.
 */
guint purple_timeout_add_seconds(guint interval, GSourceFunc function, gpointer data);

/**
 * purple_timeout_remove:
 * @handle: The handle, as returned by purple_timeout_add().
 *
 * Removes a timeout handler.
 *
 * Returns: %TRUE if the handler was successfully removed.
 */
gboolean purple_timeout_remove(guint handle);

/**
 * purple_input_add:
 * @fd:        The input file descriptor.
 * @cond:      The condition type.
 * @func:      The callback function for data.
 * @user_data: User-specified data.
 *
 * Adds an input handler.
 *
 * Returns: The resulting handle (will be greater than 0).
 * @see g_io_add_watch_full
 */
guint purple_input_add(int fd, PurpleInputCondition cond,
                       PurpleInputFunction func, gpointer user_data);

/**
 * purple_input_remove:
 * @handle: The handle of the input handler. Note that this is the return
 *               value from purple_input_add(), <i>not</i> the file descriptor.
 *
 * Removes an input handler.
 */
gboolean purple_input_remove(guint handle);

/**
 * purple_input_get_error:
 * @fd:        The input file descriptor.
 * @error:     A pointer to an #int which on return will have the error, or
 *             0 if no error.
 *
 * Get the current error status for an input.
 *
 * The return value and error follow getsockopt() with a level of SOL_SOCKET and an
 * option name of SO_ERROR, and this is how the error is determined if the UI does not
 * implement the input_get_error UI op.
 *
 * Returns: 0 if there is no error; -1 if there is an error, in which case
 *          %errno will be set.
 */
int
purple_input_get_error(int fd, int *error);

/**
 * purple_input_pipe:
 * @pipefd: Array used to return file descriptors for both ends of pipe.
 *
 * Creates a pipe - an unidirectional data channel that can be used for
 * interprocess communication.
 *
 * File descriptors for both ends of pipe will be written into provided array.
 * The first one (pipefd[0]) can be used for reading, the second one (pipefd[1])
 * for writing.
 *
 * On Windows it's simulated by creating a pair of connected sockets, on other
 * systems pipe() is used.
 *
 * Returns: 0 on success, -1 on error.
 */
int
purple_input_pipe(int pipefd[2]);


/*@}*/


/**************************************************************************/
/** @name UI Registration Functions                                       */
/**************************************************************************/
/*@{*/
/**
 * purple_eventloop_set_ui_ops:
 * @ops: The UI operations structure.
 *
 * Sets the UI operations structure to be used for accounts.
 */
void purple_eventloop_set_ui_ops(PurpleEventLoopUiOps *ops);

/**
 * purple_eventloop_get_ui_ops:
 *
 * Returns the UI operations structure used for accounts.
 *
 * Returns: The UI operations structure in use.
 */
PurpleEventLoopUiOps *purple_eventloop_get_ui_ops(void);

/*@}*/

G_END_DECLS

#endif /* _PURPLE_EVENTLOOP_H_ */
