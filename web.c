//  File:       web.c
//  Purpose:    - create a web server, which listens on port 80
//              - capable of changing port by running with -p followed by port number
//              - type /static/image/image_name.png to receive binary file of image
//              - type /stats to return an HTML page listing:
//                  - number of requests received so far
//                  - total number of received bytes
//                  - total number of sent bytes
//              - type /calc/a/b which returns an HTML page showing the sum of a and b
//=========================================================================================
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define BUFF_SIZE 1024
#define INDEX_PAGE "static/help.html"
#define LINE_LENGTH 256
#define PAGE_MAX 5000

static int requests = 0;
static ssize_t bytes_sent = 0;
static ssize_t bytes_received = 0; 


// return valid html page upon valid directory, 400 Bad Request otherwise
char* readFile(char* file_name) {
    char* page = NULL;
    char body[PAGE_MAX];
    ssize_t bytes; 

    char* error =
        "HTTP/1.1 400 Bad Request\r\n"
        "Content-Type: text/html\r\n"
        "Connection: close\r\n\r\n"
        "<html><body><h1>400 Bad Request</h1>"
        "<p>page not found</p>"
        "go to 127.0.0.1:8080/help.html for support<p>"
        "</body></html>";
    const char* header = 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Connection: close\r\n\r\n";

    int page_fd = open(file_name, O_RDONLY);
    if(page_fd < 0) return error;
    bytes = read(page_fd, body, sizeof(body));
    page = (char*)malloc(sizeof(char) * strlen(header) + sizeof(char) * bytes);
    strcpy(page, header);
    strcat(page, body);

    close(page_fd);
    return page;
}

// make sure inputs both from telnet and from browser work
char* interpret_input(char input[]) {
    input[strcspn(input, "\n")] = '\0';
    ssize_t path_len = strcspn(input, "H") - strcspn(input, "/") - 1;
    if(path_len < 0) return NULL;
    char* path = (char*)malloc(sizeof(char) * path_len);
    strncpy(path, input + strcspn(input, "/") + 1, path_len);
    path[strlen(path) - 1] = '\0';
    return path; 
}

// check for a certain directory at the beginning of the directory
int checkFor(char* word, char* dir) {
    ssize_t len = strlen(word) + 1;
    char* temp = (char*)malloc(sizeof(char) * len);
    strncpy(temp, dir, len - 1);
    temp[len] = '\0';
    if(strcmp(temp, word) == 0) {
        free(temp);
        return 0;
    } else {
        free(temp);
        return 1; 
    }
}

void send_image(int client_fd, char* dir) {
    char* header = 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: image/png\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n\r\n";
    char* img_header = (char*)malloc(strlen(header));
    int img_fd = open(dir, O_RDONLY);
    if(img_fd < 0) return;
    size_t bytes = lseek(img_fd, 0, SEEK_END);

    snprintf(img_header, sizeof(img_header), header, strlen(header) + bytes);
    send(client_fd, header, strlen(header), 0);

    lseek(img_fd, 0, SEEK_SET);
    char* body = (char*)malloc(bytes);
    ssize_t total_read = 0;
    while (total_read < bytes) {
        ssize_t result = read(img_fd, body + total_read, bytes - total_read);
        if (result < 0) {
            free(body);
            return;
        } total_read += result;
    }
    bytes_sent += send(client_fd, body, bytes, 0);
    free(body);
    close(client_fd);
}

char* stats(int requests, int bytes_received, int bytes_sent) {
    char* page = readFile("stats/stats.html");
    char* stats_page = (char*)malloc(sizeof(char) * strlen(page));
    snprintf(stats_page, strlen(page), page, requests, bytes_received, bytes_sent);
    return stats_page;
}

char* calc(char* dir) {
    char* page = readFile("calc/calc.html");
    char* calc_page = (char*)malloc(sizeof(char) * strlen(page));
    int* nums = (int*)malloc(sizeof(int) * 3); 

    calc_page = strtok(dir, "/");
    if(calc_page == NULL) return NULL;
    calc_page = strtok(NULL, "/");
    if(calc_page == NULL) return NULL;
    nums[0] = atoi(calc_page);
    calc_page = strtok(NULL, "/");
    if(calc_page == NULL) return NULL;
    nums[1] = atoi(calc_page);
    nums[2] = nums[0] + nums[1];

    snprintf(calc_page, strlen(page), page, nums[0], nums[1], nums[2]);
    return calc_page; 
}

// multithread function, the "main" for the request 
void* handle_request(void* file_desc) {
    requests++;
    int client_fd = *(int*)file_desc;
    char buff[BUFF_SIZE]; 
    char* page;
    char* dir;
    
    int bytes = recv(client_fd, buff, sizeof(buff) - 1, 0);
    bytes_received += bytes; 
    if(bytes < 0) {
        perror("failed to receive\n");
        return NULL;
    } buff[bytes] = '\0';

    dir = interpret_input(buff);
    if(dir == NULL) page = readFile(INDEX_PAGE);
    else if(checkFor("static", dir) == 0) {
        send_image(client_fd, dir);
        return NULL;
    }
    else if(strcmp(dir, "stats") == 0) page = stats(requests, bytes_received, bytes_sent);
    else if(checkFor("calc", dir) == 0) page = calc(dir);
    else page = readFile(dir);

    if(page == NULL) page = readFile(INDEX_PAGE);
    bytes = send(client_fd, page, strlen(page), 0);
    bytes_sent += bytes;
    close(client_fd);
    return NULL;
}

int main(int argc, char* argv[]) {
    pthread_t tid;
    int port = 80;
    int server_fd, client_fd;
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);

    if(argc > 2) {
        if(strcmp(argv[1], "-p") != 0) {
            printf("invalid flag\n");
            return 1;
        } else {
            printf("new port: %d\n", atoi(argv[2]));
            port = atoi(argv[2]);
        }
    }

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd < 0) {
        perror("error creating server\n");
        return 1;
    }

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if(bind(server_fd, (struct sockaddr*)&addr, addrlen) < 0) {
        perror("error binding\n");
        return 1;
    }

    if(listen(server_fd, 10) < 0) {
        perror("error listening\n");
        return 1; 
    }

    printf("server running on port %d\n", port);
    while(server_fd > 0) {
        client_fd = accept(server_fd, (struct sockaddr*)&addr, &addrlen);
        if(client_fd < 0) {
            perror("error accepting\n");
            return 1; 
        }
        pthread_create(&tid, NULL, handle_request, (void*)&client_fd);
        pthread_join(tid, NULL);
    }

    close(server_fd);

    return 0;
}