/**
 * @file slpcall.c SLP Call Functions
 *
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
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
#include "msn.h"
#include "slpcall.h"
#include "slpsession.h"

#include "slp.h"

/**************************************************************************
 * Util
 **************************************************************************/

char *
rand_guid()
{
	return g_strdup_printf("%4X%4X-%4X-%4X-%4X-%4X%4X%4X",
			rand() % 0xAAFF + 0x1111,
			rand() % 0xAAFF + 0x1111,
			rand() % 0xAAFF + 0x1111,
			rand() % 0xAAFF + 0x1111,
			rand() % 0xAAFF + 0x1111,
			rand() % 0xAAFF + 0x1111,
			rand() % 0xAAFF + 0x1111,
			rand() % 0xAAFF + 0x1111);
}

/**************************************************************************
 * SLP Call
 **************************************************************************/

MsnSlpCall *
msn_slp_call_new(MsnSlpLink *slplink)
{
	MsnSlpCall *slpcall;

	g_return_val_if_fail(slplink != NULL, NULL);

	slpcall = g_new0(MsnSlpCall, 1);

	slpcall->slplink = slplink;
	slplink->slp_calls =
		g_list_append(slplink->slp_calls, slpcall);

	slpcall->timer = gaim_timeout_add(MSN_SLPCALL_TIMEOUT, msn_slp_call_timeout, slpcall);

	return slpcall;
}

void
msn_slp_call_init(MsnSlpCall *slpcall, MsnSlpCallType type)
{
	slpcall->session_id = rand() % 0xFFFFFF00 + 4;
	slpcall->id = rand_guid();
	slpcall->type = type;
}

void
msn_slp_call_session_init(MsnSlpCall *slpcall)
{
	MsnSlpSession *slpsession;

	slpsession = msn_slp_session_new(slpcall);

	if (slpcall->session_init_cb)
		slpcall->session_init_cb(slpsession);

	slpcall->started = TRUE;
}

void
msn_slp_call_destroy(MsnSlpCall *slpcall)
{
	GList *e;

	g_return_if_fail(slpcall != NULL);

	if (slpcall->timer)
		gaim_timeout_remove(slpcall->timer);

	if (slpcall->id != NULL)
		g_free(slpcall->id);

	if (slpcall->branch != NULL)
		g_free(slpcall->branch);

	if (slpcall->data_info != NULL)
		g_free(slpcall->data_info);

	slpcall->slplink->slp_calls =
		g_list_remove(slpcall->slplink->slp_calls, slpcall);

	for (e = slpcall->slplink->slp_msgs; e != NULL; )
	{
		MsnSlpMessage *slpmsg = e->data;
		e = e->next;

		g_return_if_fail(slpmsg != NULL);

		if (slpmsg->slpcall == slpcall)
		{
#if 1
			msn_slpmsg_destroy(slpmsg);
#else
			slpmsg->wasted = TRUE;
#endif
		}
	}

	if (slpcall->end_cb != NULL)
		slpcall->end_cb(slpcall);

	g_free(slpcall);
}

void
msn_slp_call_invite(MsnSlpCall *slpcall, const char *euf_guid,
					int app_id, const char *context)
{
	MsnSlpLink *slplink;
	MsnSlpMessage *slpmsg;
	char *header;
	char *content;

	g_return_if_fail(slpcall != NULL);
	g_return_if_fail(context != NULL);

	slplink = slpcall->slplink;

	slpcall->branch = rand_guid();

	content = g_strdup_printf(
		"EUF-GUID: {%s}\r\n"
		"SessionID: %lu\r\n"
		"AppID: %d\r\n"
		"Context: %s\r\n\r\n",
		euf_guid,
		slpcall->session_id,
		app_id,
		context);

	header = g_strdup_printf("INVITE MSNMSGR:%s MSNSLP/1.0",
							 slplink->remote_user);

	slpmsg = msn_slpmsg_sip_new(slpcall, 0, header, slpcall->branch,
								"application/x-msnmsgr-sessionreqbody", content);
#ifdef DEBUG_SLP
	slpmsg->info = "SLP INVITE";
	slpmsg->text_body = TRUE;
#endif

	msn_slplink_send_slpmsg(slplink, slpmsg);

	g_free(header);
	g_free(content);
}

void
msn_slp_call_close(MsnSlpCall *slpcall)
{
	g_return_if_fail(slpcall != NULL);
	g_return_if_fail(slpcall->slplink != NULL);

	send_bye(slpcall, "application/x-msnmsgr-sessionclosebody");
	msn_slplink_unleash(slpcall->slplink);
	msn_slp_call_destroy(slpcall);
}

gboolean
msn_slp_call_timeout(gpointer data)
{
	gaim_debug_info("msn", "slpcall timeout\n");

	msn_slp_call_destroy(data);

	return FALSE;
}

MsnSlpCall *
msn_slp_process_msg(MsnSlpLink *slplink, MsnSlpMessage *slpmsg)
{
	MsnSlpCall *slpcall;
	const char *body;
	gsize body_len;

	slpcall = NULL;
	body = slpmsg->buffer;
	body_len = slpmsg->size;

	if (slpmsg->flags == 0x0)
	{
		slpcall = msn_slp_sip_recv(slplink, body, body_len);
	}
	else if (slpmsg->flags == 0x20 || slpmsg->flags == 0x1000030)
	{
		slpcall = msn_slplink_find_slp_call_with_session_id(slplink, slpmsg->session_id);

		if (slpcall != NULL)
		{
			if (slpcall->timer)
				gaim_timeout_remove(slpcall->timer);

			slpcall->cb(slpcall, body, body_len);

			/* TODO: Shall we send a BYE? I don't think so*/
#if 0
			send_bye(slpcall, "application/x-msnmsgr-sessionclosebody");
			msn_slplink_unleash(slpcall->slplink);
#endif

			slpcall->wasted = TRUE;
		}
	}
#if 0
	else if (slpmsg->flags == 0x100)
	{
		slpcall = slplink->directconn->initial_call;

		if (slpcall != NULL)
			msn_slp_call_session_init(slpcall);
	}
#endif

	if (slpcall != NULL)
	{
		if (slpcall->timer)
			gaim_timeout_remove(slpcall->timer);

		slpcall->timer = gaim_timeout_add(MSN_SLPCALL_TIMEOUT,
										  msn_slp_call_timeout, slpcall);
	}

	return slpcall;
}
