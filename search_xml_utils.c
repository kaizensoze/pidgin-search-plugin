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
static int delete_from_opensearch_files_dir(gchar *filename);

static void destroy_search_engine_struct(search_engine* ptr){
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
static void destroy_search_engine(gchar* key){
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

static gchar *search_string_replace(search_engine* se, gchar * search_query){
	gchar ** parts = NULL;
	gchar * result = "";
	parts = g_strsplit(se->query_url, "{searchTerms}", 0);
	search_query = g_strdup(purple_url_encode(search_query));
	int i = 0;
	// skip the last part
	while(parts[i] != NULL){
		if(parts[i+1] == NULL){
			result = g_strconcat(result,parts[i], NULL);
		} else {
			result = g_strconcat(result,parts[i],search_query, NULL);
		}
		i++;
	}
	return result;	 
}

// takes the Url  node
static gchar* get_url_with_params(xmlnode * child){
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
	
	
	
	// only support the GET method
	if( strcmp(xmlnode_get_attrib(temp,"method" ), "GET") != 0 ) return NULL;
	// this is an attribute of Url tag
	url = g_strdup(xmlnode_get_attrib(temp,"template" ));
	if( g_strrstr(url,"?") == NULL) url = two_strcat(url, "?");
	//return url;
	
		
	temp = xmlnode_get_child(temp, "Param");
	if(temp == NULL){
		temp = xmlnode_get_child(temp, "Parameter");
	}
	//if(temp == NULL){
	//	return NULL;
	//}
	while(temp != NULL ){
		//if(strcmp(xmlnode_get_attrib(temp,"value" ), "{searchTerms}") == 0 ){
		//	query_param = xmlnode_get_attrib(temp,"name" );
		//} else {
			url = g_strconcat(url, xmlnode_get_attrib(temp,"name"), 
				"=", xmlnode_get_attrib(temp,"value" ), "&", NULL);
		//}
		temp = xmlnode_get_next_twin(temp);
	}
	//if(strlen(query_param) > 0) url = g_strconcat(url, query_param, "=", NULL); 
	
	purple_debug_info(TEST_PLUGIN_ID, "returning url %s\n", url);
	return url;
			
}

// not needed probably:
static char *
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
{			
	int 		img_h = 0, img_w = 0, buffer_len = 0;	
	search_engine 	*result = NULL;		
	gchar 		*opensearch_xml_filename = NULL, *url = "", *query_param  = "", 
			*search_name = "", *img_xml_data = "", *img_url_decoded = NULL;
	gsize 		*ret_len  = 0; // for image size
	guchar 		*img_binary = NULL;	
	xmlnode 	*child = NULL, *node = NULL, *temp = NULL;
	
	opensearch_xml_filename = g_strconcat(LINUX_OPENSEARCH_FILES,opensearch_xml_filename_1, NULL);
	
	node = purple_util_read_xml_from_file(opensearch_xml_filename, _(opensearch_xml_filename));
	
	if(node == NULL) {
		purple_debug_info(TEST_PLUGIN_ID, "the file %s could not be loaded\n", opensearch_xml_filename);
		purple_debug_info(TEST_PLUGIN_ID, "%s may not be a valid XML file\n", opensearch_xml_filename);
		return NULL;
	} 
		
	// if the root node is not named SearchPlugin or OpenSearchDescription
	// then the xml file may not be valid
	if( (strcmp(node->name, "SearchPlugin") != 0) && (strcmp(node->name, "OpenSearchDescription") != 0) ){
		purple_debug_info(TEST_PLUGIN_ID, "the root node of the opensearch XML file should be 'OpenSearchDescription' or for mozsearch 'SearchPlugin' but found '%s'\n", node->name);
		return NULL;
	}	
	
	child = node;
	
	if (child != NULL){
		//purple_debug_info(TEST_PLUGIN_ID, "%s\n", child->name );
		// there may be more than one Url tag within SearchPlugin
		child = xmlnode_get_child(child, "Url");
		while(child != NULL ){
			purple_debug_info(TEST_PLUGIN_ID, "%s\n", child->name );
			
			// at this point we only care about one of the 'Url' tags,
			// but there could be many.
			// mozilla uses one Url tag for suggestions of type text/json (double check that)
			if( strcmp(xmlnode_get_attrib(child,"type" ), "text/html") == 0 ){
				// parse_params and make url
				url = get_url_with_params(child);
				
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
			child = node;
			// LongName should be unique
			child = xmlnode_get_child(child, "LongName");
			if(child == NULL) {
				purple_debug_info(TEST_PLUGIN_ID, "error reading 'ShortName' and 'LongName' tag\n");
				return NULL;
			}		
		}
		
		// maybe we don't need to duplicate... 
		search_name = g_strdup(xmlnode_get_data(child)); 
		if(search_name == NULL){
			purple_debug_info(TEST_PLUGIN_ID, "error reading 'ShortName'/'LongName' inner data\n");
			return NULL;
		}
	} 
	
	result = malloc(sizeof(search_engine));
	result->name = search_name;
	result->query_url = url;
	
	
	child = node;
	child = xmlnode_get_child(child, "Image");
	//img_h = atoi(xmlnode_get_attrib(child,"height" ));
	//img_w = atoi(xmlnode_get_attrib(child,"width" ));
     if(child != NULL)
	if(g_str_has_prefix(xmlnode_get_data(child), "data:image/x-icon;base64,") ){
		img_xml_data = g_strdup(&xmlnode_get_data(child)[25]);
		
		// TODO: free
		img_url_decoded = g_strdup(purple_url_decode( img_xml_data ));
		
		if(img_url_decoded != NULL){
			img_binary = purple_base64_decode(img_url_decoded, &ret_len);			
		} else {
			img_binary = purple_base64_decode(img_xml_data, &ret_len);			
		}
		
		 if(img_binary != NULL){
		 	// TODO: free?
			 purple_debug_info(TEST_PLUGIN_ID,"file save result: %d; path %s;\n", 
				g_file_set_contents(g_strconcat(LINUX_PIDGIN_PREF_DIR,LINUX_OPENSEARCH_ICONS,opensearch_xml_filename_1,".ico",NULL),
			 	img_binary, ret_len,  NULL),		 	
			 	g_strconcat(LINUX_PIDGIN_PREF_DIR,LINUX_OPENSEARCH_ICONS,opensearch_xml_filename_1,".ico",NULL)
			 );
			 result->icon_url = g_strconcat(LINUX_PIDGIN_PREF_DIR,LINUX_OPENSEARCH_ICONS,opensearch_xml_filename_1,".ico",NULL);
		} else {
			purple_debug_info(TEST_PLUGIN_ID,"count not do base64 decode on the search engines icon: %s\n",
			opensearch_xml_filename_1);
		}                                           
	        
	} else {
		img_binary = NULL;
	}
	//do a free from data created by purple_util_read_xml_from_file ??
	
	// the items returned from the xml dom are references to a structure in memory
	// the dom created by purple_util_read_xml_from_file remains in memory but should be freed
	// without altering data in seearch_engine structs
		
	return result;	
}



/*
	load_all_from_opensearch_files_dir
	get all opensearch xml files from the folder
	(i.e. the installed searches).
*/
static void load_all_from_opensearch_files_dir(void){	
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
                		insert_search_engine(temp_result);
                		purple_debug_info(TEST_PLUGIN_ID, "name: %s\n", temp_result->name);
                		purple_debug_info(TEST_PLUGIN_ID, "filename: %s\n", temp_result->filename);
				// put temp_result in the hash table
				
			}
			
		}
		
		temp_name = g_dir_read_name(opsdir);
	}
	g_dir_close(opsdir);
	
	
    // view contents of search engine hashtable
    GList *keys = g_hash_table_get_keys(search_engines);
    purple_debug_info(TEST_PLUGIN_ID, "Search Engines:\n");
    while (keys) {
        search_engine *eng;
        gpointer key = keys->data;

        eng = g_hash_table_lookup(search_engines, key);
        purple_debug_info(TEST_PLUGIN_ID, "%s\n", eng->name);
        keys = keys->next;
    }
}

static void insert_search_engine(search_engine *site) {	
    
    gpointer orig_key, orig_value;
    const gchar *key, *temp;
    gchar buf[10];
    int i = 1, loop = 0;
    
    //key = site->filename;
    key = site->name;
    while(g_hash_table_lookup_extended(search_engines, key, &orig_key, &orig_value) != NULL) {
    	loop = 1;
    	//g_free(key);
    	//key = g_strdup(site->name);
    	g_ascii_dtostr(&buf,10,(gdouble)(i++));
    	key = g_strconcat(site->name,buf, NULL);
    	//g_free(temp);    	
    }/* */
    if(loop == 1) site->name = g_strdup(key);
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
static int delete_from_opensearch_files_dir(gchar *filename){
	int result = -1;
	result = g_remove(g_strconcat(LINUX_PIDGIN_PREF_DIR, LINUX_OPENSEARCH_FILES, filename, NULL) );
	return result;
}

static void test_xml(){
	load_all_from_opensearch_files_dir();	
}

