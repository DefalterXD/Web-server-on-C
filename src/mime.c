#include <string.h>
#include <ctype.h>
#include "mime.h"
#include "hashtable.h"

#define DEFAULT_MIME_TYPE "application/octet-stream"

struct hashtable *ht = NULL;

/**
 * Lowercase a string
 */
char *strlower(char *s)
{
    for (char *p = s; *p != '\0'; p++)
    {
        *p = tolower(*p);
    }

    return s;
}

/**
 * Return a MIME type for a given filename
 */
char *mime_type_get(char *filename)
{
    if (ht == NULL)
    {
        ht = hashtable_create(0, NULL);
        hashtable_put(ht, "html", "text/html");
        hashtable_put(ht, "htm", "text/html");
        hashtable_put(ht, "jpeg", "image/jpg");
        hashtable_put(ht, "jpg", "image/jpg");
        hashtable_put(ht, "css", "text/css");
        hashtable_put(ht, "js", "application/javascript");
        hashtable_put(ht, "json", "application/json");
        hashtable_put(ht, "txt", "text/plain");
        hashtable_put(ht, "gif", "image/gif");
        hashtable_put(ht, "png", "image/png");
    }


    char *ext = strrchr(filename, '.');

    if (ext == NULL)
    {
        return DEFAULT_MIME_TYPE;
    }

    ext++;

    strlower(ext);

    char *file_type = hashtable_get(ht, ext);
    if (!file_type)
    {
        return DEFAULT_MIME_TYPE;
    }

    return file_type;
}