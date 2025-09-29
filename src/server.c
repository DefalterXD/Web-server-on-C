/**
 * webserver.c -- A webserver written in C
 *
 * Test with curl (if you don't have it, install it):
 *
 *    curl -D - http://localhost:3490/
 *    curl -D - http://localhost:3490/d20
 *    curl -D - http://localhost:3490/date
 *
 * You can also test the above URLs in your browser! They should work!
 *
 * Posting Data:
 *
 *    curl -D - -X POST -H 'Content-Type: text/plain' -d 'Hello, sample data!' http://localhost:3490/save
 *
 * (Posting data is harder to test from a browser.)
 * 
 *  With the guidance of ChatGPT and mostly guidance (his code was horrible or doesn't make sense)
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/file.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <pthread.h>
#include "net.h"
#include "file.h"
#include "mime.h"
#include "cache.h"

#define PORT "3490" // the port users will be connecting to

#define SERVER_FILES "./serverfiles"
#define SERVER_ROOT "./serverroot"
#define SERVER_ASSETS "./assets"

/**
 * Getting date for HTTP response
 * */
static inline void populate_date_string(char *buf, size_t max_len)
{
    time_t rawtime = time(NULL);
    struct tm *info = localtime(&rawtime);
    strftime(buf, max_len, "%a %b %d %H:%M:%S %Z %Y", info);
}

/**
 * Send an HTTP response
 *
 * header:       "HTTP/1.1 404 NOT FOUND" or "HTTP/1.1 200 OK", etc.
 * content_type: "text/plain", etc.
 * body:         the data to send.
 *
 * Return the value from the send() function.
 */
int send_response(int fd, char *header, char *content_type, void *body, int content_length)
{
    const int max_response_size = 262144;
    char response[max_response_size];

    // Build HTTP response and store it in response

    // GET time for the request
    char response_format[50];
    populate_date_string(response_format, sizeof(response_format));

    // INIT length of header response
    int header_length = snprintf(response, max_response_size,
                                 "%s\n"
                                 "Date: %s\n"
                                 "Connection: close\n"
                                 "Content-Length: %i\n"
                                 "Content-Type: %s\n"
                                 "\n",
                                 header, response_format, content_length, content_type);

    // APPEND body into response buffer after header
    memcpy(response + header_length, body, content_length);

    // INIT response_length of body and header
    int response_length = header_length + content_length;

    // Send it all!
    int rv = send(fd, response, response_length, 0);

    if (rv < 0)
    {
        perror("send");
    }

    return rv;
}

/**
 * Send a /d20 endpoint response
 */
void get_d20(int fd)
{
    // Generate a random number between 1 and 20 inclusive

    // INIT max number
    int max = 20;
    // INIT min number
    int min = 1;
    // INIT randomised number between 1 and 20
    int random_number = rand() % (max - min + 1) + min;

    // INIT buff_number
    char buff_number[4];

    // GET random number into the buffer
    sprintf(buff_number, "%d", random_number);

    // INIT byte_counter from buffer
    int byte_length = 0;

    // FOR every number inside the buffer
    for (int i = 0; buff_number[i] != '\0'; i++)
    {
        // COUNTER UP byte_length
        byte_length++;
    }
    // ENDFOR

    // Use send_response() to send it back as text/plain data
    send_response(fd, "HTTP/1.1 200 OK", "text/plain", buff_number, byte_length);
}

/**
 * Send a 404 response
 */
void resp_404(int fd)
{
    char filepath[4096];
    struct file_data *filedata;
    char *mime_type;

    // Fetch the 404.html file
    snprintf(filepath, sizeof filepath, "%s/404.html", SERVER_FILES);
    filedata = file_load(filepath);

    if (filedata == NULL)
    {
        fprintf(stderr, "cannot find system 404 file\n");
        send_response(fd, "HTTP/1.1 404 NOT FOUND", "text/plain", "", 0);
    }
    else
    {
        mime_type = mime_type_get(filepath);
        send_response(fd, "HTTP/1.1 404 NOT FOUND", mime_type, filedata->data, filedata->size);
        file_free(filedata);
    }
}

/**
 * Read and return a file from disk or cache
 */
void get_file(int fd, struct cache *cache, char *request_path, char *filepath)
{
    // INIT file attributes
    struct file_data *filedata;
    char *mime_type;
    time_t cache_date_created;

    filedata = file_load(filepath);

    // IF file exist in root
    if (filedata != NULL)
    {
        mime_type = mime_type_get(filepath);
        time(&cache_date_created);
        // PUT file into cache
        cache_put(cache, request_path, mime_type, filedata->data, filedata->size, cache_date_created);
        // THEN send that file to client
        send_response(fd, "HTTP/1.1 200 OK", mime_type, filedata->data, filedata->size);
    }
    else
    {
        resp_404(fd);
    }
}

/**
 * Search for the end of the HTTP header
 *
 * "Newlines" in HTTP can be \r\n (carriage return followed by newline) or \n
 * (newline) or \r (carriage return).
 */
char *find_start_of_body(char *request)
{
    char *body = strstr(request, "\r\n\r\n");

    strlcpy(body, body + 4, strlen(body));
    strlcat(body, "\n", strlen(body) + 2);

    return body;
}

/**
 * 
 * Handle save file for body from post request
 *
 **/
void save_post(int postfd, char *body)
{
    // INIT file attributes
    char jsonpath[2048];
    struct file_data *filedata;
    char *mime_type = NULL;

    snprintf(jsonpath, sizeof jsonpath, "%s/post.json", SERVER_ROOT);

    filedata = file_load(jsonpath);

    
    if (filedata != NULL)
    {
        if (mime_type == NULL) mime_type = mime_type_get(jsonpath);
        send_response(postfd, "HTTP/1.1 200 OK", mime_type, filedata->data, filedata->size);
        file_free(filedata);
    }

    // INIT file descriptor for saving file
    postfd = open("post_data.txt", O_CREAT | O_WRONLY | O_APPEND, 0644);
    if (postfd < 0)
    {
        perror("r1");
        exit(1);
    }

    // WRITE into the file
    write(postfd, body, strlen(body));
    
    close(postfd);
}

/**
 * Handle HTTP request and send response
 */
void handle_http_request(int fd, struct cache *cache)
{
    const int request_buffer_size = 65536; // 64K
    char request[request_buffer_size];
    // RESET request for remaining values
    memset(request, 0, request_buffer_size);
    // INIT filepath
    char filepath[4096];
    // INIT buffer for filepath stats
    struct stat buffer;
    // INIT current time of requst
    time_t request_created_time;

    // Read request
    int bytes_recvd = recv(fd, request, request_buffer_size - 1, 0);

    if (bytes_recvd < 0)
    {
        perror("recv");
        return;
    }

    // Read the first two components of the first line of the request

    // INIT variable for http method
    char http_method[10];

    // INIT variable for file path
    char request_route[2000];

    // ASSIGN http method into variable
    sscanf(request, "%s %s", http_method, request_route);

    // ASSIGN full path from disk
    snprintf(filepath, sizeof filepath, "%s%s", SERVER_ROOT, request_route);

    // GET OS info stats to buffer
    if (stat(filepath, &buffer) != 0)
    {
        snprintf(filepath, sizeof filepath, "%s%s", SERVER_ASSETS, request_route);
    }
    else
    {
        // IF path is a directory
        if (S_ISDIR(buffer.st_mode))
        {
            // THEN normalize requested path to automatic index.html
            if (request_route[strlen(request_route) - 1] == '/')
            {
                strlcat(request_route, "index.html", sizeof(request_route));
            }
            else
            {
                strlcat(request_route, "/index.html", sizeof(request_route));
            }
            // ASSIGN normalize path with index.html
            snprintf(filepath, sizeof filepath, "%s%s", SERVER_ROOT, request_route);
        }
    }
    time(&request_created_time);

    // If GET, handle the get endpoints

    // IF method is GET
    if (strcmp(http_method, "GET") == 0)
    {
        //    Check if it's /d20 and handle that special case
        // IF url path is /d20
        if (strcmp(request_route, "/d20") == 0)
        {
            get_d20(fd);
        }
        //    Otherwise serve the requested file by calling get_file()
        else
        {
            // INIT cached file from requested file_route
            struct cache_entry *founded_file = cache_get(cache, request_route);
            // IF file is found from cache_entry
            if (founded_file != NULL)
            {
                // INIT difference in time between entry and request
                int time_difference = difftime(request_created_time, founded_file->created_at);
                // IF cache entry was stale for 1 minute
                if (time_difference > 60)
                {
                    // THEN remove that entry and put a new one
                    remove_entry(cache, founded_file);
                    get_file(fd, cache, request_route, filepath);
                }
                else
                {
                    // THEN SERVE that file from cache
                    send_response(fd, "HTTP/1.1 200 OK", founded_file->content_type, founded_file->content, founded_file->content_length);
                }
            }
            // ELSE
            else
            {
                // SERVE that file from disk
                get_file(fd, cache, request_route, filepath);
            }
        }
    }
    // (Stretch) If POST, handle the post request
    else if (strcmp(http_method, "POST") == 0)
    {
        // SEND requests into find_start_of_body
        char *request_body = find_start_of_body(request);

        // SAVE data from body
        save_post(fd, request_body);
    }
}

/**
 * 
 * Thread handler for each request
 * 
 */ 
void *server_thread(void *arg) {
    thread_config_t *config = (thread_config_t*)arg;
    int sockfd = config->sockfd;
    struct cache *cache = config->cache;
    free(config);

    unsigned long id = (unsigned long)pthread_self();
    printf("Thread %lu created to handle connection with socket %d\n", id, sockfd);
    handle_http_request(sockfd, cache);
    printf("Thread %lu is done\n", id);
    close(sockfd);
    return 0;
}


/**
 * Main
 */
int main(void)
{
    int newfd;                          // listen on sock_fd, new connection on newfd
    struct sockaddr_storage their_addr; // connector's address information
    char s[INET6_ADDRSTRLEN];

    struct cache *cache = cache_create(10, 0);

    // Get a listening socket
    int listenfd = get_listener_socket(PORT);

    if (listenfd < 0)
    {
        fprintf(stderr, "webserver: fatal error getting listening socket\n");
        exit(1);
    }

    printf("webserver: waiting for connections on port %s...\n", PORT);

    // This is the main loop that accepts incoming connections and
    // responds to the request. The main parent process
    // then goes back to waiting for new connections.

    while (1)
    {
        socklen_t sin_size = sizeof their_addr;
        // Parent process will block on the accept() call until someone
        // makes a new connection:
        newfd = accept(listenfd, (struct sockaddr *)&their_addr, &sin_size);
        if (newfd == -1)
        {
            perror("accept");
            continue;
        }
        
        // Print out a message that we got the connection
        inet_ntop(their_addr.ss_family,
                  get_in_addr((struct sockaddr *)&their_addr),
                  s, sizeof s);
        printf("server: got connection from %s\n", s);
        pthread_t thread;

        thread_config_t *config = (thread_config_t*)malloc(sizeof(*config));
        if(!config)
        {
            perror("OOM");
            continue;
        }
        config->sockfd = newfd;
        config->cache = cache;
        pthread_create(&thread, NULL, server_thread, config);

        // newfd is a new socket descriptor for the new connection.
        // listenfd is still listening for new connections.

        pthread_detach(thread);
    }

    // Unreachable code

    return 0;
}
