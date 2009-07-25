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

#define TEST_PLUGIN_ID "gtk-test"
#define TEST_OBJECT_KEY "test"
#define two_strcat(x,y) (g_strconcat(x,y,NULL))

// 	filename is name only, not path!
// 	dir to use will be (linux)
// 	~/.purple/opensearch_files/
#define LINUX_PIDGIN_PREF_DIR g_strconcat(g_get_home_dir(), "/.purple/", NULL)
#define LINUX_OPENSEARCH_FILES "opensearch_files/"
#define LINUX_OPENSEARCH_ICONS "opensearch_icons/" 
// 	/usr/lib/pidgin/default_opensearch_files/
#define LINUX_OPENSEARCH_FILES_DEFAULT "/usr/lib/pidgin/default_opensearch_files/"
// 	(windows)
// 	%USER_HOME%\\Application%20Data\.purple\opensearch_files\
//#define WINDOWS_OPENSEARCH_FILES "%USER_HOME%\\Application Data\.purple\opensearch_files\"
// 	%PROGRAM_FILES%\Pidgin\default_opensearch_files\
//#define WINDOWS_OPENSEARCH_FILES_DEFAULT "%PROGRAM_FILES%\Pidgin\default_opensearch_files\"
	
	
typedef struct {
    gchar *name;
    gchar *query_url;
    gchar *icon_url;
    gchar *filename;
} search_engine;


/* Map of search engines */
static GHashTable *search_engines;
static void insert_search_engine(search_engine *site);
int delete_from_opensearch_files_dir(gchar *filename);

void destroy_search_engine_struct(search_engine* ptr){
	g_free(ptr->name);
	g_free(ptr->query_url);
	g_free(ptr->icon_url);
	g_free(ptr->filename);	
}

/*
	destroy_search_engine
	deletes the entry in the hash table with the key 'key'. Deletes the XML file
	associated with the search. Frees the memory used by the search struct.
	@parm key the unique key to find the search_engine struct from the hash table
*/
void destroy_search_engine(gchar* key){
	search_engine* search_engine_to_remove;	
	gpointer orig_key, orig_value;
	if (g_hash_table_lookup_extended(search_engines, key, &orig_key, &orig_value)) {
        	search_engine_to_remove =  g_hash_table_lookup(search_engines, key); 
        	g_hash_table_remove(search_engines, key);
        
        //g_free(orig_key); //segfault-a-licious!
        //g_free(orig_value); // segfault-a-licious!
    }
    
    // remove corresponding xml file
    if (search_engine_to_remove != NULL) {
        delete_from_opensearch_files_dir(search_engine_to_remove->filename);
    }
    destroy_search_engine_struct(search_engine_to_remove);	
}





/*
int get_params_length(xmlnode * firstParamNode){
	unsigned int result = 0;
	xmlnode * temp, *node;
	node = firstParamNode;
	// check to see if the node at least has the right name
	if( strcmp(node->name, "Param") != 0 ){
		return 0;
	}
	temp = firstParamNode;
	while(temp != NULL ){
		if(strcmp(xmlnode_get_attrib(temp,"value" ), "{searchTerms}") == 0 ){
			result += strlen(xmlnode_get_attrib(temp,"name" ));
			result++; // for the '=' symbol
		} else {
			result += strlen(xmlnode_get_attrib(temp,"name" ));
			result += strlen(xmlnode_get_attrib(temp,"value" ));
			result++; // for the '=' symbol	
		}
		temp = xmlnode_get_next_twin(temp);
	}
	return result;
}*/


// takes the Url  node
gchar* get_url_with_params(xmlnode * child){
	gchar *url = "", *query_param = "";
	xmlnode *temp;
	
	if( child == NULL ){
		purple_debug_info(TEST_PLUGIN_ID, "NULL argument\n");
		return NULL;
	}
	
	if( strcmp(child->name, "Url") != 0 ){
		purple_debug_info(TEST_PLUGIN_ID, "argument node is not named 'Url'\n");
		return NULL;
	}
	
	temp = child;
	
	// this is an attribute of Url tag
	url = g_strdup(xmlnode_get_attrib(temp,"template" ));
	url = two_strcat(url, "?");
	//return url;
	
		
	temp = xmlnode_get_child(temp, "Param");
	while(temp != NULL ){
		if(strcmp(xmlnode_get_attrib(temp,"value" ), "{searchTerms}") == 0 ){
			query_param = xmlnode_get_attrib(temp,"name" );
		} else {
			url = g_strconcat(url, xmlnode_get_attrib(temp,"name"), 
				"=", xmlnode_get_attrib(temp,"value" ), "&", NULL);
		}
		temp = xmlnode_get_next_twin(temp);
	}
	url = g_strconcat(url, query_param, "=", NULL); 
	purple_debug_info(TEST_PLUGIN_ID, "returning url %s\n", url);
	return url;
			
}

char *
purple_url_decode_malloc(const char *str)
{
	char *buf, *smaller_buf;
	guint i, j = 0;
	char *bum;
	char hex[3];

	g_return_val_if_fail(str != NULL, NULL);

	
	buf = malloc(strlen(str));

	for (i = 0; i < strlen(str); i++) {

		if (str[i] != '%')
			buf[j++] = str[i];
		else {
			strncpy(hex, str + ++i, 2);
			hex[2] = '\0';

			/* i is pointing to the start of the number */
			i++;

			/*
			 * Now it's at the end and at the start of the for loop
			 * will be at the next character.
			 */
			buf[j++] = strtol(hex, NULL, 16);
		}
	}

	buf[j] = '\0';

	if (!g_utf8_validate(buf, -1, (const char **)&bum))
		*bum = '\0';

	return buf;
}


/*	
	parse_opensearch
	@param opensearch_xml_filename the name of the opensearch xml file 
	@return the search_engine struct with URL information from the given opensearch file
*/
static search_engine*
parse_opensearch(gchar *opensearch_xml_filename_1)
{	//TODO if there is no ShortName use the longname, 
        //if there is no LongName or ShortName, invalid?
		
	int 		img_h = 0, img_w = 0, buffer_len = 0;	
	search_engine 	*result = NULL;		
	gchar 		*opensearch_xml_filename = NULL, *url = "", *query_param  = "", 
			*search_name = "",*img_xml_data = "", *img_url_decoded = NULL;
	gsize 		*ret_len  = NULL; // for image size
	guchar 		*img_binary = NULL;	
	xmlnode 	*child = NULL, *node = NULL, *temp = NULL;
	
	opensearch_xml_filename = g_strconcat(LINUX_OPENSEARCH_FILES,opensearch_xml_filename_1, NULL);
	
	node = purple_util_read_xml_from_file(opensearch_xml_filename, _(opensearch_xml_filename));
	
	if(node == NULL) {
		purple_debug_info(TEST_PLUGIN_ID, "the file %s could not be loaded\n", opensearch_xml_filename);
		purple_debug_info(TEST_PLUGIN_ID, "%s may not be a valid XML file\n", opensearch_xml_filename);
		return NULL;
	} 
		
	// if the root node is not named SearchPlugin
	// then the xml file may not be valid
	if( strcmp(node->name, "SearchPlugin") != 0 ){
		purple_debug_info(TEST_PLUGIN_ID, "the root node of the opensearch XML file should be 'SearchPlugin' but found '%s'\n", node->name);
		return NULL;
	}	
	
	child = node;
	
	if (child != NULL){
		//purple_debug_info(TEST_PLUGIN_ID, "%s\n", child->name );
		// there may be more than one Url tag within SearchPlugin
		child = xmlnode_get_child(child, "Url");
		while(child != NULL ){
			//purple_debug_info(TEST_PLUGIN_ID, "%s\n", xmlnode_get_attrib(child,"type" ) );
			purple_debug_info(TEST_PLUGIN_ID, "%s\n", child->name );
			//purple_debug_info(TEST_PLUGIN_ID, "i : %d: %s :%s (%d)\n", i++,
			//	 node->name, xmlnode_get_attrib(node,"type" ),  node->type );
			//purple_debug_info(TEST_PLUGIN_ID, "%s\n", g_hash_table_lookup(node->namespace_map, "type"));
			
			// at this point we only care about one of the 'Url' tags,
			// but there could be many (at least two that I know of).
			// mozilla uses one Url tag for suggestions
			if( strcmp(xmlnode_get_attrib(child,"type" ), "text/html") == 0 ){
				// parse_params and make url
				url = get_url_with_params(child);
				//url = g_strdup("http://google.com/search?q=");
				if(url == NULL) {
					purple_debug_info(TEST_PLUGIN_ID, "error creating url from params\n");
					return NULL;			
				}
			}
			child = xmlnode_get_next_twin(child);
		}
		child = node;
		// ShortName should be unique
		child = xmlnode_get_child(child, "ShortName");
		if(child == NULL) {
			purple_debug_info(TEST_PLUGIN_ID, "error reading 'ShortName' tag\n");
			return NULL;			
		}
		
		// maybe we don't need to duplicate... 
		search_name = g_strdup(xmlnode_get_data(child)); 
		if(search_name == NULL){
			purple_debug_info(TEST_PLUGIN_ID, "error reading 'ShortName' inner data\n");
			return NULL;
		}
	} 
	
	child = node;
	child = xmlnode_get_child(child, "Image");
	img_h = xmlnode_get_attrib(child,"height" );
	img_w = xmlnode_get_attrib(child,"width" );
	
	if(g_str_has_prefix(xmlnode_get_data(child), "data:image/x-icon;base64,") ){
		img_xml_data = g_strdup(&xmlnode_get_data(child)[25]);
		
		img_url_decoded = g_strdup(purple_url_decode( img_xml_data ));
		
		if(img_url_decoded != NULL){
			img_binary = purple_base64_decode(img_url_decoded, &ret_len);
			//purple_debug_info(TEST_PLUGIN_ID,"icon test: %s\n",img_url_decoded );
		} else {
			img_binary = purple_base64_decode(img_xml_data, &ret_len);
			//purple_debug_info(TEST_PLUGIN_ID,"icon test: %s\n",img_url_decoded );
		}
		//img_binary = purple_base64_decode(img_url_decoded, &ret_len);
		
		 /* purple_debug_info(TEST_PLUGIN_ID,"file save result: %d; path %s;\n", 
			g_file_set_contents(g_strconcat(LINUX_PIDGIN_PREF_DIR,LINUX_OPENSEARCH_ICONS,opensearch_xml_filename_1,".icon",NULL),
		 	&xmlnode_get_data(child)[24], strlen(&xmlnode_get_data(child)[24]),  NULL),
		 	g_strconcat(LINUX_PIDGIN_PREF_DIR,LINUX_OPENSEARCH_ICONS,opensearch_xml_filename_1,".icon",NULL)
		 ); */
		/*if(img_url_decoded != NULL)g_file_set_contents(g_strconcat(LINUX_PIDGIN_PREF_DIR,LINUX_OPENSEARCH_ICONS,opensearch_xml_filename_1,".ico",NULL),
		img_url_decoded,  strlen(img_url_decoded),  NULL);	*/ 
		 if(img_binary != NULL){
			 purple_debug_info(TEST_PLUGIN_ID,"file save result: %d; path %s;\n", 
				g_file_set_contents(g_strconcat(LINUX_PIDGIN_PREF_DIR,LINUX_OPENSEARCH_ICONS,opensearch_xml_filename_1,".ico",NULL),
			 	img_binary, ret_len,  NULL),		 	
			 	g_strconcat(LINUX_PIDGIN_PREF_DIR,LINUX_OPENSEARCH_ICONS,opensearch_xml_filename_1,".ico",NULL)
			 );
		} else {
			purple_debug_info(TEST_PLUGIN_ID,"count not do base64 decode on the search engines icon: %s\n",
			opensearch_xml_filename_1);
		}                                           
	        
	} else {
		img_binary = NULL;
	}
	//do a free from data created by purple_util_read_xml_from_file ??
	
	// the items returned should be copies! the only reason the following will work
	// is because the dom created by purple_util_read_xml_from_file remains in memory
	result = malloc(sizeof(search_engine));
	result->name = search_name;
	result->query_url = url;
	result->icon_url = g_strconcat(LINUX_PIDGIN_PREF_DIR,LINUX_OPENSEARCH_ICONS,opensearch_xml_filename_1,".ico",NULL);
	return result;	
}



/*
	Definitely need a function to get all opensearch xml files in a given folder
	(i.e. the installed searches).
*/
void load_all_from_opensearch_files_dir(void){	
	char* temp_name = NULL;
	search_engine* temp_result = NULL;
	GDir *  opsdir =  NULL;
	purple_debug_info(TEST_PLUGIN_ID, "Opening dir: %s\n", g_strconcat(LINUX_PIDGIN_PREF_DIR, LINUX_OPENSEARCH_FILES, NULL) );
	opsdir = g_dir_open (g_strconcat(LINUX_PIDGIN_PREF_DIR, LINUX_OPENSEARCH_FILES, NULL), 0, NULL);
	if(opsdir == NULL) purple_debug_info(TEST_PLUGIN_ID, "directory handle is NULL\n" );
	// making a new list wether there is a list in memory or not
	// but we should later check if there is one there and free it properly if its there 
	
	search_engines = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	temp_name = g_dir_read_name(opsdir);
	while( temp_name != NULL ){		 
		if(temp_name[0] != '.'){
			temp_result = parse_opensearch( temp_name );
			if(temp_result != NULL){
				temp_result->filename = g_strdup(temp_name);
                purple_debug_info(TEST_PLUGIN_ID, "filename: %s\n", temp_result->filename);
			}
			// put temp_result in the hash table
			insert_search_engine(temp_result);
		}
		
		temp_name = g_dir_read_name(opsdir);
	}
	g_dir_close(opsdir);
	
	/*
    // view contents of search engine hashtable
    GList *keys = g_hash_table_get_keys(search_engines);
    purple_debug_info(TEST_PLUGIN_ID, "Search Engines:\n");
    while (keys) {
        search_engine *eng;
        gpointer key = keys->data;

        eng = g_hash_table_lookup(search_engines, key);
        purple_debug_info(TEST_PLUGIN_ID, "%s\n", eng->name);
        keys = keys->next;
    }*/
}

static void insert_search_engine(search_engine *site) {	
    
    gpointer orig_key, orig_value;
    const gchar *key, *temp;
    int u = 1;
    
    key = site->filename;
   
    /*while(g_hash_table_lookup_extended(search_engines, key, &orig_key, &orig_value)) {
    	//g_free(key);
    	key = g_strdup(site->name);
    	temp = atoi(u);
    	key = g_strconcat(key,temp, NULL);
    	//g_free(temp);    	
    }*/
    
    g_hash_table_insert(search_engines, g_strdup(key), site);
    //g_free(key);
}

/*
	Maybe need a function to copy a given file to the opensearch plugins directory
	(i.e. to install a search)
*/

/*	
	Definitely Need a function to delete a given file from the opensearch plugins directory
	(i.e. to uninstall a search)
	@param filename name of opensearch file to delete, filename must just be a name, not a full path
	@return 0 if no err, -1 if error
*/
int delete_from_opensearch_files_dir(gchar *filename){
	int result = -1;
	result = g_remove(g_strconcat(LINUX_PIDGIN_PREF_DIR, LINUX_OPENSEARCH_FILES, filename, NULL) );
	return result;
}

static void test_xml()
{
	load_all_from_opensearch_files_dir();
	/*search_engine* test = NULL;
	test = parse_opensearch("google_search-opensearch.xml");
	if(test != NULL){
		purple_debug_info(TEST_PLUGIN_ID, "%s\n", test->name);
		purple_debug_info(TEST_PLUGIN_ID, "%s\n", test->query_url);
		//if(test->name == NULL) purple_debug_info(TEST_PLUGIN_ID, "error getting test->name == NULL\n");
	} else {
		purple_debug_info(TEST_PLUGIN_ID, "error getting search_engine struct\n");
	}
	*/
	
	//int i = 0, ii = 0;
	//xmlnode *node = NULL;//, *temp = NULL;	
	// path relative to user's dir (i.e ~/.purple/)
	//gchar *search_provider_path = "google_search-opensearch.xml";
	//node = load_search_provider(search_provider_path);
	//node = purple_util_read_xml_from_file(search_provider_path, _("search_provider"));
	//if(node == NULL) {
	//	purple_debug_info(TEST_PLUGIN_ID, "the file %s could not be loaded\n", search_provider_path);
	//} else {
		
			
		/*temp = node;
		
		while(node != NULL ){
			purple_debug_info(TEST_PLUGIN_ID, "%s\n", xmlnode_get_attrib(node,"type" ) );
			//purple_debug_info(TEST_PLUGIN_ID, "i : %d: %s :%s (%d)\n", i++,
			//	 node->name, xmlnode_get_attrib(node,"type" ),  node->type );
			//purple_debug_info(TEST_PLUGIN_ID, "%s\n", g_hash_table_lookup(node->namespace_map, "type"));
			node = xmlnode_get_next_twin(node);
		}
		node = temp->child;
		i = 0;
		while(node != NULL){					
			purple_debug_info(TEST_PLUGIN_ID, "i : %d: %s :%s (%d)\n", i++, 
				node->name, xmlnode_get_attrib(node,"type" ), node->type );		
			if(node->name != NULL && strcmp(node->name, "Url") == 0  ){
				temp = node->child;
				while(temp != NULL){
					purple_debug_info(TEST_PLUGIN_ID, "ii : %d: %s :%s (%d)\n", ii++, 
						temp->name, xmlnode_get_attrib(temp,"type" ), temp->type );
					temp = temp->next;
				}
				ii = 0;
			} 
				
			node = node->next;			
		}*/
	//}
}

/*
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
*/

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
