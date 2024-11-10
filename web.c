#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <pthread.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <dirent.h>

//====================================================================
//====================================================================

void send_response(int client_socket, const char *header, const char *content) {
    char response[1024];
    snprintf(response, sizeof(response), "%s\n\n%s", header, content);
    send(client_socket, response, strlen(response), 0);
}

//====================================================================
//====================================================================

void *client(void* cfd) {

    // variables
    int client_fd = *(int*)cfd;
    char buffer[1024] = {0};
    int buffsize = sizeof(buffer);

    // receive client info
    int bytes_recv = recv(client_fd, buffer, buffsize - 1, 0);
    if(bytes_recv < 0) {
        perror("failed to receive\n");
        return NULL;
    }
    buffer[bytes_recv] = '\0';

    // parse GET request path
    char method[1024];
    char path[1024];
    sscanf(buffer, "%s %s", method, path);

    // remove the leading '/'
    char* directory = path + 1;
    
    // check to see if directory exists (using stat)
    struct stat dir_info;
    if(stat(directory, &dir_info) == 0 && S_ISDIR(dir_info.st_mode)) {
        DIR* dir = opendir(directory);
        if(dir == NULL) {
            send_response(client_fd, "HTTP/1.1 Internal Service Error", "unable to read directory");
        } else {
            struct dirent *entry;
            char content[1024] = " ";
            while((entry = readdir(dir)) != NULL) {
                strcat(content, entry->d_name);
                strcat(content, "\n");
            }
            closedir(dir);
            send_response(client_fd, "HTTP/1.1 200 OK", content);
        }
    } else {
        send_response(client_fd, "HTTP/1.1 404 Not Found", "directory does not exist\n");
    }

    return NULL;
}

//====================================================================
//====================================================================

void create_server(int port) {

    // variables
    int server_fd;
    int client_fd;
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    pthread_t tid;

    // create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd == 0) {
        perror("failed to create socket\n");
        return;
    }

    // set addr values
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    // bind
    if(bind(server_fd, (struct sockaddr*)&addr, addrlen)) {
        perror("failed to bind\n");
        return;
    }

    // listen
    if(listen(server_fd, 10) < 0) {
        perror("failed to listen\n");
        return;
    }

    printf("server is listening on port %d\n", port);
    // handle the client input, with multithreading
    while(server_fd > 0) {
        client_fd = accept(server_fd, (struct sockaddr*)&addr, &addrlen);
        if(client_fd < 0) {
            perror("failed to accept\n");
            break;
        }
        pthread_create(&tid, NULL, client, (void*)&client_fd);
        pthread_join(tid, NULL);
    }

    close(server_fd);
    close(client_fd);

}

//====================================================================
//====================================================================

int main(int argc, char* argv[]) {

    // variables 
    int port = 80;

    // get port, if specified
    if(argc > 2) {
        if(strcmp(argv[1], "-p") != 0) {
            printf("invalid flag\n");
            exit(1);
        } else
            port = atoi(argv[2]);
    }

    // create server
    create_server(port);

    return 0;
}

//====================================================================
//====================================================================