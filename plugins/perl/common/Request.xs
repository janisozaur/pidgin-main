#include "module.h"

/* TODO

void *
gaim_request_fields(handle, title, primary, secondary, fields, ok_text, ok_cb, cancel_text, cancel_cb, user_data)

void *
gaim_request_input(handle, title, primary, secondary, default_value, multiline, masked, hint, ok_text, ok_cb, cancel_text, cancel_cb, user_data)
 

void *
gaim_request_file(handle, title, filename, savedialog, ok_cb, cancel_cb, user_data)

void 
gaim_request_field_account_set_filter(field, filter_func)


void *
gaim_request_action(handle, title, primary, secondary, default_action, user_data, action_count, gaim_request_action)


void *
gaim_request_choice(handle, title, primary, secondary, default_value, ok_text, ok_cb, cancel_text, cancel_cb, user_data, gaim_request_choice)

void *
gaim_request_choice_varg(handle, title, primary, secondary, default_value, ok_text, ok_cb, cancel_text, cancel_cb, user_data, choices)



*/


MODULE = Gaim::Request  PACKAGE = Gaim::Request  PREFIX = gaim_request_
PROTOTYPES: ENABLE


void *
gaim_request_action_varg(handle, title, primary, secondary, default_action, user_data, action_count, actions)
 	void * handle
	const char *title
	const char *primary
	const char *secondary
	unsigned int default_action
	void *user_data 
	size_t action_count
	va_list actions



void 
gaim_request_close(type, uihandle)
	Gaim::RequestType type
	void * uihandle 

void 
gaim_request_close_with_handle(handle)
	void * handle 

Gaim::Account
gaim_request_field_account_get_default_value(field)
	Gaim::Request::Field field

SV *
gaim_request_field_account_get_filter(field)
	Gaim::Request::Field field

gboolean 
gaim_request_field_account_get_show_all(field)
	Gaim::Request::Field field

Gaim::Account
gaim_request_field_account_get_value(field)
	Gaim::Request::Field field

Gaim::Request::Field
gaim_request_field_account_new(id, text, account)
	const char *id
	const char *text
	Gaim::Account account

void 
gaim_request_field_account_set_default_value(field, default_value)
	Gaim::Request::Field field
	Gaim::Account default_value


void 
gaim_request_field_account_set_show_all(field, show_all)
	Gaim::Request::Field field
	gboolean show_all

void 
gaim_request_field_account_set_value(field, value)
	Gaim::Request::Field field
	Gaim::Account value

gboolean 
gaim_request_field_bool_get_default_value(field)
	Gaim::Request::Field field

gboolean 
gaim_request_field_bool_get_value(field)
	Gaim::Request::Field field

Gaim::Request::Field
gaim_request_field_bool_new(id, text, default_value)
	const char *id
	const char *text
	gboolean default_value

void 
gaim_request_field_bool_set_default_value(field, default_value)
	Gaim::Request::Field field
	gboolean default_value

void 
gaim_request_field_bool_set_value(field, value)
	Gaim::Request::Field field
	gboolean value

void 
gaim_request_field_choice_add(field, label)
	Gaim::Request::Field field
	const char *label

int 
gaim_request_field_choice_get_default_value(field)
	Gaim::Request::Field field

void
gaim_request_field_choice_get_labels(field)
	Gaim::Request::Field field
PREINIT:
        GList *l;
PPCODE:
        for (l = gaim_request_field_choice_get_labels(field); l != NULL; l = l->next) {
                XPUSHs(sv_2mortal(gaim_perl_bless_object(l->data, "Gaim::ListItem")));
        }




int 
gaim_request_field_choice_get_value(field)
	Gaim::Request::Field field

Gaim::Request::Field
gaim_request_field_choice_new(id, text, default_value)
	const char *id
	const char *text
	int default_value

void 
gaim_request_field_choice_set_default_value(field, default_value)
	Gaim::Request::Field field
	int default_value

void 
gaim_request_field_choice_set_value(field, value)
	Gaim::Request::Field field
	int value

void 
gaim_request_field_destroy(field)
	Gaim::Request::Field field

const char *
gaim_request_field_get_id(field)
	Gaim::Request::Field field

const char *
gaim_request_field_get_label(field)
	Gaim::Request::Field field

Gaim::RequestFieldType
gaim_request_field_get_type(field)
	Gaim::Request::Field field

const char *
gaim_request_field_get_type_hint(field)
	Gaim::Request::Field field

void 
gaim_request_field_group_add_field(group, field)
	Gaim::Request::Field::Group group
	Gaim::Request::Field field

void 
gaim_request_field_group_destroy(group)
	Gaim::Request::Field::Group group

void
gaim_request_field_group_get_fields(group)
	Gaim::Request::Field::Group group
PREINIT:
        GList *l;
PPCODE:
        for (l = gaim_request_field_group_get_fields(group); l != NULL; l = l->next) {
                XPUSHs(sv_2mortal(gaim_perl_bless_object(l->data, "Gaim::ListItem")));
        }



const char *
gaim_request_field_group_get_title(group)
	Gaim::Request::Field::Group group

Gaim::Request::Field::Group
gaim_request_field_group_new(title)
	const char *title

int 
gaim_request_field_int_get_default_value(field)
	Gaim::Request::Field field

int 
gaim_request_field_int_get_value(field)
	Gaim::Request::Field field

Gaim::Request::Field
gaim_request_field_int_new(id, text, default_value)
	const char *id
	const char *text
	int default_value

void 
gaim_request_field_int_set_default_value(field, default_value)
	Gaim::Request::Field field
	int default_value

void 
gaim_request_field_int_set_value(field, value)
	Gaim::Request::Field field
	int value

gboolean 
gaim_request_field_is_required(field)
	Gaim::Request::Field field

gboolean 
gaim_request_field_is_visible(field)
	Gaim::Request::Field field

Gaim::Request::Field
gaim_request_field_label_new(id, text)
	const char *id
	const char *text

void 
gaim_request_field_list_add(field, item, data)
	Gaim::Request::Field field
	const char *item
 	void * data

void 
gaim_request_field_list_add_selected(field, item)
	Gaim::Request::Field field
	const char *item

void 
gaim_request_field_list_clear_selected(field)
	Gaim::Request::Field field

void *
gaim_request_field_list_get_data(field, text)
	Gaim::Request::Field field
	const char *text

void
gaim_request_field_list_get_items(field)
	Gaim::Request::Field field
PREINIT:
        GList *l;
PPCODE:
        for (l = gaim_request_field_list_get_items(field); l != NULL; l = l->next) {
                XPUSHs(sv_2mortal(gaim_perl_bless_object(l->data, "Gaim::ListItem")));
        }

gboolean 
gaim_request_field_list_get_multi_select(field)
	Gaim::Request::Field field

void
gaim_request_field_list_get_selected(field)
	Gaim::Request::Field field
PREINIT:
        GList *l;
PPCODE:
        for (l = gaim_request_field_list_get_selected(field); l != NULL; l = l->next) {
                XPUSHs(sv_2mortal(gaim_perl_bless_object(l->data, "Gaim::ListItem")));
        }


gboolean 
gaim_request_field_list_is_selected(field, item)
	Gaim::Request::Field field
	const char *item

Gaim::Request::Field
gaim_request_field_list_new(id, text)
	const char *id
	const char *text

void 
gaim_request_field_list_set_multi_select(field, multi_select)
	Gaim::Request::Field field
	gboolean multi_select

Gaim::Request::Field
gaim_request_field_new(id, text, type)
	const char *id
	const char *text
	Gaim::RequestFieldType type

void 
gaim_request_field_set_label(field, label)
	Gaim::Request::Field field
	const char *label

void 
gaim_request_field_set_required(field, required)
	Gaim::Request::Field field
	gboolean required

void 
gaim_request_field_set_type_hint(field, type_hint)
	Gaim::Request::Field field
	const char *type_hint

void 
gaim_request_field_set_visible(field, visible)
	Gaim::Request::Field field
	gboolean visible

const char *
gaim_request_field_string_get_default_value(field)
	Gaim::Request::Field field

const char *
gaim_request_field_string_get_value(field)
	Gaim::Request::Field field

gboolean 
gaim_request_field_string_is_editable(field)
	Gaim::Request::Field field

gboolean 
gaim_request_field_string_is_masked(field)
	Gaim::Request::Field field

gboolean 
gaim_request_field_string_is_multiline(field)
	Gaim::Request::Field field

Gaim::Request::Field
gaim_request_field_string_new(id, text, default_value, multiline)
	const char *id
	const char *text
	const char *default_value
	gboolean multiline

void 
gaim_request_field_string_set_default_value(field, default_value)
	Gaim::Request::Field field
	const char *default_value

void 
gaim_request_field_string_set_editable(field, editable)
	Gaim::Request::Field field
	gboolean editable

void 
gaim_request_field_string_set_masked(field, masked)
	Gaim::Request::Field field
	gboolean masked

void 
gaim_request_field_string_set_value(field, value)
	Gaim::Request::Field field
	const char *value

void 
gaim_request_fields_add_group(fields, group)
	Gaim::Request::Fields fields
	Gaim::Request::Field::Group group

gboolean 
gaim_request_fields_all_required_filled(fields)
	Gaim::Request::Fields fields

void 
gaim_request_fields_destroy(fields)
	Gaim::Request::Fields fields

gboolean 
gaim_request_fields_exists(fields, id)
	Gaim::Request::Fields fields
	const char *id

Gaim::Account
gaim_request_fields_get_account(fields, id)
	Gaim::Request::Fields fields
	const char *id

gboolean 
gaim_request_fields_get_bool(fields, id)
	Gaim::Request::Fields fields
	const char *id

int 
gaim_request_fields_get_choice(fields, id)
	Gaim::Request::Fields fields
	const char *id

Gaim::Request::Field
gaim_request_fields_get_field(fields, id)
	Gaim::Request::Fields fields
	const char *id

void
gaim_request_fields_get_groups(fields)
	Gaim::Request::Fields fields
PREINIT:
        GList *l;
PPCODE:
        for (l = gaim_request_fields_get_groups(fields); l != NULL; l = l->next) {
                XPUSHs(sv_2mortal(gaim_perl_bless_object(l->data, "Gaim::ListItem")));
        }



int 
gaim_request_fields_get_integer(fields, id)
	Gaim::Request::Fields fields
	const char *id

void
gaim_request_fields_get_required(fields)
	Gaim::Request::Fields fields
PREINIT:
        GList *l;
PPCODE:
        for (l = gaim_request_fields_get_required(fields); l != NULL; l = l->next) {
                XPUSHs(sv_2mortal(gaim_perl_bless_object(l->data, "Gaim::ListItem")));
        }



const char *
gaim_request_fields_get_string(fields, id)
	Gaim::Request::Fields fields
	const char *id

gboolean 
gaim_request_fields_is_field_required(fields, id)
	Gaim::Request::Fields fields
	const char *id

Gaim::Request::Fields
gaim_request_fields_new()
 

Gaim::Request::UiOps
gaim_request_get_ui_ops()
 

void 
gaim_request_set_ui_ops(ops)
	Gaim::Request::UiOps ops

