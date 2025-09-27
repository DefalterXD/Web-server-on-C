#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hashtable.h"
#include "cache.h"

/**
 * Allocate a cache entry
 */
struct cache_entry *alloc_entry(char *path, char *content_type, void *content, int content_length, time_t time)
{
    ///////////////////
    // IMPLEMENT ME! //
    ///////////////////

    struct cache_entry *new_entry = malloc(sizeof(*new_entry));
    if (!new_entry)
    {
        return NULL;
    }

    new_entry->path = strdup(path);
    new_entry->content_type = strdup(content_type);
    new_entry->content_length = content_length;
    new_entry->content = malloc(content_length);
    memcpy(new_entry->content, content, content_length);
    new_entry->created_at = time;

    new_entry->prev = NULL;
    new_entry->next = NULL;

    return new_entry;
}

/**
 * Deallocate a cache entry
 */
void free_entry(struct cache_entry *entry)
{
    ///////////////////
    // IMPLEMENT ME! //
    ///////////////////

    if (!entry) { return; }
    free(entry->path);
    free(entry->content_type);
    free(entry->content);
    free(entry);
}

/**
 * Insert a cache entry at the head of the linked list
 */
void dllist_insert_head(struct cache *cache, struct cache_entry *ce)
{
    // Insert at the head of the list
    if (cache->head == NULL)
    {
        cache->head = cache->tail = ce;
        ce->prev = ce->next = NULL;
    }
    else
    {
        cache->head->prev = ce;
        ce->next = cache->head;
        ce->prev = NULL;
        cache->head = ce;
    }
}

/**
 * Move a cache entry to the head of the list
 */
void dllist_move_to_head(struct cache *cache, struct cache_entry *ce)
{
    if (ce != cache->head)
    {
        if (ce == cache->tail)
        {
            // We're the tail
            cache->tail = ce->prev;
            cache->tail->next = NULL;
        }
        else
        {
            // We're neither the head nor the tail
            ce->prev->next = ce->next;
            ce->next->prev = ce->prev;
        }

        ce->next = cache->head;
        cache->head->prev = ce;
        ce->prev = NULL;
        cache->head = ce;
    }
}

/**
 * Removes the tail from the list and returns it
 *
 * NOTE: does not deallocate the tail
 */
struct cache_entry *dllist_remove_tail(struct cache *cache)
{
    struct cache_entry *oldtail = cache->tail;

    cache->tail = oldtail->prev;
    cache->tail->next = NULL;

    cache->cur_size--;

    return oldtail;
}

/**
 * Create a new cache
 *
 * max_size: maximum number of entries in the cache
 * hashsize: hashtable size (0 for default)
 */
struct cache *cache_create(int max_size, int hashsize)
{
    ///////////////////
    // IMPLEMENT ME! //
    ///////////////////

    struct cache *new_cache = malloc(sizeof(*new_cache));
    if (!new_cache)
    {
        return NULL;
    }

    // CREATE hashtable inside cache index
    new_cache->index = hashtable_create(128, NULL);
    new_cache->head = NULL;
    new_cache->tail = NULL;

    new_cache->max_size = max_size;
    new_cache->cur_size = hashsize;

    return new_cache;
}

void cache_free(struct cache *cache)
{
    struct cache_entry *cur_entry = cache->head;

    hashtable_destroy(cache->index);

    while (cur_entry != NULL)
    {
        struct cache_entry *next_entry = cur_entry->next;

        free_entry(cur_entry);

        cur_entry = next_entry;
    }

    free(cache);
}

/**
 * Store an entry in the cache
 *
 * This will also remove the least-recently-used items as necessary.
 *
 * NOTE: doesn't check for duplicate cache entries
 */
void cache_put(struct cache *cache, char *path, char *content_type, void *content, int content_length, time_t time)
{
    ///////////////////
    // IMPLEMENT ME! //
    ///////////////////

    // INIT new cache entry
    struct cache_entry *new_entry = alloc_entry(path, content_type, content, content_length, time);
    if (!new_entry)
    {
        return;
    }

    // INSERT cache_entry into the head of dllist
    dllist_insert_head(cache, new_entry);

    // PUT cache entry inside hashtable
    hashtable_put(cache->index, path, new_entry);
    // INCREMENT current cache size
    cache->cur_size++;
    // IF cache max size greater than current size
    if (cache->cur_size > cache->max_size)
    {
        // INIT tail entry from cache
        struct cache_entry *tail_entry = hashtable_get(cache->index, cache->tail->path);
        // THEN delist tail cache entry
        dllist_remove_tail(cache);
        // DELETE from hashtable
        hashtable_delete(cache->index, tail_entry->path);
        // FREE entry from memory
        free_entry(tail_entry);
    }
}

/**
 * Retrieve an entry from the cache
 */
struct cache_entry *cache_get(struct cache *cache, char *path)
{
    ///////////////////
    // IMPLEMENT ME! //
    ///////////////////

    // INIT founded cache entry
    struct cache_entry *founded_entry = hashtable_get(cache->index, path);
    if (!founded_entry)
    {
        return NULL;
    }

    // MOVE founded entry to head
    dllist_move_to_head(cache, founded_entry);

    // RETURN founded cache entry
    return founded_entry;
}

/**
* Remove stale cache entry
*/
void remove_entry(struct cache *cache, struct cache_entry *cache_entry)
{
    // IF cache has only one entry
    if (cache_entry->next == NULL)
    {
        cache->head = NULL;
        cache->tail = NULL;
        free_entry(cache_entry);
    }
    else
    {
        cache->head = cache_entry->next;
        cache_entry->next->prev = NULL;
        free_entry(cache_entry);
    }
    cache->cur_size--;
}