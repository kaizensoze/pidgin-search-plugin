/*#include "internal.h"
#include "account.h"
#include "core.h"
#include "dbus-maybe.h"
#include "debug.h"
#include "network.h"
#include "notify.h"
#include "pounce.h"
#include "prefs.h"
#include "privacy.h"
#include "prpl.h"
#include "request.h"
#include "server.h"
#include "signals.h"
#include "status.h"*/
#include "util.h"
#include "xmlnode.h"

static xmlnode*
load_search_provider(gchar *path_to_opensearch_xml)
{
	xmlnode *node;//, *child;

	//accounts_loaded = TRUE;

	node = purple_util_read_xml_from_file(path_to_opensearch_xml, _("search_provider"));

	//if (node == NULL)
		return node;

	//for (child = xmlnode_get_child(node, "account"); child != NULL;
	//		child = xmlnode_get_next_twin(child))
	//{
	//	PurpleAccount *new_acct;
	//	new_acct = parse_account(child);
	//	purple_accounts_add(new_acct);
	//}

	//xmlnode_free(node);

	//_purple_buddy_icons_account_loaded_cb();
}


/*

static xmlnode *
search_provider_to_xmlnode(void)
{
	xmlnode *node, *child;
	GList *cur;

	node = xmlnode_new("search_provider");
	xmlnode_set_attrib(node, "version", "1.0");

	for (cur = purple_accounts_get_all(); cur != NULL; cur = cur->next)
	{
		child = account_to_xmlnode(cur->data);
		xmlnode_insert_child(node, child);
	}

	return node;
}

static xmlnode *
account_to_xmlnode(PurpleAccount *account)
{
	PurpleAccountPrivate *priv = PURPLE_ACCOUNT_GET_PRIVATE(account);

	xmlnode *node, *child;
	const char *tmp;
	PurplePresence *presence;
	PurpleProxyInfo *proxy_info;

	node = xmlnode_new("account");

	child = xmlnode_new_child(node, "protocol");
	xmlnode_insert_data(child, purple_account_get_protocol_id(account), -1);

	child = xmlnode_new_child(node, "name");
	xmlnode_insert_data(child, purple_account_get_username(account), -1);

	if (purple_account_get_remember_password(account) &&
		((tmp = purple_account_get_password(account)) != NULL))
	{
		child = xmlnode_new_child(node, "password");
		xmlnode_insert_data(child, tmp, -1);
	}

	if ((tmp = purple_account_get_alias(account)) != NULL)
	{
		child = xmlnode_new_child(node, "alias");
		xmlnode_insert_data(child, tmp, -1);
	}

	if ((presence = purple_account_get_presence(account)) != NULL)
	{
		child = statuses_to_xmlnode(presence);
		xmlnode_insert_child(node, child);
	}

	if ((tmp = purple_account_get_user_info(account)) != NULL)
	{
		// TODO: Do we need to call purple_str_strip_char(tmp, '\r') here? 
		child = xmlnode_new_child(node, "userinfo");
		xmlnode_insert_data(child, tmp, -1);
	}

	if (g_hash_table_size(account->settings) > 0)
	{
		child = xmlnode_new_child(node, "settings");
		g_hash_table_foreach(account->settings, setting_to_xmlnode, child);
	}

	if (g_hash_table_size(account->ui_settings) > 0)
	{
		g_hash_table_foreach(account->ui_settings, ui_setting_to_xmlnode, node);
	}

	if ((proxy_info = purple_account_get_proxy_info(account)) != NULL)
	{
		child = proxy_settings_to_xmlnode(proxy_info);
		xmlnode_insert_child(node, child);
	}

	child = current_error_to_xmlnode(priv->current_error);
	xmlnode_insert_child(node, child);

	return node;
}*/
