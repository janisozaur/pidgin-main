/**
 * @file msn.c The MSN protocol plugin
 *
 * purple
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
#define PHOTO_SUPPORT 1

#include "msn.h"
#include "accountopt.h"
#include "contact.h"
#include "msg.h"
#include "page.h"
#include "pluginpref.h"
#include "prefs.h"
#include "session.h"
#include "smiley.h"
#include "state.h"
#include "util.h"
#include "cmds.h"
#include "core.h"
#include "prpl.h"
#include "msnutils.h"
#include "version.h"

#include "msg.h"
#include "switchboard.h"
#include "notification.h"
#include "sync.h"
#include "slplink.h"

#if PHOTO_SUPPORT
#define MAX_HTTP_BUDDYICON_BYTES (200 * 1024)
#include "imgstore.h"
#endif

typedef struct
{
	PurpleConnection *gc;
	const char *passport;

} MsnMobileData;

typedef struct
{
	PurpleConnection *gc;
	char *name;

} MsnGetInfoData;

typedef struct
{
	MsnGetInfoData *info_data;
	char *stripped;
	char *url_buffer;
	PurpleNotifyUserInfo *user_info;
	char *photo_url_text;

} MsnGetInfoStepTwoData;

typedef struct
{
	PurpleConnection *gc;
	const char *who;
	char *msg;
	PurpleMessageFlags flags;
	time_t when;
} MsnIMData;

typedef struct
{
	char *smile;
	PurpleSmiley *ps;
	MsnObject *obj;
} MsnEmoticon;

typedef struct
{
	PurpleConnection *pc;
	PurpleBuddy *buddy;
	PurpleGroup *group;
} MsnAddReqData;

static const char *
msn_normalize(const PurpleAccount *account, const char *str)
{
	static char buf[BUF_LEN];
	char *tmp;

	g_return_val_if_fail(str != NULL, NULL);

	g_snprintf(buf, sizeof(buf), "%s%s", str,
			   (strchr(str, '@') ? "" : "@hotmail.com"));

	tmp = g_utf8_strdown(buf, -1);
	strncpy(buf, tmp, sizeof(buf));
	g_free(tmp);

	return buf;
}

gboolean
msn_email_is_valid(const char *passport)
{
	if (purple_email_is_valid(passport)) {
		/* Special characters aren't allowed in domains, so only go to '@' */
		while (*passport != '@') {
			if (*passport == '/')
				return FALSE;
			else if (*passport == '?')
				return FALSE;
			else if (*passport == '=')
				return FALSE;
			/* MSN also doesn't like colons, but that's checked already */

			passport++;
		}

		return TRUE;
	}

	return FALSE;
}

static gboolean
msn_send_attention(PurpleConnection *gc, const char *username, guint type)
{
	MsnMessage *msg;
	MsnSession *session;
	MsnSwitchBoard *swboard;

	msg = msn_message_new_nudge();
	session = gc->proto_data;
	swboard = msn_session_get_swboard(session, username, MSN_SB_FLAG_IM);

	msn_switchboard_send_msg(swboard, msg, TRUE);
	msn_message_destroy(msg);

	return TRUE;
}

static GList *
msn_attention_types(PurpleAccount *account)
{
	static GList *list = NULL;

	if (!list) {
		list = g_list_append(list, purple_attention_type_new("Nudge", _("Nudge"),
				_("%s has nudged you!"), _("Nudging %s...")));
	}

	return list;
}

static GHashTable *
msn_get_account_text_table(PurpleAccount *unused)
{
	GHashTable *table;

	table = g_hash_table_new(g_str_hash, g_str_equal);

	g_hash_table_insert(table, "login_label", (gpointer)_("Email Address..."));

	return table;
}

static PurpleCmdRet
msn_cmd_nudge(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data)
{
	PurpleAccount *account = purple_conversation_get_account(conv);
	PurpleConnection *gc = purple_account_get_connection(account);
	const gchar *username;

	username = purple_conversation_get_name(conv);

	purple_prpl_send_attention(gc, username, MSN_NUDGE);

	return PURPLE_CMD_RET_OK;
}

void
msn_act_id(PurpleConnection *gc, const char *entry)
{
	MsnCmdProc *cmdproc;
	MsnSession *session;
	PurpleAccount *account;
	const char *alias;

	session = gc->proto_data;
	cmdproc = session->notification->cmdproc;
	account = purple_connection_get_account(gc);

	if (entry && *entry)
	{
		char *tmp = g_strdup(entry);
		alias = purple_url_encode(g_strstrip(tmp));
		g_free(tmp);
	}
	else
		alias = "";

	if (strlen(alias) > BUDDY_ALIAS_MAXLEN)
	{
		purple_notify_error(gc, NULL,
						  _("Your new MSN friendly name is too long."), NULL);
		return;
	}

	if (*alias == '\0') {
		alias = purple_url_encode(purple_account_get_username(account));
	}

	msn_cmdproc_send(cmdproc, "PRP", "MFN %s", alias);
}

static void
msn_set_prp(PurpleConnection *gc, const char *type, const char *entry)
{
	MsnCmdProc *cmdproc;
	MsnSession *session;

	session = gc->proto_data;
	cmdproc = session->notification->cmdproc;

	if (entry == NULL || *entry == '\0')
	{
		msn_cmdproc_send(cmdproc, "PRP", "%s", type);
	}
	else
	{
		msn_cmdproc_send(cmdproc, "PRP", "%s %s", type,
						 purple_url_encode(entry));
	}
}

static void
msn_set_home_phone_cb(PurpleConnection *gc, const char *entry)
{
	msn_set_prp(gc, "PHH", entry);
}

static void
msn_set_work_phone_cb(PurpleConnection *gc, const char *entry)
{
	msn_set_prp(gc, "PHW", entry);
}

static void
msn_set_mobile_phone_cb(PurpleConnection *gc, const char *entry)
{
	msn_set_prp(gc, "PHM", entry);
}

static void
enable_msn_pages_cb(PurpleConnection *gc)
{
	msn_set_prp(gc, "MOB", "Y");
}

static void
disable_msn_pages_cb(PurpleConnection *gc)
{
	msn_set_prp(gc, "MOB", "N");
}

static void
send_to_mobile(PurpleConnection *gc, const char *who, const char *entry)
{
	MsnTransaction *trans;
	MsnSession *session;
	MsnCmdProc *cmdproc;
	MsnPage *page;
	MsnMessage *msg;
	MsnUser *user;
	char *payload = NULL;
	const char *mobile_number = NULL;
	gsize payload_len;

	session = gc->proto_data;
	cmdproc = session->notification->cmdproc;

	page = msn_page_new();
	msn_page_set_body(page, entry);

	payload = msn_page_gen_payload(page, &payload_len);

	if ((user = msn_userlist_find_user(session->userlist, who)) &&
		(mobile_number = msn_user_get_mobile_phone(user)) &&
		mobile_number[0] == '+') {
		/* if msn_user_get_mobile_phone() has a + in front, it's a number
		   that from the buddy's contact card */
		trans = msn_transaction_new(cmdproc, "PGD", "tel:%s 1 %" G_GSIZE_FORMAT,
			mobile_number, payload_len);
	} else {
		/* otherwise we send to whatever phone number the buddy registered
		   with msn */
		trans = msn_transaction_new(cmdproc, "PGD", "%s 1 %" G_GSIZE_FORMAT,
			who, payload_len);
	}

	msn_transaction_set_payload(trans, payload, payload_len);
	g_free(payload);

	msg = msn_message_new_plain(entry);
	msn_transaction_set_data(trans, msg);

	msn_page_destroy(page);

	msn_cmdproc_send_trans(cmdproc, trans);
}

static void
send_to_mobile_cb(MsnMobileData *data, const char *entry)
{
	send_to_mobile(data->gc, data->passport, entry);
	g_free(data);
}

static void
close_mobile_page_cb(MsnMobileData *data, const char *entry)
{
	g_free(data);
}

/* -- */

static void
msn_show_set_friendly_name(PurplePluginAction *action)
{
	PurpleConnection *gc;
	PurpleAccount *account;
	char *tmp;

	gc = (PurpleConnection *) action->context;
	account = purple_connection_get_account(gc);

	tmp = g_strdup_printf(_("Set friendly name for %s."),
	                      purple_account_get_username(account));
	purple_request_input(gc, _("Set your friendly name."), tmp,
					   _("This is the name that other MSN buddies will "
						 "see you as."),
					   purple_connection_get_display_name(gc), FALSE, FALSE, NULL,
					   _("OK"), G_CALLBACK(msn_act_id),
					   _("Cancel"), NULL,
					   account, NULL, NULL,
					   gc);
	g_free(tmp);
}

static void
msn_show_set_home_phone(PurplePluginAction *action)
{
	PurpleConnection *gc;
	MsnSession *session;

	gc = (PurpleConnection *) action->context;
	session = gc->proto_data;

	purple_request_input(gc, NULL, _("Set your home phone number."), NULL,
					   msn_user_get_home_phone(session->user), FALSE, FALSE, NULL,
					   _("OK"), G_CALLBACK(msn_set_home_phone_cb),
					   _("Cancel"), NULL,
					   purple_connection_get_account(gc), NULL, NULL,
					   gc);
}

static void
msn_show_set_work_phone(PurplePluginAction *action)
{
	PurpleConnection *gc;
	MsnSession *session;

	gc = (PurpleConnection *) action->context;
	session = gc->proto_data;

	purple_request_input(gc, NULL, _("Set your work phone number."), NULL,
					   msn_user_get_work_phone(session->user), FALSE, FALSE, NULL,
					   _("OK"), G_CALLBACK(msn_set_work_phone_cb),
					   _("Cancel"), NULL,
					   purple_connection_get_account(gc), NULL, NULL,
					   gc);
}

static void
msn_show_set_mobile_phone(PurplePluginAction *action)
{
	PurpleConnection *gc;
	MsnSession *session;

	gc = (PurpleConnection *) action->context;
	session = gc->proto_data;

	purple_request_input(gc, NULL, _("Set your mobile phone number."), NULL,
					   msn_user_get_mobile_phone(session->user), FALSE, FALSE, NULL,
					   _("OK"), G_CALLBACK(msn_set_mobile_phone_cb),
					   _("Cancel"), NULL,
					   purple_connection_get_account(gc), NULL, NULL,
					   gc);
}

static void
msn_show_set_mobile_pages(PurplePluginAction *action)
{
	PurpleConnection *gc;

	gc = (PurpleConnection *) action->context;

	purple_request_action(gc, NULL, _("Allow MSN Mobile pages?"),
						_("Do you want to allow or disallow people on "
						  "your buddy list to send you MSN Mobile pages "
						  "to your cell phone or other mobile device?"),
						PURPLE_DEFAULT_ACTION_NONE,
						purple_connection_get_account(gc), NULL, NULL,
						gc, 3,
						_("Allow"), G_CALLBACK(enable_msn_pages_cb),
						_("Disallow"), G_CALLBACK(disable_msn_pages_cb),
						_("Cancel"), NULL);
}

/* QuLogic: Disabled until confirmed correct. */
#if 0
static void
msn_show_blocked_text(PurplePluginAction *action)
{
	PurpleConnection *pc = (PurpleConnection *) action->context;
	MsnSession *session;
	char *title;

	session = pc->proto_data;

	title = g_strdup_printf(_("Blocked Text for %s"), session->account->username);
	if (session->blocked_text == NULL) {
		purple_notify_formatted(pc, title, title, NULL, _("No text is blocked for this account."), NULL, NULL);
	} else {
		char *blocked_text;
		blocked_text = g_strdup_printf(_("MSN servers are currently blocking the following regular expressions:<br/>%s"),
		                               session->blocked_text);
		
		purple_notify_formatted(pc, title, title, NULL, blocked_text, NULL, NULL);
		g_free(blocked_text);
	}
	g_free(title);
}
#endif

static void
msn_show_hotmail_inbox(PurplePluginAction *action)
{
	PurpleConnection *gc;
	MsnSession *session;

	gc = (PurpleConnection *) action->context;
	session = gc->proto_data;

	if (!session->passport_info.email_enabled) {
		purple_notify_error(gc, NULL,
						  _("This account does not have email enabled."), NULL);
		return;
	}

	/** apparently the correct value is 777, use 750 as a failsafe */ 
	if ((session->passport_info.mail_url == NULL)
		|| (time (NULL) - session->passport_info.mail_timestamp >= 750)) {
		MsnTransaction *trans;
		MsnCmdProc *cmdproc;

		cmdproc = session->notification->cmdproc;

		trans = msn_transaction_new(cmdproc, "URL", "%s", "INBOX");
		msn_transaction_set_data(trans, GUINT_TO_POINTER(TRUE));

		msn_cmdproc_send_trans(cmdproc, trans);

	} else
		purple_notify_uri(gc, session->passport_info.mail_url);
}

static void
show_send_to_mobile_cb(PurpleBlistNode *node, gpointer ignored)
{
	PurpleBuddy *buddy;
	PurpleConnection *gc;
	MsnSession *session;
	MsnMobileData *data;
	PurpleAccount *account;
	const char *name;

	g_return_if_fail(PURPLE_BLIST_NODE_IS_BUDDY(node));

	buddy = (PurpleBuddy *) node;
	account = purple_buddy_get_account(buddy);
	gc = purple_account_get_connection(account);
	name = purple_buddy_get_name(buddy);

	session = gc->proto_data;

	data = g_new0(MsnMobileData, 1);
	data->gc = gc;
	data->passport = name;

	purple_request_input(gc, NULL, _("Send a mobile message."), NULL,
					   NULL, TRUE, FALSE, NULL,
					   _("Page"), G_CALLBACK(send_to_mobile_cb),
					   _("Close"), G_CALLBACK(close_mobile_page_cb),
					   account, name, NULL,
					   data);
}

static gboolean
msn_offline_message(const PurpleBuddy *buddy) {
	return TRUE;
}

void
msn_send_privacy(PurpleConnection *gc)
{
	PurpleAccount *account;
	MsnSession *session;
	MsnCmdProc *cmdproc;

	account = purple_connection_get_account(gc);
	session = gc->proto_data;
	cmdproc = session->notification->cmdproc;

	if (account->perm_deny == PURPLE_PRIVACY_ALLOW_ALL ||
	    account->perm_deny == PURPLE_PRIVACY_DENY_USERS)
		msn_cmdproc_send(cmdproc, "BLP", "%s", "AL");
	else
		msn_cmdproc_send(cmdproc, "BLP", "%s", "BL");
}

static void
initiate_chat_cb(PurpleBlistNode *node, gpointer data)
{
	PurpleBuddy *buddy;
	PurpleConnection *gc;
	PurpleAccount *account;

	MsnSession *session;
	MsnSwitchBoard *swboard;

	const char *alias;

	g_return_if_fail(PURPLE_BLIST_NODE_IS_BUDDY(node));

	buddy = (PurpleBuddy *) node;
	account = purple_buddy_get_account(buddy);
	gc = purple_account_get_connection(account);

	session = gc->proto_data;

	swboard = msn_switchboard_new(session);
	msn_switchboard_request(swboard);
	msn_switchboard_request_add_user(swboard, purple_buddy_get_name(buddy));

	/* TODO: This might move somewhere else, after USR might be */
	swboard->chat_id = msn_switchboard_get_chat_id();
	swboard->conv = serv_got_joined_chat(gc, swboard->chat_id, "MSN Chat");
	swboard->flag = MSN_SB_FLAG_IM;

	/* Local alias > Display name > Username */
	if ((alias = purple_account_get_alias(account)) == NULL)
		if ((alias = purple_connection_get_display_name(gc)) == NULL)
			alias = purple_account_get_username(account);

	purple_conv_chat_add_user(PURPLE_CONV_CHAT(swboard->conv),
	                          alias, NULL, PURPLE_CBFLAGS_NONE, TRUE);
}

static void
t_msn_xfer_init(PurpleXfer *xfer)
{
	MsnSlpLink *slplink = xfer->data;
	msn_slplink_request_ft(slplink, xfer);
}

static PurpleXfer*
msn_new_xfer(PurpleConnection *gc, const char *who)
{
	MsnSession *session;
	PurpleXfer *xfer;

	session = gc->proto_data;

	xfer = purple_xfer_new(gc->account, PURPLE_XFER_SEND, who);

	g_return_val_if_fail(xfer != NULL, NULL);

	xfer->data = msn_session_get_slplink(session, who);

	purple_xfer_set_init_fnc(xfer, t_msn_xfer_init);

	return xfer;
}

static void
msn_send_file(PurpleConnection *gc, const char *who, const char *file)
{
	PurpleXfer *xfer = msn_new_xfer(gc, who);

	if (file)
		purple_xfer_request_accepted(xfer, file);
	else
		purple_xfer_request(xfer);
}

static gboolean
msn_can_receive_file(PurpleConnection *gc, const char *who)
{
	PurpleAccount *account;
	gchar *normal;
	gboolean ret;

	account = purple_connection_get_account(gc);

	normal = g_strdup(msn_normalize(account, purple_account_get_username(account)));
	ret = strcmp(normal, msn_normalize(account, who));
	g_free(normal);

	if (ret) {
		MsnSession *session = gc->proto_data;
		if (session) {
			MsnUser *user = msn_userlist_find_user(session->userlist, who);
			if (user) {
				/* Include these too: MSN_CLIENT_CAP_MSNMOBILE|MSN_CLIENT_CAP_MSNDIRECT ? */
				if ((user->clientid & MSN_CLIENT_CAP_WEBMSGR) ||
						user->networkid == MSN_NETWORK_YAHOO)
					ret = FALSE;
				else
					ret = TRUE;
			}
		} else
			ret = FALSE;
	}

	return ret;
}

/**************************************************************************
 * Protocol Plugin ops
 **************************************************************************/

static const char *
msn_list_icon(PurpleAccount *a, PurpleBuddy *b)
{
	return "msn";
}

static const char *
msn_list_emblems(PurpleBuddy *b)
{
	MsnUser *user = purple_buddy_get_protocol_data(b);

	if (user != NULL) {
		if (user->clientid & MSN_CLIENT_CAP_BOT)
			return "bot";
		if (user->clientid & MSN_CLIENT_CAP_WIN_MOBILE)
			return "mobile";
#if 0
		/* XXX: Since we don't support this, there's no point in showing it just yet */
		if (user->clientid & MSN_CLIENT_CAP_SCHANNEL)
			return "secure";
#endif
		if (user->clientid & MSN_CLIENT_CAP_WEBMSGR)
			return "external";
		if (user->networkid == MSN_NETWORK_YAHOO)
			return "yahoo";
	}

	return NULL;
}

/*
 * Set the User status text
 */
static char *
msn_status_text(PurpleBuddy *buddy)
{
	PurplePresence *presence;
	PurpleStatus *status;
	const char *msg;

	presence = purple_buddy_get_presence(buddy);
	status = purple_presence_get_active_status(presence);

	/* Official client says media takes precedence over message */
	/* I say message take precedence over media! Plus prpl-jabber agrees
	   too */
	msg = purple_status_get_attr_string(status, "message");
	if (msg && *msg)
		return g_markup_escape_text(msg, -1);

	if (purple_presence_is_status_primitive_active(presence, PURPLE_STATUS_TUNE)) {
		const char *title, *game, *office;
		char *media, *esc;
		status = purple_presence_get_status(presence, "tune");
		title = purple_status_get_attr_string(status, PURPLE_TUNE_TITLE);

		game = purple_status_get_attr_string(status, "game");
		office = purple_status_get_attr_string(status, "office");

		if (title && *title) {
			const char *artist = purple_status_get_attr_string(status, PURPLE_TUNE_ARTIST);
			const char *album = purple_status_get_attr_string(status, PURPLE_TUNE_ALBUM);
			media = purple_util_format_song_info(title, artist, album, NULL);
			return media;
		}
		else if (game && *game)
			media = g_strdup_printf("Playing %s", game);
		else if (office && *office)
			media = g_strdup_printf("Editing %s", office);
		else
			return NULL;
		esc = g_markup_escape_text(media, -1);
		g_free(media);
		return esc;
	}

	return NULL;
}

static void
msn_tooltip_text(PurpleBuddy *buddy, PurpleNotifyUserInfo *user_info, gboolean full)
{
	MsnUser *user;
	PurplePresence *presence = purple_buddy_get_presence(buddy);
	PurpleStatus *status = purple_presence_get_active_status(presence);

	user = purple_buddy_get_protocol_data(buddy);

	if (purple_presence_is_online(presence))
	{
		const char *psm, *name;
		const char *mediatype = NULL;
		char *currentmedia = NULL;
		char *tmp;

		psm = purple_status_get_attr_string(status, "message");
		if (purple_presence_is_status_primitive_active(presence, PURPLE_STATUS_TUNE)) {
			PurpleStatus *tune = purple_presence_get_status(presence, "tune");
			const char *title = purple_status_get_attr_string(tune, PURPLE_TUNE_TITLE);
			const char *game = purple_status_get_attr_string(tune, "game");
			const char *office = purple_status_get_attr_string(tune, "office");
			if (title && *title) {
				const char *artist = purple_status_get_attr_string(tune, PURPLE_TUNE_ARTIST);
				const char *album = purple_status_get_attr_string(tune, PURPLE_TUNE_ALBUM);
				mediatype = _("Now Listening");
				currentmedia = purple_util_format_song_info(title, artist, album, NULL);
			} else if (game && *game) {
				mediatype = _("Playing a game");
				currentmedia = g_strdup(game);
			} else if (office && *office) {
				mediatype = _("Working");
				currentmedia = g_strdup(office);
			}
		}

		if (!purple_status_is_available(status)) {
			name = purple_status_get_name(status);
		} else {
			name = NULL;
		}

		if (name != NULL && *name) {
			char *tmp2;

			tmp2 = g_markup_escape_text(name, -1);
			if (purple_presence_is_idle(presence)) {
				char *idle;
				char *tmp3;
				/* Never know what those translations might end up like... */
				idle = g_markup_escape_text(_("Idle"), -1);
				tmp3 = g_strdup_printf("%s/%s", tmp2, idle);
				g_free(idle);
				g_free(tmp2);
				tmp2 = tmp3;
			}

			if (psm != NULL && *psm) {
				tmp = g_markup_escape_text(psm, -1);
				purple_notify_user_info_add_pair(user_info, tmp2, tmp);
				g_free(tmp);
			} else {
				purple_notify_user_info_add_pair(user_info, _("Status"), tmp2);
			}

			g_free(tmp2);
		} else {
			if (psm != NULL && *psm) {
				tmp = g_markup_escape_text(psm, -1);
				if (purple_presence_is_idle(presence)) {
					purple_notify_user_info_add_pair(user_info, _("Idle"), tmp);
				} else {
					purple_notify_user_info_add_pair(user_info, _("Status"), tmp);
				}
				g_free(tmp);
			} else {
				if (purple_presence_is_idle(presence)) {
					purple_notify_user_info_add_pair(user_info, _("Status"),
						_("Idle"));
				} else {
					purple_notify_user_info_add_pair(user_info, _("Status"),
						purple_status_get_name(status));
				}
			}
		}

		if (currentmedia) {
			purple_notify_user_info_add_pair(user_info, mediatype, currentmedia);
			g_free(currentmedia);
		}
	}

	/* XXX: This is being shown in non-full tooltips because the
	 * XXX: blocked icon overlay isn't always accurate for MSN.
	 * XXX: This can die as soon as purple_privacy_check() knows that
	 * XXX: this prpl always honors both the allow and deny lists. */
	/* While the above comment may be strictly correct (the privacy API needs
	 * rewriteing), purple_privacy_check() is going to be more accurate at
	 * indicating whether a particular buddy is going to be able to message
	 * you, which is the important information that this is trying to convey.
	 */
	if (full && user)
	{
		const char *phone;

		purple_notify_user_info_add_pair(user_info, _("Has you"),
									   ((user->list_op & (1 << MSN_LIST_RL)) ? _("Yes") : _("No")));

		purple_notify_user_info_add_pair(user_info, _("Blocked"),
									   ((user->list_op & (1 << MSN_LIST_BL)) ? _("Yes") : _("No")));

		phone = msn_user_get_home_phone(user);
		if (phone != NULL)
			purple_notify_user_info_add_pair(user_info, _("Home Phone Number"), phone);

		phone = msn_user_get_work_phone(user);
		if (phone != NULL)
			purple_notify_user_info_add_pair(user_info, _("Work Phone Number"), phone);

		phone = msn_user_get_mobile_phone(user);
		if (phone != NULL)
			purple_notify_user_info_add_pair(user_info, _("Mobile Phone Number"), phone);
	}
}

static GList *
msn_status_types(PurpleAccount *account)
{
	PurpleStatusType *status;
	GList *types = NULL;

	status = purple_status_type_new_with_attrs(
				PURPLE_STATUS_AVAILABLE, NULL, NULL, TRUE, TRUE, FALSE,
				"message", _("Message"), purple_value_new(PURPLE_TYPE_STRING),
				NULL);
	types = g_list_append(types, status);

	status = purple_status_type_new_with_attrs(
			PURPLE_STATUS_AWAY, NULL, NULL, TRUE, TRUE, FALSE,
			"message", _("Message"), purple_value_new(PURPLE_TYPE_STRING),
			NULL);
	types = g_list_append(types, status);

	status = purple_status_type_new_with_attrs(
			PURPLE_STATUS_AWAY, "brb", _("Be Right Back"), TRUE, TRUE, FALSE,
			"message", _("Message"), purple_value_new(PURPLE_TYPE_STRING),
			NULL);
	types = g_list_append(types, status);

	status = purple_status_type_new_with_attrs(
			PURPLE_STATUS_UNAVAILABLE, "busy", _("Busy"), TRUE, TRUE, FALSE,
			"message", _("Message"), purple_value_new(PURPLE_TYPE_STRING),
			NULL);
	types = g_list_append(types, status);
	status = purple_status_type_new_with_attrs(
			PURPLE_STATUS_UNAVAILABLE, "phone", _("On the Phone"), TRUE, TRUE, FALSE,
			"message", _("Message"), purple_value_new(PURPLE_TYPE_STRING),
			NULL);
	types = g_list_append(types, status);
	status = purple_status_type_new_with_attrs(
			PURPLE_STATUS_AWAY, "lunch", _("Out to Lunch"), TRUE, TRUE, FALSE,
			"message", _("Message"), purple_value_new(PURPLE_TYPE_STRING),
			NULL);
	types = g_list_append(types, status);

	status = purple_status_type_new_full(PURPLE_STATUS_INVISIBLE,
			NULL, NULL, TRUE, TRUE, FALSE);
	types = g_list_append(types, status);

	status = purple_status_type_new_full(PURPLE_STATUS_OFFLINE,
			NULL, NULL, TRUE, TRUE, FALSE);
	types = g_list_append(types, status);

	status = purple_status_type_new_full(PURPLE_STATUS_MOBILE,
			"mobile", NULL, FALSE, FALSE, TRUE);
	types = g_list_append(types, status);

	status = purple_status_type_new_with_attrs(PURPLE_STATUS_TUNE,
			"tune", NULL, FALSE, TRUE, TRUE,
			PURPLE_TUNE_ARTIST, _("Artist"), purple_value_new(PURPLE_TYPE_STRING),
			PURPLE_TUNE_ALBUM, _("Album"), purple_value_new(PURPLE_TYPE_STRING),
			PURPLE_TUNE_TITLE, _("Title"), purple_value_new(PURPLE_TYPE_STRING),
			"game", _("Game Title"), purple_value_new(PURPLE_TYPE_STRING),
			"office", _("Office Title"), purple_value_new(PURPLE_TYPE_STRING),
			NULL);
	types = g_list_append(types, status);

	return types;
}

static GList *
msn_actions(PurplePlugin *plugin, gpointer context)
{
	GList *m = NULL;
	PurplePluginAction *act;

	act = purple_plugin_action_new(_("Set Friendly Name..."),
								 msn_show_set_friendly_name);
	m = g_list_append(m, act);
	m = g_list_append(m, NULL);

	act = purple_plugin_action_new(_("Set Home Phone Number..."),
								 msn_show_set_home_phone);
	m = g_list_append(m, act);

	act = purple_plugin_action_new(_("Set Work Phone Number..."),
			msn_show_set_work_phone);
	m = g_list_append(m, act);

	act = purple_plugin_action_new(_("Set Mobile Phone Number..."),
			msn_show_set_mobile_phone);
	m = g_list_append(m, act);
	m = g_list_append(m, NULL);

#if 0
	act = purple_plugin_action_new(_("Enable/Disable Mobile Devices..."),
			msn_show_set_mobile_support);
	m = g_list_append(m, act);
#endif

	act = purple_plugin_action_new(_("Allow/Disallow Mobile Pages..."),
			msn_show_set_mobile_pages);
	m = g_list_append(m, act);

/* QuLogic: Disabled until confirmed correct. */
#if 0
	m = g_list_append(m, NULL);
	act = purple_plugin_action_new(_("View Blocked Text..."),
			msn_show_blocked_text);
	m = g_list_append(m, act);
#endif

	m = g_list_append(m, NULL);
	act = purple_plugin_action_new(_("Open Hotmail Inbox"),
			msn_show_hotmail_inbox);
	m = g_list_append(m, act);

	return m;
}

static GList *
msn_buddy_menu(PurpleBuddy *buddy)
{
	MsnUser *user;

	GList *m = NULL;
	PurpleMenuAction *act;

	g_return_val_if_fail(buddy != NULL, NULL);

	user = purple_buddy_get_protocol_data(buddy);

	if (user != NULL)
	{
		if (user->mobile)
		{
			act = purple_menu_action_new(_("Send to Mobile"),
			                           PURPLE_CALLBACK(show_send_to_mobile_cb),
			                           NULL, NULL);
			m = g_list_append(m, act);
		}
	}

	if (g_ascii_strcasecmp(purple_buddy_get_name(buddy),
				purple_account_get_username(purple_buddy_get_account(buddy))))
	{
		act = purple_menu_action_new(_("Initiate _Chat"),
		                           PURPLE_CALLBACK(initiate_chat_cb),
		                           NULL, NULL);
		m = g_list_append(m, act);
	}

	return m;
}

static GList *
msn_blist_node_menu(PurpleBlistNode *node)
{
	if(PURPLE_BLIST_NODE_IS_BUDDY(node))
	{
		return msn_buddy_menu((PurpleBuddy *) node);
	}
	else
	{
		return NULL;
	}
}

static void
msn_login(PurpleAccount *account)
{
	PurpleConnection *gc;
	MsnSession *session;
	const char *username;
	const char *host;
	gboolean http_method = FALSE;
	int port;

	gc = purple_account_get_connection(account);

	if (!purple_ssl_is_supported())
	{
		purple_connection_error_reason(gc,
			PURPLE_CONNECTION_ERROR_NO_SSL_SUPPORT,
			_("SSL support is needed for MSN. Please install a supported "
			  "SSL library."));
		return;
	}

	http_method = purple_account_get_bool(account, "http_method", FALSE);

	if (http_method)
		host = purple_account_get_string(account, "http_method_server", MSN_HTTPCONN_SERVER);
	else
		host = purple_account_get_string(account, "server", MSN_SERVER);
	port = purple_account_get_int(account, "port", MSN_PORT);

	session = msn_session_new(account);

	gc->proto_data = session;
	gc->flags |= PURPLE_CONNECTION_HTML | PURPLE_CONNECTION_FORMATTING_WBFO | PURPLE_CONNECTION_NO_BGCOLOR |
		PURPLE_CONNECTION_NO_FONTSIZE | PURPLE_CONNECTION_NO_URLDESC | PURPLE_CONNECTION_ALLOW_CUSTOM_SMILEY;

	msn_session_set_login_step(session, MSN_LOGIN_STEP_START);

	/* Hmm, I don't like this. */
	/* XXX shx: Me neither */
	username = msn_normalize(account, purple_account_get_username(account));

	if (strcmp(username, purple_account_get_username(account)))
		purple_account_set_username(account, username);

	username = purple_account_get_string(account, "display-name", NULL);
	purple_connection_set_display_name(gc, username);

	if (!msn_session_connect(session, host, port, http_method))
		purple_connection_error_reason(gc,
			PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
			_("Unable to connect"));
}

static void
msn_close(PurpleConnection *gc)
{
	MsnSession *session;

	session = gc->proto_data;

	g_return_if_fail(session != NULL);

	msn_session_destroy(session);

	gc->proto_data = NULL;
}

static gboolean
msn_send_me_im(gpointer data)
{
	MsnIMData *imdata = data;
	serv_got_im(imdata->gc, imdata->who, imdata->msg, imdata->flags, imdata->when);
	g_free(imdata->msg);
	g_free(imdata);
	return FALSE;
}

static GString*
msn_msg_emoticon_add(GString *current, MsnEmoticon *emoticon)
{
	MsnObject *obj;
	char *strobj;

	if (emoticon == NULL)
		return current;

	obj = emoticon->obj;

	if (!obj)
		return current;

	strobj = msn_object_to_string(obj);

	if (current)
		g_string_append_printf(current, "\t%s\t%s", emoticon->smile, strobj);
	else {
		current = g_string_new("");
		g_string_printf(current, "%s\t%s", emoticon->smile, strobj);
	}

	g_free(strobj);

	return current;
}

static void
msn_send_emoticons(MsnSwitchBoard *swboard, GString *body)
{
	MsnMessage *msg;

	g_return_if_fail(body != NULL);

	msg = msn_message_new(MSN_MSG_SLP);
	msn_message_set_content_type(msg, "text/x-mms-emoticon");
	msn_message_set_flag(msg, 'N');
	msn_message_set_bin_data(msg, body->str, body->len);

	msn_switchboard_send_msg(swboard, msg, TRUE);
	msn_message_destroy(msg);
}

static void msn_emoticon_destroy(MsnEmoticon *emoticon)
{
	if (emoticon->obj)
		msn_object_destroy(emoticon->obj);
	g_free(emoticon->smile);
	g_free(emoticon);
}

static GSList* msn_msg_grab_emoticons(const char *msg, const char *username)
{
	GSList *list;
	GList *smileys;
	PurpleSmiley *smiley;
	PurpleStoredImage *img;
	char *ptr;
	MsnEmoticon *emoticon;
	int length;

	list = NULL;
	smileys = purple_smileys_get_all();
	length = strlen(msg);

	for (; smileys; smileys = g_list_delete_link(smileys, smileys)) {
		smiley = smileys->data;

		ptr = g_strstr_len(msg, length, purple_smiley_get_shortcut(smiley));

		if (!ptr)
			continue;

		img = purple_smiley_get_stored_image(smiley);

		emoticon = g_new0(MsnEmoticon, 1);
		emoticon->smile = g_strdup(purple_smiley_get_shortcut(smiley));
		emoticon->ps = smiley;
		emoticon->obj = msn_object_new_from_image(img,
				purple_imgstore_get_filename(img),
				username, MSN_OBJECT_EMOTICON);

		purple_imgstore_unref(img);
		list = g_slist_prepend(list, emoticon);
	}

	return list;
}

void
msn_send_im_message(MsnSession *session, MsnMessage *msg)
{
	MsnEmoticon *smile;
	GSList *smileys;
	GString *emoticons = NULL;
	const char *username = purple_account_get_username(session->account);
	MsnSwitchBoard *swboard = msn_session_get_swboard(session, msg->remote_user, MSN_SB_FLAG_IM);

	smileys = msn_msg_grab_emoticons(msg->body, username);
	while (smileys) {
		smile = (MsnEmoticon *)smileys->data;
		emoticons = msn_msg_emoticon_add(emoticons, smile);
		msn_emoticon_destroy(smile);
		smileys = g_slist_delete_link(smileys, smileys);
	}

	if (emoticons) {
		msn_send_emoticons(swboard, emoticons);
		g_string_free(emoticons, TRUE);
	}

	msn_switchboard_send_msg(swboard, msg, TRUE);
}

static int
msn_send_im(PurpleConnection *gc, const char *who, const char *message,
			PurpleMessageFlags flags)
{
	PurpleAccount *account;
	PurpleBuddy *buddy = purple_find_buddy(gc->account, who);
	MsnSession *session;
	MsnSwitchBoard *swboard;
	MsnMessage *msg;
	char *msgformat;
	char *msgtext;
	size_t msglen;
	const char *username;

	purple_debug_info("msn", "send IM {%s} to %s\n", message, who);
	account = purple_connection_get_account(gc);
	username = purple_account_get_username(account);

	session = gc->proto_data;
	swboard = msn_session_find_swboard(session, who);

	if (!strncmp("tel:+", who, 5)) {
		char *text = purple_markup_strip_html(message);
		send_to_mobile(gc, who, text);
		g_free(text);
		return 1;
	}

	if (buddy) {
		PurplePresence *p = purple_buddy_get_presence(buddy);
		if (purple_presence_is_status_primitive_active(p, PURPLE_STATUS_MOBILE)) {
			char *text = purple_markup_strip_html(message);
			send_to_mobile(gc, who, text);
			g_free(text);
			return 1;
		}
	}

	msn_import_html(message, &msgformat, &msgtext);
	msglen = strlen(msgtext);
	if (msglen == 0) {
		/* Stuff like <hr> will be ignored. Don't send an empty message
		   if that's all there is. */
		g_free(msgtext);
		g_free(msgformat);

		return 0;
	}

	if (msn_user_is_online(account, who) ||
		msn_user_is_yahoo(account, who) ||
		swboard != NULL) {
		/*User online or have a swboard open because it's invisible
		 * and sent us a message,then send Online Instant Message*/
 
		if (msglen + strlen(msgformat) + strlen(VERSION) > 1564)
		{
			g_free(msgformat);
			g_free(msgtext);

			return -E2BIG;
		}

		msg = msn_message_new_plain(msgtext);
		msg->remote_user = g_strdup(who);
		msn_message_set_attr(msg, "X-MMS-IM-Format", msgformat);

		g_free(msgformat);
		g_free(msgtext);

		purple_debug_info("msn", "prepare to send online Message\n");
		if (g_ascii_strcasecmp(who, username))
		{
			if (flags & PURPLE_MESSAGE_AUTO_RESP) {
				msn_message_set_flag(msg, 'U');
			}
			if (msn_user_is_yahoo(account, who)) {
				/*we send the online and offline Message to Yahoo User via UBM*/
				purple_debug_info("msn", "send to Yahoo User\n");
				uum_send_msg(session, msg);
			} else {
				purple_debug_info("msn", "send via switchboard\n");
				msn_send_im_message(session, msg);
			}
		}
		else
		{
			char *body_str, *body_enc, *pre, *post;
			const char *format;
			MsnIMData *imdata = g_new0(MsnIMData, 1);
			/*
			 * In MSN, you can't send messages to yourself, so
			 * we'll fake like we received it ;)
			 */
			body_str = msn_message_to_string(msg);
			body_enc = g_markup_escape_text(body_str, -1);
			g_free(body_str);

			format = msn_message_get_attr(msg, "X-MMS-IM-Format");
			msn_parse_format(format, &pre, &post);
			body_str = g_strdup_printf("%s%s%s", pre ? pre :  "",
									   body_enc ? body_enc : "", post ? post : "");
			g_free(body_enc);
			g_free(pre);
			g_free(post);

			serv_got_typing_stopped(gc, who);
			imdata->gc = gc;
			imdata->who = who;
			imdata->msg = body_str;
			imdata->flags = flags & ~PURPLE_MESSAGE_SEND;
			imdata->when = time(NULL);
			purple_timeout_add(0, msn_send_me_im, imdata);
		}

		msn_message_destroy(msg);
	} else {
		/*send Offline Instant Message,only to MSN Passport User*/
		char *friendname;

		purple_debug_info("msn", "prepare to send offline Message\n");

		friendname = msn_encode_mime(account->username);
		msn_oim_prep_send_msg_info(session->oim,
			purple_account_get_username(account),
			friendname, who, msgtext);
		msn_oim_send_msg(session->oim);

		g_free(msgformat);
		g_free(msgtext);
		g_free(friendname);
	}

	return 1;
}

static unsigned int
msn_send_typing(PurpleConnection *gc, const char *who, PurpleTypingState state)
{
	PurpleAccount *account;
	MsnSession *session;
	MsnSwitchBoard *swboard;
	MsnMessage *msg;

	account = purple_connection_get_account(gc);
	session = gc->proto_data;

	/*
	 * TODO: I feel like this should be "if (state != PURPLE_TYPING)"
	 *       but this is how it was before, and I don't want to break
	 *       anything. --KingAnt
	 */
	if (state == PURPLE_NOT_TYPING)
		return 0;

	if (!g_ascii_strcasecmp(who, purple_account_get_username(account)))
	{
		/* We'll just fake it, since we're sending to ourself. */
		serv_got_typing(gc, who, MSN_TYPING_RECV_TIMEOUT, PURPLE_TYPING);

		return MSN_TYPING_SEND_TIMEOUT;
	}

	swboard = msn_session_find_swboard(session, who);

	if (swboard == NULL || !msn_switchboard_can_send(swboard))
		return 0;

	swboard->flag |= MSN_SB_FLAG_IM;

	msg = msn_message_new(MSN_MSG_TYPING);
	msn_message_set_content_type(msg, "text/x-msmsgscontrol");
	msn_message_set_flag(msg, 'U');
	msn_message_set_attr(msg, "TypingUser",
						 purple_account_get_username(account));
	msn_message_set_bin_data(msg, "\r\n", 2);

	msn_switchboard_send_msg(swboard, msg, FALSE);

	msn_message_destroy(msg);

	return MSN_TYPING_SEND_TIMEOUT;
}

static void
msn_set_status(PurpleAccount *account, PurpleStatus *status)
{
	PurpleConnection *gc;
	MsnSession *session;

	gc = purple_account_get_connection(account);

	if (gc != NULL)
	{
		session = gc->proto_data;
		msn_change_status(session);
	}
}

static void
msn_set_idle(PurpleConnection *gc, int idle)
{
	MsnSession *session;

	session = gc->proto_data;

	msn_change_status(session);
}

/*
 * Actually adds a buddy once we have the response from FQY
 */
static void
add_pending_buddy(MsnSession *session,
                  const char *who,
                  MsnNetwork network,
                  MsnUser *user)
{
	char *group;

	g_return_if_fail(user != NULL);

	group = msn_user_remove_pending_group(user);

	if (network != MSN_NETWORK_UNKNOWN) {
		MsnUserList *userlist = session->userlist;
		MsnUser *user2 = msn_userlist_find_user(userlist, who);
		if (user2 != NULL) {
			/* User already in userlist, so just update it. */
			msn_user_destroy(user);
			user = user2;
		} else {
			msn_userlist_add_user(userlist, user);
		}

		msn_user_set_network(user, network);
		msn_userlist_add_buddy(userlist, who, group);
	}
	else
	{
		PurpleBuddy * buddy = purple_find_buddy(session->account, who);
		gchar *buf;
		buf = g_strdup_printf(_("Unable to add the buddy %s because the username is invalid.  Usernames must be valid email addresses."), who);
		if (!purple_conv_present_error(who, session->account, buf))
			purple_notify_error(purple_account_get_connection(session->account), NULL, _("Unable to Add"), buf);
		g_free(buf);

		/* Remove from local list */
		purple_blist_remove_buddy(buddy);
		msn_user_destroy(user);
	}
	g_free(group);
}

static void
finish_auth_request(MsnAddReqData *data, char *msg)
{
	PurpleConnection *pc;
	PurpleBuddy *buddy;
	PurpleGroup *group;
	PurpleAccount *account;
	MsnSession *session;
	MsnUserList *userlist;
	const char *who, *gname;
	MsnUser *user;

	pc = data->pc;
	buddy = data->buddy;
	group = data->group;
	g_free(data);

	account = purple_connection_get_account(pc);
	session = pc->proto_data;
	userlist = session->userlist;

	who = msn_normalize(account, purple_buddy_get_name(buddy));
	gname = group ? purple_group_get_name(group) : NULL;
	purple_debug_info("msn", "Add user:%s to group:%s\n", who, gname ? gname : "(null)");
	if (!session->logged_in)
	{
		purple_debug_error("msn", "msn_add_buddy called before connected\n");

		return;
	}

	/* XXX - Would group ever be NULL here?  I don't think so...
	 * shx: Yes it should; MSN handles non-grouped buddies, and this is only
	 * internal. */
	user = msn_userlist_find_user(userlist, who);
	if ((user != NULL) && (user->networkid != MSN_NETWORK_UNKNOWN)) {
		/* We already know this buddy and their network. This function knows
		   what to do with users already in the list and stuff... */
		msn_user_set_invite_message(user, msg);
		msn_userlist_add_buddy(userlist, who, gname);
	} else {
		char **tokens;
		char *fqy;
		/* We need to check the network for this buddy first */
		user = msn_user_new(userlist, who, NULL);
		msn_user_set_invite_message(user, msg);
		msn_user_set_pending_group(user, gname);
		msn_user_set_network(user, MSN_NETWORK_UNKNOWN);
		tokens = g_strsplit(who, "@", 2);
		fqy = g_strdup_printf("<ml><d n=\"%s\"><c n=\"%s\"/></d></ml>",
		                      tokens[1],
		                      tokens[0]);
		msn_notification_send_fqy(session, fqy, strlen(fqy),
		                          (MsnFqyCb)add_pending_buddy, user);
		g_free(fqy);
		g_strfreev(tokens);
	}
}

static void
cancel_auth_request(MsnAddReqData *data, char *msg)
{
	/* Remove from local list */
	purple_blist_remove_buddy(data->buddy);
	
	g_free(data);
}

static void
msn_add_buddy(PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group)
{
	const char *bname;
	MsnAddReqData *data;
	MsnSession *session;
	MsnUser *user;

	bname = purple_buddy_get_name(buddy);

	if (!msn_email_is_valid(bname)) {
		gchar *buf;
		buf = g_strdup_printf(_("Unable to add the buddy %s because the username is invalid.  Usernames must be valid email addresses."), bname);
		if (!purple_conv_present_error(bname, purple_connection_get_account(gc), buf))
			purple_notify_error(gc, NULL, _("Unable to Add"), buf);
		g_free(buf);

		/* Remove from local list */
		purple_blist_remove_buddy(buddy);

		return;
	}

	data = g_new0(MsnAddReqData, 1);
	data->pc = gc;
	data->buddy = buddy;
	data->group = group;

	session = purple_connection_get_protocol_data(gc);
	user = msn_userlist_find_user(session->userlist, bname);
	if (user && user->authorized) {
		finish_auth_request(data, NULL);
	} else {
		purple_request_input(gc, NULL, _("Authorization Request Message:"),
		                     NULL, _("Please authorize me!"), TRUE, FALSE, NULL,
		                     _("_OK"), G_CALLBACK(finish_auth_request),
		                     _("_Cancel"), G_CALLBACK(cancel_auth_request),
		                     purple_connection_get_account(gc), bname, NULL,
		                     data);
	}
}

static void
msn_rem_buddy(PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group)
{
	MsnSession *session;
	MsnUserList *userlist;

	session = gc->proto_data;
	userlist = session->userlist;

	if (!session->logged_in)
		return;

	/* XXX - Does buddy->name need to be msn_normalize'd here?  --KingAnt */
	msn_userlist_rem_buddy(userlist, purple_buddy_get_name(buddy));
}

static void
msn_add_permit(PurpleConnection *gc, const char *who)
{
	MsnSession *session;
	MsnUserList *userlist;
	MsnUser *user;

	session = gc->proto_data;
	userlist = session->userlist;
	user = msn_userlist_find_user(userlist, who);

	if (!session->logged_in)
		return;

	if (user != NULL && user->list_op & MSN_LIST_BL_OP) {
		msn_userlist_rem_buddy_from_list(userlist, who, MSN_LIST_BL);

		/* delete contact from Block list and add it to Allow in the callback */
		msn_del_contact_from_list(session, NULL, who, MSN_LIST_BL);
	} else {
		/* just add the contact to Allow list */
		msn_add_contact_to_list(session, NULL, who, MSN_LIST_AL);
	}


	msn_userlist_add_buddy_to_list(userlist, who, MSN_LIST_AL);
}

static void
msn_add_deny(PurpleConnection *gc, const char *who)
{
	MsnSession *session;
	MsnUserList *userlist;
	MsnUser *user;

	session = gc->proto_data;
	userlist = session->userlist;
	user = msn_userlist_find_user(userlist, who);

	if (!session->logged_in)
		return;

	if (user != NULL && user->list_op & MSN_LIST_AL_OP) {
		msn_userlist_rem_buddy_from_list(userlist, who, MSN_LIST_AL);

		/* delete contact from Allow list and add it to Block in the callback */
		msn_del_contact_from_list(session, NULL, who, MSN_LIST_AL);
	} else {
		/* just add the contact to Block list */
		msn_add_contact_to_list(session, NULL, who, MSN_LIST_BL);
	}

	msn_userlist_add_buddy_to_list(userlist, who, MSN_LIST_BL);
}

static void
msn_rem_permit(PurpleConnection *gc, const char *who)
{
	MsnSession *session;
	MsnUserList *userlist;
	MsnUser *user;

	session = gc->proto_data;
	userlist = session->userlist;

	if (!session->logged_in)
		return;

	user = msn_userlist_find_user(userlist, who);

	msn_userlist_rem_buddy_from_list(userlist, who, MSN_LIST_AL);

	msn_del_contact_from_list(session, NULL, who, MSN_LIST_AL);

	if (user != NULL && user->list_op & MSN_LIST_RL_OP)
		msn_userlist_add_buddy_to_list(userlist, who, MSN_LIST_BL);
}

static void
msn_rem_deny(PurpleConnection *gc, const char *who)
{
	MsnSession *session;
	MsnUserList *userlist;
	MsnUser *user;

	session = gc->proto_data;
	userlist = session->userlist;

	if (!session->logged_in)
		return;

	user = msn_userlist_find_user(userlist, who);

	msn_userlist_rem_buddy_from_list(userlist, who, MSN_LIST_BL);

	msn_del_contact_from_list(session, NULL, who, MSN_LIST_BL);

	if (user != NULL && user->list_op & MSN_LIST_RL_OP)
		msn_userlist_add_buddy_to_list(userlist, who, MSN_LIST_AL);
}

static void
msn_set_permit_deny(PurpleConnection *gc)
{
	msn_send_privacy(gc);
}

static void
msn_chat_invite(PurpleConnection *gc, int id, const char *msg,
				const char *who)
{
	MsnSession *session;
	MsnSwitchBoard *swboard;

	session = gc->proto_data;

	swboard = msn_session_find_swboard_with_id(session, id);

	if (swboard == NULL)
	{
		/* if we have no switchboard, everyone else left the chat already */
		swboard = msn_switchboard_new(session);
		msn_switchboard_request(swboard);
		swboard->chat_id = id;
		swboard->conv = purple_find_chat(gc, id);
	}

	swboard->flag |= MSN_SB_FLAG_IM;

	msn_switchboard_request_add_user(swboard, who);
}

static void
msn_chat_leave(PurpleConnection *gc, int id)
{
	MsnSession *session;
	MsnSwitchBoard *swboard;
	PurpleConversation *conv;

	session = gc->proto_data;

	swboard = msn_session_find_swboard_with_id(session, id);

	/* if swboard is NULL we were the only person left anyway */
	if (swboard == NULL)
		return;

	conv = swboard->conv;

	msn_switchboard_release(swboard, MSN_SB_FLAG_IM);

	/* If other switchboards managed to associate themselves with this
	 * conv, make sure they know it's gone! */
	if (conv != NULL)
	{
		while ((swboard = msn_session_find_swboard_with_conv(session, conv)) != NULL)
			swboard->conv = NULL;
	}
}

static int
msn_chat_send(PurpleConnection *gc, int id, const char *message, PurpleMessageFlags flags)
{
	PurpleAccount *account;
	MsnSession *session;
	const char *username;
	MsnSwitchBoard *swboard;
	MsnMessage *msg;
	char *msgformat;
	char *msgtext;
	size_t msglen;
	MsnEmoticon *smile;
	GSList *smileys;
	GString *emoticons = NULL;

	account = purple_connection_get_account(gc);
	session = gc->proto_data;
	username = purple_account_get_username(account);
	swboard = msn_session_find_swboard_with_id(session, id);

	if (swboard == NULL)
		return -EINVAL;

	if (!swboard->ready)
		return 0;

	swboard->flag |= MSN_SB_FLAG_IM;

	msn_import_html(message, &msgformat, &msgtext);
	msglen = strlen(msgtext);

	if ((msglen == 0) || (msglen + strlen(msgformat) + strlen(VERSION) > 1564))
	{
		g_free(msgformat);
		g_free(msgtext);

		return -E2BIG;
	}

	msg = msn_message_new_plain(msgtext);
	msn_message_set_attr(msg, "X-MMS-IM-Format", msgformat);

	smileys = msn_msg_grab_emoticons(msg->body, username);
	while (smileys) {
		smile = (MsnEmoticon *)smileys->data;
		emoticons = msn_msg_emoticon_add(emoticons, smile);
		if (purple_conv_custom_smiley_add(swboard->conv, smile->smile,
		                                  "sha1", purple_smiley_get_checksum(smile->ps),
		                                  FALSE)) {
			gconstpointer data;
			size_t len;
			data = purple_smiley_get_data(smile->ps, &len);
			purple_conv_custom_smiley_write(swboard->conv, smile->smile, data, len);
			purple_conv_custom_smiley_close(swboard->conv, smile->smile);
		}
		msn_emoticon_destroy(smile);
		smileys = g_slist_delete_link(smileys, smileys);
	}

	if (emoticons) {
		msn_send_emoticons(swboard, emoticons);
		g_string_free(emoticons, TRUE);
	}

	msn_switchboard_send_msg(swboard, msg, FALSE);
	msn_message_destroy(msg);

	g_free(msgformat);
	g_free(msgtext);

	serv_got_chat_in(gc, id, purple_account_get_username(account), flags,
					 message, time(NULL));

	return 0;
}

static void
msn_keepalive(PurpleConnection *gc)
{
	MsnSession *session;

	session = gc->proto_data;

	if (!session->http_method)
	{
		MsnCmdProc *cmdproc;

		cmdproc = session->notification->cmdproc;

		msn_cmdproc_send_quick(cmdproc, "PNG", NULL, NULL);
	}
}

static void msn_alias_buddy(PurpleConnection *pc, const char *name, const char *alias)
{
	MsnSession *session;

	session = pc->proto_data;

	msn_update_contact(session, name, MSN_UPDATE_ALIAS, alias);
}

static void
msn_group_buddy(PurpleConnection *gc, const char *who,
				const char *old_group_name, const char *new_group_name)
{
	MsnSession *session;
	MsnUserList *userlist;

	session = gc->proto_data;
	userlist = session->userlist;

	msn_userlist_move_buddy(userlist, who, old_group_name, new_group_name);
}

static void
msn_rename_group(PurpleConnection *gc, const char *old_name,
				 PurpleGroup *group, GList *moved_buddies)
{
	MsnSession *session;
	const char *gname;

	session = gc->proto_data;

	g_return_if_fail(session != NULL);
	g_return_if_fail(session->userlist != NULL);

	gname = purple_group_get_name(group);
	if (msn_userlist_find_group_with_name(session->userlist, old_name) != NULL)
	{
		msn_contact_rename_group(session, old_name, gname);
	}
	else
	{
		/* not found */
		msn_add_group(session, NULL, gname);
	}
}

static void
msn_convo_closed(PurpleConnection *gc, const char *who)
{
	MsnSession *session;
	MsnSwitchBoard *swboard;
	PurpleConversation *conv;

	session = gc->proto_data;

	swboard = msn_session_find_swboard(session, who);

	/*
	 * Don't perform an assertion here. If swboard is NULL, then the
	 * switchboard was either closed by the other party, or the person
	 * is talking to himself.
	 */
	if (swboard == NULL)
		return;

	conv = swboard->conv;

	/* If we release the switchboard here, it may still have messages
	   pending ACK which would result in incorrect unsent message errors.
	   Just let it timeout... This is *so* going to screw with people who
	   use dumb clients that report "User has closed the conversation window" */
	/* msn_switchboard_release(swboard, MSN_SB_FLAG_IM); */
	swboard->conv = NULL;

	/* If other switchboards managed to associate themselves with this
	 * conv, make sure they know it's gone! */
	if (conv != NULL)
	{
		while ((swboard = msn_session_find_swboard_with_conv(session, conv)) != NULL)
			swboard->conv = NULL;
	}
}

static void
msn_set_buddy_icon(PurpleConnection *gc, PurpleStoredImage *img)
{
	MsnSession *session;
	MsnUser *user;

	session = gc->proto_data;
	user = session->user;

	msn_user_set_buddy_icon(user, img);

	msn_change_status(session);
}

static void
msn_remove_group(PurpleConnection *gc, PurpleGroup *group)
{
	MsnSession *session;
	MsnCmdProc *cmdproc;
	const char *gname;

	session = gc->proto_data;
	cmdproc = session->notification->cmdproc;
	gname = purple_group_get_name(group);

	purple_debug_info("msn", "Remove group %s\n", gname);
	/*we can't delete the default group*/
	if(!strcmp(gname, MSN_INDIVIDUALS_GROUP_NAME)||
		!strcmp(gname, MSN_NON_IM_GROUP_NAME))
	{
		purple_debug_info("msn", "This group can't be removed, returning.\n");
		return ;
	}

	msn_del_group(session, gname);
}

/**
 * Extract info text from info_data and add it to user_info
 */
static gboolean
msn_tooltip_extract_info_text(PurpleNotifyUserInfo *user_info, MsnGetInfoData *info_data)
{
	PurpleBuddy *b;

	b = purple_find_buddy(purple_connection_get_account(info_data->gc),
						info_data->name);

	if (b)
	{
		char *tmp;
		const char *alias;

		alias = purple_buddy_get_local_buddy_alias(b);
		if (alias && alias[0])
		{
			char *aliastext = g_markup_escape_text(alias, -1);
			purple_notify_user_info_add_pair(user_info, _("Alias"), aliastext);
			g_free(aliastext);
		}

		if ((alias = purple_buddy_get_server_alias(b)) != NULL)
		{
			char *nicktext = g_markup_escape_text(alias, -1);
			tmp = g_strdup_printf("<font sml=\"msn\">%s</font>", nicktext);
			purple_notify_user_info_add_pair(user_info, _("Nickname"), tmp);
			g_free(tmp);
			g_free(nicktext);
		}

		/* Add the tooltip information */
		msn_tooltip_text(b, user_info, TRUE);

		return TRUE;
	}

	return FALSE;
}

#if PHOTO_SUPPORT

static char *
msn_get_photo_url(const char *url_text)
{
	char *p, *q;

	if ((p = strstr(url_text, PHOTO_URL)) != NULL)
	{
		p += strlen(PHOTO_URL);
	}
	if (p && (strncmp(p, "http://", strlen("http://")) == 0) && ((q = strchr(p, '"')) != NULL))
			return g_strndup(p, q - p);

	return NULL;
}

static void msn_got_photo(PurpleUtilFetchUrlData *url_data, gpointer data,
		const gchar *url_text, gsize len, const gchar *error_message);

#endif

#if 0
static char *msn_info_date_reformat(const char *field, size_t len)
{
	char *tmp = g_strndup(field, len);
	time_t t = purple_str_to_time(tmp, FALSE, NULL, NULL, NULL);

	g_free(tmp);
	return g_strdup(purple_date_format_short(localtime(&t)));
}
#endif

#define MSN_GOT_INFO_GET_FIELD(a, b) \
	found = purple_markup_extract_info_field(stripped, stripped_len, user_info, \
			"\n" a ":", 0, "\n", 0, "Undisclosed", b, 0, NULL, NULL); \
	if (found) \
		sect_info = TRUE;

#define MSN_GOT_INFO_GET_FIELD_NO_SEARCH(a, b) \
	found = purple_markup_extract_info_field(stripped, stripped_len, user_info, \
			"\n" a ":", 0, "\n", 0, "Undisclosed", b, 0, NULL, msn_info_strip_search_link); \
	if (found) \
		sect_info = TRUE;

static char *
msn_info_strip_search_link(const char *field, size_t len)
{
	const char *c;
	if ((c = strstr(field, " (http://")) == NULL)
		return g_strndup(field, len);
	return g_strndup(field, c - field);
}

static void
msn_got_info(PurpleUtilFetchUrlData *url_data, gpointer data,
		const gchar *url_text, size_t len, const gchar *error_message)
{
	MsnGetInfoData *info_data = (MsnGetInfoData *)data;
	PurpleNotifyUserInfo *user_info;
	char *stripped, *p, *q, *tmp;
	char *user_url = NULL;
	gboolean found;
	gboolean has_tooltip_text = FALSE;
	gboolean has_info = FALSE;
	gboolean sect_info = FALSE;
	gboolean has_contact_info = FALSE;
	char *url_buffer;
	int stripped_len;
#if PHOTO_SUPPORT
	char *photo_url_text = NULL;
	MsnGetInfoStepTwoData *info2_data = NULL;
#endif

	purple_debug_info("msn", "In msn_got_info,url_text:{%s}\n",url_text);

	/* Make sure the connection is still valid */
	/* TODO: Instead of this, we should be canceling this when we disconnect */
	if (g_list_find(purple_connections_get_all(), info_data->gc) == NULL)
	{
		purple_debug_warning("msn", "invalid connection. ignoring buddy info.\n");
		g_free(info_data->name);
		g_free(info_data);
		return;
	}

	user_info = purple_notify_user_info_new();
	has_tooltip_text = msn_tooltip_extract_info_text(user_info, info_data);

	if (error_message != NULL || url_text == NULL || strcmp(url_text, "") == 0)
	{
		purple_notify_user_info_add_pair(user_info,
				_("Error retrieving profile"), NULL);

		purple_notify_userinfo(info_data->gc, info_data->name, user_info, NULL, NULL);
		purple_notify_user_info_destroy(user_info);

		g_free(info_data->name);
		g_free(info_data);
		return;
	}

	url_buffer = g_strdup(url_text);

	/* If they have a homepage link, MSN masks it such that we need to
	 * fetch the url out before purple_markup_strip_html() nukes it */
	/* I don't think this works with the new spaces profiles - Stu 3/2/06 */
	if ((p = strstr(url_text,
			"Take a look at my </font><A class=viewDesc title=\"")) != NULL)
	{
		p += 50;

		if ((q = strchr(p, '"')) != NULL)
			user_url = g_strndup(p, q - p);
	}

	/*
	 * purple_markup_strip_html() doesn't strip out character entities like &nbsp;
	 * and &#183;
	 */
	while ((p = strstr(url_buffer, "&nbsp;")) != NULL)
	{
		*p = ' '; /* Turn &nbsp;'s into ordinary blanks */
		p += 1;
		memmove(p, p + 5, strlen(p + 5));
		url_buffer[strlen(url_buffer) - 5] = '\0';
	}

	while ((p = strstr(url_buffer, "&#183;")) != NULL)
	{
		memmove(p, p + 6, strlen(p + 6));
		url_buffer[strlen(url_buffer) - 6] = '\0';
	}

	/* Nuke the nasty \r's that just get in the way */
	purple_str_strip_char(url_buffer, '\r');

	/* MSN always puts in &#39; for apostrophes...replace them */
	while ((p = strstr(url_buffer, "&#39;")) != NULL)
	{
		*p = '\'';
		memmove(p + 1, p + 5, strlen(p + 5));
		url_buffer[strlen(url_buffer) - 4] = '\0';
	}

	/* Nuke the html, it's easier than trying to parse the horrid stuff */
	stripped = purple_markup_strip_html(url_buffer);
	stripped_len = strlen(stripped);

	purple_debug_misc("msn", "stripped = %p\n", stripped);
	purple_debug_misc("msn", "url_buffer = %p\n", url_buffer);

	/* General section header */
	if (has_tooltip_text)
		purple_notify_user_info_add_section_break(user_info);

	purple_notify_user_info_add_section_header(user_info, _("General"));

	/* Extract their Name and put it in */
	MSN_GOT_INFO_GET_FIELD("Name", _("Name"));

	/* General */
	MSN_GOT_INFO_GET_FIELD("Nickname", _("Nickname"));
	MSN_GOT_INFO_GET_FIELD_NO_SEARCH("Age", _("Age"));
	MSN_GOT_INFO_GET_FIELD_NO_SEARCH("Gender", _("Gender"));
	MSN_GOT_INFO_GET_FIELD_NO_SEARCH("Occupation", _("Occupation"));
	MSN_GOT_INFO_GET_FIELD_NO_SEARCH("Location", _("Location"));

	/* Extract their Interests and put it in */
	found = purple_markup_extract_info_field(stripped, stripped_len, user_info,
			"\nInterests\t", 0, " (/default.aspx?page=searchresults", 0,
			"Undisclosed", _("Hobbies and Interests") /* _("Interests") */,
			0, NULL, NULL);

	if (found)
		sect_info = TRUE;

	MSN_GOT_INFO_GET_FIELD("More about me", _("A Little About Me"));

	if (sect_info)
	{
		has_info = TRUE;
		sect_info = FALSE;
	}
    else
    {
		/* Remove the section header */
		purple_notify_user_info_remove_last_item(user_info);
		if (has_tooltip_text)
			purple_notify_user_info_remove_last_item(user_info);
	}

	/* Social */
	purple_notify_user_info_add_section_break(user_info);
	purple_notify_user_info_add_section_header(user_info, _("Social"));

	MSN_GOT_INFO_GET_FIELD_NO_SEARCH("Marital status", _("Marital Status"));
	MSN_GOT_INFO_GET_FIELD_NO_SEARCH("Interested in", _("Interests"));
	MSN_GOT_INFO_GET_FIELD_NO_SEARCH("Pets", _("Pets"));
	MSN_GOT_INFO_GET_FIELD_NO_SEARCH("Hometown", _("Hometown"));
	MSN_GOT_INFO_GET_FIELD("Places lived", _("Places Lived"));
	MSN_GOT_INFO_GET_FIELD_NO_SEARCH("Fashion", _("Fashion"));
	MSN_GOT_INFO_GET_FIELD_NO_SEARCH("Humor", _("Humor"));
	MSN_GOT_INFO_GET_FIELD_NO_SEARCH("Music", _("Music"));
	MSN_GOT_INFO_GET_FIELD_NO_SEARCH("Favorite quote", _("Favorite Quote"));

	if (sect_info)
	{
		has_info = TRUE;
		sect_info = FALSE;
	}
    else
    {
		/* Remove the section header */
		purple_notify_user_info_remove_last_item(user_info);
		purple_notify_user_info_remove_last_item(user_info);
	}

	/* Contact Info */
	/* Personal */
	purple_notify_user_info_add_section_break(user_info);
	purple_notify_user_info_add_section_header(user_info, _("Contact Info"));
	purple_notify_user_info_add_section_header(user_info, _("Personal"));

	MSN_GOT_INFO_GET_FIELD("Name", _("Name"));
	MSN_GOT_INFO_GET_FIELD("Significant other", _("Significant Other"));
	MSN_GOT_INFO_GET_FIELD("Home phone", _("Home Phone"));
	MSN_GOT_INFO_GET_FIELD("Home phone 2", _("Home Phone 2"));
	MSN_GOT_INFO_GET_FIELD("Home address", _("Home Address"));
	MSN_GOT_INFO_GET_FIELD("Personal Mobile", _("Personal Mobile"));
	MSN_GOT_INFO_GET_FIELD("Home fax", _("Home Fax"));
	MSN_GOT_INFO_GET_FIELD("Personal email", _("Personal Email"));
	MSN_GOT_INFO_GET_FIELD("Personal IM", _("Personal IM"));
	MSN_GOT_INFO_GET_FIELD("Birthday", _("Birthday"));
	MSN_GOT_INFO_GET_FIELD("Anniversary", _("Anniversary"));
	MSN_GOT_INFO_GET_FIELD("Notes", _("Notes"));

	if (sect_info)
	{
		has_info = TRUE;
		sect_info = FALSE;
		has_contact_info = TRUE;
	}
    else
    {
		/* Remove the section header */
		purple_notify_user_info_remove_last_item(user_info);
	}

	/* Business */
	purple_notify_user_info_add_section_header(user_info, _("Work"));
	MSN_GOT_INFO_GET_FIELD("Name", _("Name"));
	MSN_GOT_INFO_GET_FIELD("Job title", _("Job Title"));
	MSN_GOT_INFO_GET_FIELD("Company", _("Company"));
	MSN_GOT_INFO_GET_FIELD("Department", _("Department"));
	MSN_GOT_INFO_GET_FIELD("Profession", _("Profession"));
	MSN_GOT_INFO_GET_FIELD("Work phone 1", _("Work Phone"));
	MSN_GOT_INFO_GET_FIELD("Work phone 2", _("Work Phone 2"));
	MSN_GOT_INFO_GET_FIELD("Work address", _("Work Address"));
	MSN_GOT_INFO_GET_FIELD("Work mobile", _("Work Mobile"));
	MSN_GOT_INFO_GET_FIELD("Work pager", _("Work Pager"));
	MSN_GOT_INFO_GET_FIELD("Work fax", _("Work Fax"));
	MSN_GOT_INFO_GET_FIELD("Work email", _("Work Email"));
	MSN_GOT_INFO_GET_FIELD("Work IM", _("Work IM"));
	MSN_GOT_INFO_GET_FIELD("Start date", _("Start Date"));
	MSN_GOT_INFO_GET_FIELD("Notes", _("Notes"));

	if (sect_info)
	{
		has_info = TRUE;
		sect_info = FALSE;
		has_contact_info = TRUE;
	}
    else
    {
		/* Remove the section header */
		purple_notify_user_info_remove_last_item(user_info);
	}

	if (!has_contact_info)
	{
		/* Remove the Contact Info section header */
		purple_notify_user_info_remove_last_item(user_info);
	}

#if 0 /* these probably don't show up any more */
	/*
	 * The fields, 'A Little About Me', 'Favorite Things', 'Hobbies
	 * and Interests', 'Favorite Quote', and 'My Homepage' may or may
	 * not appear, in any combination. However, they do appear in
	 * certain order, so we can successively search to pin down the
	 * distinct values.
	 */

	/* Check if they have A Little About Me */
	found = purple_markup_extract_info_field(stripped, stripped_len, s,
			" A Little About Me \n\n", 0, "Favorite Things", '\n', NULL,
			_("A Little About Me"), 0, NULL, NULL);

	if (!found)
	{
		found = purple_markup_extract_info_field(stripped, stripped_len, s,
				" A Little About Me \n\n", 0, "Hobbies and Interests", '\n',
				NULL, _("A Little About Me"), 0, NULL, NULL);
	}

	if (!found)
	{
		found = purple_markup_extract_info_field(stripped, stripped_len, s,
				" A Little About Me \n\n", 0, "Favorite Quote", '\n', NULL,
				_("A Little About Me"), 0, NULL, NULL);
	}

	if (!found)
	{
		found = purple_markup_extract_info_field(stripped, stripped_len, s,
				" A Little About Me \n\n", 0, "My Homepage \n\nTake a look",
				'\n',
				NULL, _("A Little About Me"), 0, NULL, NULL);
	}

	if (!found)
	{
		purple_markup_extract_info_field(stripped, stripped_len, s,
				" A Little About Me \n\n", 0, "last updated", '\n', NULL,
				_("A Little About Me"), 0, NULL, NULL);
	}

	if (found)
		has_info = TRUE;

	/* Check if they have Favorite Things */
	found = purple_markup_extract_info_field(stripped, stripped_len, s,
			" Favorite Things \n\n", 0, "Hobbies and Interests", '\n', NULL,
			_("Favorite Things"), 0, NULL, NULL);

	if (!found)
	{
		found = purple_markup_extract_info_field(stripped, stripped_len, s,
				" Favorite Things \n\n", 0, "Favorite Quote", '\n', NULL,
				_("Favorite Things"), 0, NULL, NULL);
	}

	if (!found)
	{
		found = purple_markup_extract_info_field(stripped, stripped_len, s,
				" Favorite Things \n\n", 0, "My Homepage \n\nTake a look", '\n',
				NULL, _("Favorite Things"), 0, NULL, NULL);
	}

	if (!found)
	{
		purple_markup_extract_info_field(stripped, stripped_len, s,
				" Favorite Things \n\n", 0, "last updated", '\n', NULL,
				_("Favorite Things"), 0, NULL, NULL);
	}

	if (found)
		has_info = TRUE;

	/* Check if they have Hobbies and Interests */
	found = purple_markup_extract_info_field(stripped, stripped_len, s,
			" Hobbies and Interests \n\n", 0, "Favorite Quote", '\n', NULL,
			_("Hobbies and Interests"), 0, NULL, NULL);

	if (!found)
	{
		found = purple_markup_extract_info_field(stripped, stripped_len, s,
				" Hobbies and Interests \n\n", 0, "My Homepage \n\nTake a look",
				'\n', NULL, _("Hobbies and Interests"), 0, NULL, NULL);
	}

	if (!found)
	{
		purple_markup_extract_info_field(stripped, stripped_len, s,
				" Hobbies and Interests \n\n", 0, "last updated", '\n', NULL,
				_("Hobbies and Interests"), 0, NULL, NULL);
	}

	if (found)
		has_info = TRUE;

	/* Check if they have Favorite Quote */
	found = purple_markup_extract_info_field(stripped, stripped_len, s,
			"Favorite Quote \n\n", 0, "My Homepage \n\nTake a look", '\n', NULL,
			_("Favorite Quote"), 0, NULL, NULL);

	if (!found)
	{
		purple_markup_extract_info_field(stripped, stripped_len, s,
				"Favorite Quote \n\n", 0, "last updated", '\n', NULL,
				_("Favorite Quote"), 0, NULL, NULL);
	}

	if (found)
		has_info = TRUE;

	/* Extract the last updated date and put it in */
	found = purple_markup_extract_info_field(stripped, stripped_len, s,
			" last updated:", 1, "\n", 0, NULL, _("Last Updated"), 0,
			NULL, msn_info_date_reformat);

	if (found)
		has_info = TRUE;
#endif

	/* If we were able to fetch a homepage url earlier, stick it in there */
	if (user_url != NULL)
	{
		tmp = g_strdup_printf("<a href=\"%s\">%s</a>", user_url, user_url);
		purple_notify_user_info_add_pair(user_info, _("Homepage"), tmp);
		g_free(tmp);
		g_free(user_url);

		has_info = TRUE;
	}

	if (!has_info)
	{
		/* MSN doesn't actually distinguish between "unknown member" and
		 * a known member with an empty profile. Try to explain this fact.
		 * Note that if we have a nonempty tooltip_text, we know the user
		 * exists.
		 */
		/* This doesn't work with the new spaces profiles - Stu 3/2/06
		char *p = strstr(url_buffer, "Unknown Member </TITLE>");
		 * This might not work for long either ... */
		/* Nope, it failed some time before 5/2/07 :(
		char *p = strstr(url_buffer, "form id=\"SpacesSearch\" name=\"SpacesSearch\"");
		 * Let's see how long this one holds out for ... */
		char *p = strstr(url_buffer, "<form id=\"profile_form\" name=\"profile_form\" action=\"http&#58;&#47;&#47;spaces.live.com&#47;profile.aspx&#63;cid&#61;0\"");
		PurpleBuddy *b = purple_find_buddy
				(purple_connection_get_account(info_data->gc), info_data->name);
		purple_notify_user_info_add_pair(user_info,
				_("Error retrieving profile"), NULL);
		purple_notify_user_info_add_pair(user_info, NULL,
				((p && b) ? _("The user has not created a public profile.") :
					(p ? _("MSN reported not being able to find the user's profile. "
							"This either means that the user does not exist, "
							"or that the user exists "
							"but has not created a public profile.") :
						_("Could not find "	/* This should never happen */
							"any information in the user's profile. "
							"The user most likely does not exist."))));
	}

	/* put a link to the actual profile URL */
	purple_notify_user_info_add_section_break(user_info);
	tmp = g_strdup_printf("<a href=\"%s%s\">%s</a>",
			PROFILE_URL, info_data->name, _("View web profile"));
	purple_notify_user_info_add_pair(user_info, NULL, tmp);
	g_free(tmp);

#if PHOTO_SUPPORT
	/* Find the URL to the photo; must be before the marshalling [Bug 994207] */
	photo_url_text = msn_get_photo_url(url_text);
	purple_debug_info("msn", "photo url:{%s}\n", photo_url_text ? photo_url_text : "(null)");

	/* Marshall the existing state */
	info2_data = g_new0(MsnGetInfoStepTwoData, 1);
	info2_data->info_data = info_data;
	info2_data->stripped = stripped;
	info2_data->url_buffer = url_buffer;
	info2_data->user_info = user_info;
	info2_data->photo_url_text = photo_url_text;

	/* Try to put the photo in there too, if there's one */
	if (photo_url_text)
	{
		purple_util_fetch_url_len(photo_url_text, FALSE, NULL, FALSE, MAX_HTTP_BUDDYICON_BYTES, msn_got_photo,
					   info2_data);
	}
	else
	{
		/* Emulate a callback */
		/* TODO: Huh? */
		msn_got_photo(NULL, info2_data, NULL, 0, NULL);
	}
}

static void
msn_got_photo(PurpleUtilFetchUrlData *url_data, gpointer user_data,
		const gchar *url_text, gsize len, const gchar *error_message)
{
	MsnGetInfoStepTwoData *info2_data = (MsnGetInfoStepTwoData *)user_data;
	int id = -1;

	/* Unmarshall the saved state */
	MsnGetInfoData *info_data = info2_data->info_data;
	char *stripped = info2_data->stripped;
	char *url_buffer = info2_data->url_buffer;
	PurpleNotifyUserInfo *user_info = info2_data->user_info;
	char *photo_url_text = info2_data->photo_url_text;

	/* Make sure the connection is still valid if we got here by fetching a photo url */
	/* TODO: Instead of this, we should be canceling this when we disconnect */
	if (url_text && (error_message != NULL ||
					 g_list_find(purple_connections_get_all(), info_data->gc) == NULL))
	{
		purple_debug_warning("msn", "invalid connection. ignoring buddy photo info.\n");
		g_free(stripped);
		g_free(url_buffer);
		purple_notify_user_info_destroy(user_info);
		g_free(info_data->name);
		g_free(info_data);
		g_free(photo_url_text);
		g_free(info2_data);

		return;
	}

	/* Try to put the photo in there too, if there's one and is readable */
	if (user_data && url_text && len != 0)
	{
		if (strstr(url_text, "400 Bad Request")
			|| strstr(url_text, "403 Forbidden")
			|| strstr(url_text, "404 Not Found"))
		{

			purple_debug_info("msn", "Error getting %s: %s\n",
					photo_url_text, url_text);
		}
		else
		{
			char buf[1024];
			purple_debug_info("msn", "%s is %" G_GSIZE_FORMAT " bytes\n", photo_url_text, len);
			id = purple_imgstore_add_with_id(g_memdup(url_text, len), len, NULL);
			g_snprintf(buf, sizeof(buf), "<img id=\"%d\"><br>", id);
			purple_notify_user_info_prepend_pair(user_info, NULL, buf);
		}
	}

	/* We continue here from msn_got_info, as if nothing has happened */
#endif
	purple_notify_userinfo(info_data->gc, info_data->name, user_info, NULL, NULL);

	g_free(stripped);
	g_free(url_buffer);
	purple_notify_user_info_destroy(user_info);
	g_free(info_data->name);
	g_free(info_data);
#if PHOTO_SUPPORT
	g_free(photo_url_text);
	g_free(info2_data);
	if (id != -1)
		purple_imgstore_unref_by_id(id);
#endif
}

static void
msn_get_info(PurpleConnection *gc, const char *name)
{
	MsnGetInfoData *data;
	char *url;

	data       = g_new0(MsnGetInfoData, 1);
	data->gc   = gc;
	data->name = g_strdup(name);

	url = g_strdup_printf("%s%s", PROFILE_URL, name);

	purple_util_fetch_url(url, FALSE,
				   "Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1)",
				   TRUE, msn_got_info, data);

	g_free(url);
}

static gboolean msn_load(PurplePlugin *plugin)
{
	msn_notification_init();
	msn_switchboard_init();
	msn_sync_init();

	return TRUE;
}

static gboolean msn_unload(PurplePlugin *plugin)
{
	msn_notification_end();
	msn_switchboard_end();
	msn_sync_end();

	return TRUE;
}

static PurpleAccount *find_acct(const char *prpl, const char *acct_id)
{
	PurpleAccount *acct = NULL;

	/* If we have a specific acct, use it */
	if (acct_id) {
		acct = purple_accounts_find(acct_id, prpl);
		if (acct && !purple_account_is_connected(acct))
			acct = NULL;
	} else { /* Otherwise find an active account for the protocol */
		GList *l = purple_accounts_get_all();
		while (l) {
			if (!strcmp(prpl, purple_account_get_protocol_id(l->data))
					&& purple_account_is_connected(l->data)) {
				acct = l->data;
				break;
			}
			l = l->next;
		}
	}

	return acct;
}

static gboolean msn_uri_handler(const char *proto, const char *cmd, GHashTable *params)
{
	char *acct_id = g_hash_table_lookup(params, "account");
	PurpleAccount *acct;

	if (g_ascii_strcasecmp(proto, "msnim"))
		return FALSE;

	acct = find_acct("prpl-msn", acct_id);

	if (!acct)
		return FALSE;

	/* msnim:chat?contact=user@domain.tld */
	if (!g_ascii_strcasecmp(cmd, "Chat")) {
		char *sname = g_hash_table_lookup(params, "contact");
		if (sname) {
			PurpleConversation *conv = purple_find_conversation_with_account(
				PURPLE_CONV_TYPE_IM, sname, acct);
			if (conv == NULL)
				conv = purple_conversation_new(PURPLE_CONV_TYPE_IM, acct, sname);
			purple_conversation_present(conv);
		}
		/*else
			**If pidgindialogs_im() was in the core, we could use it here.
			 * It is all purple_request_* based, but I'm not sure it really belongs in the core
			pidgindialogs_im();*/

		return TRUE;
	}
	/* msnim:add?contact=user@domain.tld */
	else if (!g_ascii_strcasecmp(cmd, "Add")) {
		char *name = g_hash_table_lookup(params, "contact");
		purple_blist_request_add_buddy(acct, name, NULL, NULL);
		return TRUE;
	}

	return FALSE;
}


static PurplePluginProtocolInfo prpl_info =
{
	OPT_PROTO_MAIL_CHECK,
	NULL,					/* user_splits */
	NULL,					/* protocol_options */
	{"png", 0, 0, 96, 96, 0, PURPLE_ICON_SCALE_SEND},	/* icon_spec */
	msn_list_icon,			/* list_icon */
	msn_list_emblems,		/* list_emblems */
	msn_status_text,		/* status_text */
	msn_tooltip_text,		/* tooltip_text */
	msn_status_types,		/* away_states */
	msn_blist_node_menu,		/* blist_node_menu */
	NULL,					/* chat_info */
	NULL,					/* chat_info_defaults */
	msn_login,			/* login */
	msn_close,			/* close */
	msn_send_im,			/* send_im */
	NULL,					/* set_info */
	msn_send_typing,		/* send_typing */
	msn_get_info,			/* get_info */
	msn_set_status,			/* set_away */
	msn_set_idle,			/* set_idle */
	NULL,					/* change_passwd */
	msn_add_buddy,			/* add_buddy */
	NULL,					/* add_buddies */
	msn_rem_buddy,			/* remove_buddy */
	NULL,					/* remove_buddies */
	msn_add_permit,			/* add_permit */
	msn_add_deny,			/* add_deny */
	msn_rem_permit,			/* rem_permit */
	msn_rem_deny,			/* rem_deny */
	msn_set_permit_deny,	/* set_permit_deny */
	NULL,					/* join_chat */
	NULL,					/* reject chat invite */
	NULL,					/* get_chat_name */
	msn_chat_invite,		/* chat_invite */
	msn_chat_leave,			/* chat_leave */
	NULL,					/* chat_whisper */
	msn_chat_send,			/* chat_send */
	msn_keepalive,			/* keepalive */
	NULL,					/* register_user */
	NULL,					/* get_cb_info */
	NULL,					/* get_cb_away */
	msn_alias_buddy,		/* alias_buddy */
	msn_group_buddy,		/* group_buddy */
	msn_rename_group,		/* rename_group */
	NULL,					/* buddy_free */
	msn_convo_closed,		/* convo_closed */
	msn_normalize,			/* normalize */
	msn_set_buddy_icon,		/* set_buddy_icon */
	msn_remove_group,		/* remove_group */
	NULL,					/* get_cb_real_name */
	NULL,					/* set_chat_topic */
	NULL,					/* find_blist_chat */
	NULL,					/* roomlist_get_list */
	NULL,					/* roomlist_cancel */
	NULL,					/* roomlist_expand_category */
	msn_can_receive_file,	/* can_receive_file */
	msn_send_file,			/* send_file */
	msn_new_xfer,			/* new_xfer */
	msn_offline_message,			/* offline_message */
	NULL,					/* whiteboard_prpl_ops */
	NULL,					/* send_raw */
	NULL,					/* roomlist_room_serialize */
	NULL,					/* unregister_user */
	msn_send_attention,                     /* send_attention */
	msn_attention_types,                    /* attention_types */
	sizeof(PurplePluginProtocolInfo),       /* struct_size */
	msn_get_account_text_table,             /* get_account_text_table */
	NULL,                                   /* initiate_media */
	NULL                                    /* can_do_media */
};

static PurplePluginInfo info =
{
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,
	PURPLE_MINOR_VERSION,
	PURPLE_PLUGIN_PROTOCOL,                           /**< type           */
	NULL,                                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	PURPLE_PRIORITY_DEFAULT,                          /**< priority       */

	"prpl-msn",                                       /**< id             */
	"MSN",                                            /**< name           */
	DISPLAY_VERSION,                                  /**< version        */
	N_("Windows Live Messenger Protocol Plugin"),     /**< summary        */
	N_("Windows Live Messenger Protocol Plugin"),     /**< description    */
	NULL,                                             /**< author         */
	PURPLE_WEBSITE,                                   /**< homepage       */

	msn_load,                                         /**< load           */
	msn_unload,                                       /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	&prpl_info,                                       /**< extra_info     */
	NULL,                                             /**< prefs_info     */
	msn_actions,

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};

static void
init_plugin(PurplePlugin *plugin)
{
	PurpleAccountOption *option;

	option = purple_account_option_string_new(_("Server"), "server",
											MSN_SERVER);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options,
											   option);

	option = purple_account_option_int_new(_("Port"), "port", MSN_PORT);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options,
											   option);

	option = purple_account_option_bool_new(_("Use HTTP Method"),
										  "http_method", FALSE);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options,
											   option);

	option = purple_account_option_string_new(_("HTTP Method Server"),
										  "http_method_server", MSN_HTTPCONN_SERVER);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options,
											   option);

	option = purple_account_option_bool_new(_("Show custom smileys"),
										  "custom_smileys", TRUE);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options,
											   option);

	purple_cmd_register("nudge", "", PURPLE_CMD_P_PRPL,
	                  PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_PRPL_ONLY,
	                 "prpl-msn", msn_cmd_nudge,
	                  _("nudge: nudge a user to get their attention"), NULL);

	purple_prefs_remove("/plugins/prpl/msn");

	purple_signal_connect(purple_get_core(), "uri-handler", plugin,
		PURPLE_CALLBACK(msn_uri_handler), NULL);
}

PURPLE_INIT_PLUGIN(msn, init_plugin, info);
