 /** @file module_list.h
 *  @brief Definitions for a module list.
 *
 *  Definitions for a module list, implemented using BSD 
 *  implementation of a singly-linked list (queue.h)
 *
 *  @author Nuno Pereira
 *  @date July, 2019
 */
 
#ifndef HTTP_MODULE_LIST_H 
#define HTTP_MODULE_LIST_H 

#include "queue.h"

/**
 * Entry we generate for each module
 */
struct module_descriptor
{
    /* module id */
	int id;
	
	/* module name */
	char *name;

    /* list head for the publish topics of this module */
    SLIST_HEAD(slisthead_topics, topic_descriptor) topics;

    SLIST_ENTRY(module_descriptor) next_mod;
};

/**
 * Entry we generate for each topic
 */
struct topic_descriptor
{
	/* topic string*/
	char *topic;

    SLIST_ENTRY(topic_descriptor) next_topic;
};


/**
 * Init the list
 */
int module_list_init();

/**
 * Add a module to the list
 * 
 * @param mod_id module id
 * @param mod_name module name
 * @return returns 0 (success), -1 (failure)
 */
int module_list_add(int mod_id, char *mod_name);

/**
 * Remove a module from the list *and* release resources 
 * 
 * @param mod_id id of the module to delete
 * @return returns 0 (success), -1 (failure)
 */
int module_list_del_by_id(int mod_id);

/**
 * Get the module topics list from a module id
 * 
 * @param mod_id the module id to search
 * @return returns the module name (success), NULL (failure)
 */
struct slisthead_topics *module_list_get_topic_list_by_id(int mod_id);

/**
 * Get the module name from a module id
 * 
 * @param mod_id the module id to search
 * @return returns the module name (success), NULL (failure)
 */
char *module_list_get_name_by_id(int mod_id);

/**
 * Add a topic to the list
 * 
 * @param topic topic name
 * @param topics the list of topics
 * @return returns 0 if not inserted (already in list), 1 if inserted (not in list), -1 (failure)
 */
int topic_list_check_and_add(char *topic, int mod_id);

/**
 * Remove a all topics from the list *and* release resources 
 * 
 * @param topics topic list 
 * @return returns 0 (success), -1 (failure)
 */
int topic_list_del_all(struct slisthead_topics *topics);

/**
 * Remove a topic from the list *and* release resources 
 * 
 * @param topic topic to delete
 * @param mod_id the module publishing the topic 
 * @return returns 0 (success), -1 (failure)
 */
int topic_list_del(char *topic, int mod_id);

/**
 * Find a topic given the topic list
 * 
 * @param topic the topic to search
 * @param topics points to the list of topics
 * @return 1 if found, 0 if not
 */
int topic_list_is_in_list(char *topic, struct slisthead_topics *topics);

#endif