#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/socket.h>
#include <pthread.h>
#include <regex.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <ctype.h>

const char *get_mime_type(const char *filename) {
    const char *ext = strrchr(filename, '.');
    if (!ext || ext == filename) {
        return "application/octet-stream";  // Default binary type if no extension
    }

    ext++;  // Move past the dot

    // Map common file extensions to MIME types
    if (strcmp(ext, "html") == 0 || strcmp(ext, "htm") == 0) return "text/html";
    if (strcmp(ext, "jpg") == 0 || strcmp(ext, "jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, "png") == 0) return "image/png";
    if (strcmp(ext, "gif") == 0) return "image/gif";
    if (strcmp(ext, "css") == 0) return "text/css";
    if (strcmp(ext, "js") == 0) return "application/javascript";
    if (strcmp(ext, "json") == 0) return "application/json";
    if (strcmp(ext, "pdf") == 0) return "application/pdf";
    if (strcmp(ext, "txt") == 0) return "text/plain";
    if (strcmp(ext, "xml") == 0) return "application/xml";
    if (strcmp(ext, "zip") == 0) return "application/zip";
    if (strcmp(ext, "mp3") == 0) return "audio/mpeg";
    if (strcmp(ext, "mp4") == 0) return "video/mp4";
    // Add more mappings as needed

    return "application/octet-stream";  // Default binary type for unknown extensions
}

char *url_decode(const char *str) {
    int len = strlen(str);
    char *decoded = malloc(len + 1);  // Allocate memory for decoded string
    if (!decoded) return NULL;        // Handle memory allocation failure

    char *p = decoded;
    for (int i = 0; i < len; i++) {
        if (str[i] == '%' && i + 2 < len && isxdigit(str[i + 1]) && isxdigit(str[i + 2])) {
            char hex[3] = { str[i + 1], str[i + 2], '\0' };
            *p++ = (char) strtol(hex, NULL, 16);
            i += 2;
        } else if (str[i] == '+') {
            *p++ = ' ';
        } else {
            *p++ = str[i];
        }
    }
    *p = '\0';
    return decoded;
}

const char *get_file_extension(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if (!dot || dot == filename) {
        return "";  // No extension found
    }
    return dot + 1;  // Return extension without the dot
}

void create_http_server(char* file_name, char* file_ext, char* response, size_t *resplen, int buffsize) {

    const char* mime_type = get_mime_type(file_ext);
    char* header = (char*)malloc(sizeof(buffsize));
    snprintf(header, buffsize, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\n\r\n", mime_type);

    int file_fd = open(file_name, O_RDONLY);
    if(file_fd == -1) {
        snprintf(response, buffsize, "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\r\n404 Not Found");
        *resplen = strlen(response);
        return;
    }

    struct stat file_stat;
    fstat(file_fd, &file_stat);
    off_t file_size = file_stat.st_size;

    *resplen = 0;
    memcpy(response, header, strlen(header));
    *resplen += strlen(header);

    ssize_t bytes_read;
    while((bytes_read = read(file_fd, response + *resplen, buffsize - *resplen)) > 0) 
        *resplen += bytes_read;
    
    close(file_fd);

}

void *handle_client(void *sfd) {

    // variables
    int server_fd = *(int*)sfd;
    int client_fd;
    struct sockaddr_in c_addr;
    socklen_t addrlen = sizeof(c_addr);
    char buffer[1024];

    ssize_t bytes = recv(client_fd, buffer, sizeof(buffer), 0);

    regex_t regex;
    regcomp(&regex, "GET /([^ ]*) HTTP/1", REG_EXTENDED);
    regmatch_t matches[2];

    if(regexec(&regex, buffer, 2, matches, 0) == 0) {
        buffer[matches[1].rm_eo] = '\0';
        const char *url_file_name = buffer + matches[1].rm_so;
        char * file_name = url_decode(url_file_name);

        char file_ext[32];
        strcpy(file_ext, get_file_extension(file_name));

        char *response = (char*)malloc(sizeof(buffer) * 2 * sizeof(char));
        size_t resplen;
        create_http_server(file_name, file_ext, response, &resplen, sizeof(buffer));

        send(client_fd, response, resplen, 0);

    }

    regfree(&regex);
    close(client_fd);

    return NULL;
}


void create_server(int port) {

    // variables
    int server_fd;
    struct sockaddr_in addr;
    pthread_t tid;

    // create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd < 0) {
        perror("failed to create socket\n");
        return;
    }

    // set addr values
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    // bind 
    if(bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("failed to bind\n");
        return;
    }

    // listen 
    if(listen(server_fd, 10 < 0)) {
        perror("failed to listen\n");
        return;
    }

    pthread_create(&tid, NULL, handle_client, (void*)&server_fd);
    pthread_join(tid, NULL);

    close(server_fd);
}


int main(int argc, char* argv[]) {

    int port = 80;

    // assign port, if given
    if(argc > 2) {
        if(strcmp(argv[1], "-p") != 0) {
            printf("invalid flag\n");
            exit(1);
        } else {
            port = atoi(argv[2]);
        }
    }

    create_server(port);

    return 0;
}