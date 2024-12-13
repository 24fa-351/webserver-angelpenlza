#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>

char* get_directory(const char* request) {

    return NULL;
}

char* get_page(const char* fname) {

    // open the file
    FILE* page = fopen(fname, "r");
    if(page == NULL) {
        printf("error reading file\n");
        fclose(page);
        return NULL;
    }

    // get the size of the page
    fseek(page, 0, SEEK_END);
    int size = ftell(page);
    rewind(page);

    // read the page, copy its contents, add null terminator
    char* content = (char*)malloc(size + 1);
    if(content == NULL) {
        fclose(page);
        return NULL;
    }
    fread(content, 1, size, page);
    content[size] = '\0';

    // create header
    char header[80];
    snprintf(header, sizeof(header), 
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "Connection: close\r\n"
            "Content-Length: %d\r\n"
            "\r\n", size);
    
    char* request = (char*)malloc(sizeof(char)*(74 + size)); // size of page + size of content
    strcpy(request, header);
    strcat(request, content);

    // end
    free(content);
    fclose(page);
    return request;
}

void* handle_request(void* cfd) {

    // variables
    int client_fd = *(int*)cfd;
    char* request = get_page("static/home.html");
    char user_input[1024];

    // handle invalid request
    if(request == NULL) {
        request = 
            "HTML/1.1 404 Not Found\r\n"
            "Content-Type: text/html\r\n"
            "Connection: close\r\n"
            "Content-Length: 13\r\n"
            "\r\n"
            "<h1>404</h1>";
    }

    read(client_fd, user_input, 1024);
    printf("user sent: %s\n", user_input);

    write(client_fd, request, strlen(request));
    close(client_fd);
    return NULL;
}

int main(int argc, char* argv[]) {

    // variables 
    int server_fd, client_fd;
    int port = 80;
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    pthread_t tid;

    // update port, if specified
    if(argc > 2) {
        if(strcmp(argv[1], "-p") != 0) {
            printf("invalid flag\n");
            exit(1);
        } else {
            port = atoi(argv[2]);
        }
    }

    // create the server socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd < 0) {
        perror("error creating server\n");
        exit(1);
    }

    // config addr values
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    // bind
    if(bind(server_fd, (struct sockaddr*)&addr, addrlen) < 0) {
        perror("error binding\n");
        exit(1);
    }

    // start listening
    if(listen(server_fd, 3) < 0) {
        perror("error listening\n");
        exit(1);
    }

    printf("server running on port %d\n", port);
    // accept requests
    while(server_fd > 0) {
        client_fd = accept(server_fd, (struct sockaddr*)&addr, &addrlen);
        if(client_fd < 0) {
            perror("error accepting\n");
            exit(1);
        }
        pthread_create(&tid, NULL, handle_request, (void*)&client_fd);
        pthread_join(tid, NULL);
    }

    // close the server
    close(server_fd);
    
    return 0;
}