#include "internal.h"
#include "conversation.h"

#define EXTPLACEMENT_OPT "/plugins/core/nosnilmot/extplacement"

static void
conv_placement_last_created_win_split(GaimConversation *conv)
{
	GaimConvWindow *win;

	win = gaim_get_last_window_with_type(gaim_conversation_get_type(conv));

	if (win == NULL) {
		win = gaim_conv_window_new();

		gaim_conv_window_add_conversation(win, conv);
		gaim_conv_window_show(win);
	}
	else
		gaim_conv_window_add_conversation(win, conv);
}

static void
conv_placement_by_number(GaimConversation *conv)
{
	GaimConvWindow *win = NULL;

	if (gaim_prefs_get_bool("/core/conversations/combine_chat_im"))
		win = g_list_last(gaim_get_windows())->data;
	else
		win = gaim_get_last_window_with_type(gaim_conversation_get_type(conv));

	if (win == NULL) {
		win = gaim_conv_window_new();

		gaim_conv_window_add_conversation(win, conv);
		gaim_conv_window_show(win);
	} else {
		int max_count = gaim_prefs_get_int(EXTPLACEMENT_OPT "/placement_number");
		int count = gaim_conv_window_get_conversation_count(win);

		if (count < max_count)
			gaim_conv_window_add_conversation(win, conv);
		else {
			GList *l = NULL;

			for (l = gaim_get_windows(); l != NULL; l = l->next) {
				win = (GaimConvWindow *)l->data;

				count = gaim_conv_window_get_conversation_count(win);
				if (count < max_count) {
					gaim_conv_window_add_conversation(win, conv);
					return;
				}
			}
			win = gaim_conv_window_new();

			gaim_conv_window_add_conversation(win, conv);
			gaim_conv_window_show(win);
		}
	}
}

static gboolean
plugin_load(GaimPlugin *plugin)
{
	gaim_prefs_add_none("/plugins/core/nosnilmot");
	gaim_prefs_add_none("/plugins/core/nosnilmot/extplacement");
	gaim_prefs_add_int(EXTPLACEMENT_OPT "/placement_number", 4);
	gaim_conv_placement_add_fnc("im_chat", _("Separate IM and Chat windows"),
							   &conv_placement_last_created_win_split);
	gaim_conv_placement_add_fnc("number", _("By conversation count"),
							   &conv_placement_by_number);
	return TRUE;
}

static gboolean
plugin_unload(GaimPlugin *plugin)
{
	gaim_conv_placement_remove_fnc("im_chat");
	gaim_conv_placement_remove_fnc("number");
	return TRUE;
}

static GaimPluginPrefFrame *
get_plugin_pref_frame(GaimPlugin *plugin) {
	GaimPluginPrefFrame *frame;
	GaimPluginPref *ppref;

	frame = gaim_plugin_pref_frame_new();

	ppref = gaim_plugin_pref_new_with_label("Conversation Placement");
	gaim_plugin_pref_frame_add(frame, ppref);

	ppref = gaim_plugin_pref_new_with_name_and_label(
									EXTPLACEMENT_OPT "/placement_number",
									"Number of conversations per window");
	gaim_plugin_pref_set_bounds(ppref, 1, 50);
	gaim_plugin_pref_frame_add(frame, ppref);

	return frame;
}

static GaimPluginUiInfo prefs_info = {
	get_plugin_pref_frame
};

static GaimPluginInfo info =
{
	GAIM_PLUGIN_API_VERSION,						/**< api_version	*/
	GAIM_PLUGIN_STANDARD,							/**< type			*/
	NULL,											/**< ui_requirement	*/
	0,												/**< flags			*/
	NULL,											/**< dependencies	*/
	GAIM_PRIORITY_DEFAULT,							/**< priority		*/

	"core-nosnilmot-extplacement",					/**< id				*/
	N_("ExtPlacement"),								/**< name			*/
	VERSION,										/**< version		*/
													/**  summary		*/
	N_("Extra conversation placement options."),
													/**  description	*/
	N_("Either restrict the number of conversations per windows"
	   " or use separate windows for IMs and Chats"),
	"Stu Tomlinson <stu@nosnilmot.com>",			/**< author			*/
	"http://www.nosnilmot.com/gaim/plugins/",		/**< homepage		*/

	plugin_load,									/**< load			*/
	plugin_unload,									/**< unload			*/
	NULL,											/**< destroy		*/

	NULL,											/**< ui_info		*/
	NULL,											/**< extra_info		*/
	&prefs_info,									/**< prefs_info		*/
	NULL											/**< actions		*/
};

static void
init_plugin(GaimPlugin *plugin)
{
}

GAIM_INIT_PLUGIN(extplacement, init_plugin, info)
