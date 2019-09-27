/** @file Module_list.c
 *  @brief Implementation of a module list.
 *
 *  Implementation for a module list, using BSD 
 *  singly-linked list (queue.h)
 *
 *
 *  @author Nuno Pereira
 *  @date July, 2019
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "module_list.h"
#include "queue.h"

/* Declare variable to hold the singly-linked list head for the modules */
SLIST_HEAD(slisthead_modules, module_descriptor) modules;

int module_list_init()
{
    SLIST_INIT(&modules);
    return 0;
}

/**
 * Add a module to the list
 * 
 * @param mod_id module id
 * @param mod_name module name
 * @return returns 0 (success), -1 (failure)
 */
int module_list_add(int mod_id, char *mod_name)
{
    int len=strlen(mod_name);
    struct module_descriptor *new_mod = malloc(sizeof(struct module_descriptor));

    if (new_mod == NULL ) return(-1);

    new_mod->name = malloc(len+1);
    if (new_mod->name != NULL) {
        strncpy(new_mod->name, mod_name, len); /* strncpy does not guarantee a '\0'-terminated string */
        new_mod->name[len]='\0';
        new_mod->id = mod_id;
        SLIST_INIT(&new_mod->topics);
        SLIST_INSERT_HEAD(&modules, new_mod, next_mod);
    } else {
        if (new_mod->name != NULL) free(new_mod->name);
        return -1;
    }

    return 0;
}

/**
 * Remove a module from the list *and* release resources 
 * 
 * @param mod_id id of the module to delete
 * @return returns 0 (success), -1 (failure)
 */
int module_list_del_by_id(int mod_id) 
{
    struct module_descriptor *mod;

    /* find module */
    SLIST_FOREACH(mod, &modules, next_mod) {
        if (mod->id==mod_id) {
            if (mod->name != NULL) free(mod->name);
            topic_list_del_all(&mod->topics);
            SLIST_REMOVE(&modules, mod, module_descriptor, next_mod);
            free(mod);
            return 0;
        }
    }

    return -1; 
}

/**
 * Get the module topics list from a module id
 * 
 * @param mod_id the module id to search
 * @return returns the module name (success), NULL (failure)
 */
struct slisthead_topics *module_list_get_topic_list_by_id(int mod_id)
{
    struct module_descriptor *mod;

    SLIST_FOREACH(mod, &modules, next_mod) {
        if (mod->id==mod_id) return &mod->topics;
    }

    return NULL;
}

/**
 * Get the module name from a module id
 * 
 * @param mod_id the module id to search
 * @return returns the module name (success), NULL (failure)
 */
char *module_list_get_name_by_id(int mod_id)
{
    struct module_descriptor *mod;

    SLIST_FOREACH(mod, &modules, next_mod) {
        if (mod->id==mod_id) return mod->name;
    }

    return NULL;
}

/**
 * Add a topic to the list
 * 
 * @param topic topic name
 * @param topics the list of topics
 * @return returns 0 if not inserted (already in list), 1 if inserted (not in list), -1 (failure)
 */
int topic_list_check_and_add(char *topic, int mod_id)
{
    int len=strlen(topic);

    struct slisthead_topics *topics = module_list_get_topic_list_by_id(mod_id);
    if (topics == NULL) return -1;

    if (topic_list_is_in_list(topic, topics) == 1) return 0; // already in list

    struct topic_descriptor *new_topic = malloc(sizeof(struct topic_descriptor));
    if (new_topic == NULL ) return -1;
    new_topic->topic = malloc(len+1);
    if (new_topic->topic != NULL) {
        strncpy(new_topic->topic, topic, len); /* strncpy does not guarantee a '\0'-terminated string */
        new_topic->topic[len]='\0';
        SLIST_INSERT_HEAD(topics, new_topic, next_topic);
        return 1;
    } else {
        if (new_topic->topic != NULL) free(new_topic->topic);
        return -1;
    }
}

/**
 * Remove a all topics from the list *and* release resources 
 * 
 * @param topics topic list 
 * @return returns 0 (success), -1 (failure)
 */
int topic_list_del_all(struct slisthead_topics *topics) 
{
    struct topic_descriptor *t;

    SLIST_FOREACH(t, topics, next_topic) {
        if (t->topic != NULL) free(t->topic);
        SLIST_REMOVE(topics, t, topic_descriptor, next_topic);
        free(t);
    }

    return -1; 
}

/**
 * Remove a topic from the list *and* release resources 
 * 
 * @param topic topic to delete
 * @param mod_id the module publishing the topic 
 * @return returns 0 (success), -1 (failure)
 */
int topic_list_del(char *topic, int mod_id) 
{
    struct topic_descriptor *t;

    struct slisthead_topics *topics = module_list_get_topic_list_by_id(mod_id);
    if (topics == NULL) {
        printf("Could not find module id %d\n", mod_id);
        return -1;
    }

    /* find topic */
    SLIST_FOREACH(t, topics, next_topic) {
        if (strcmp(t->topic, topic) == 0) {
            if (t->topic != NULL) free(t->topic);
            SLIST_REMOVE(topics, t, topic_descriptor, next_topic);
            free(t);
            return 0;
        }
    }

    return -1; 
}

/**
 * Find a topic given the topic list
 * 
 * @param topic the topic to search
 * @param topics points to the list of topics
 * @return 1 if found, 0 if not
 */
int topic_list_is_in_list(char *topic, struct slisthead_topics *topics)
{
    struct topic_descriptor *t;

    SLIST_FOREACH(t, topics, next_topic) {
        if (strcmp(t->topic, topic) == 0) return 1;
    }

    return 0;
}