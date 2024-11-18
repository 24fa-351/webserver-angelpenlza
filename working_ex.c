#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 1024

// Function to read the contents of an HTML file
char *read_file(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Could not open file");
        return NULL;
    }

    // Find the file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file);

    // Allocate memory for the file contents
    char *content = (char *)malloc(file_size + 1);
    if (content == NULL) {
        perror("Memory allocation failed");
        fclose(file);
        return NULL;
    }

    // Read the file contents and close the file
    fread(content, 1, file_size, file);
    content[file_size] = '\0';  // Null-terminate the string
    fclose(file);

    return content;
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = {0};

    // Read the HTML content from the file
    char *html_content = read_file("index.html");
    if (html_content == NULL) {
        fprintf(stderr, "Failed to load HTML file.\n");
        exit(EXIT_FAILURE);
    }

    // Create the HTTP response header with a placeholder for Content-Length
    char http_header[BUFFER_SIZE];
    snprintf(http_header, sizeof(http_header),
             "HTTP/1.1 200 OK\n"
             "Content-Type: text/html\n"
             "Content-Length: %ld\n\n",
             strlen(html_content));

    // Combine the header and the HTML content into the response
    char *response = malloc(strlen(http_header) + strlen(html_content) + 1);
    if (response == NULL) {
        perror("Memory allocation failed");
        free(html_content);
        exit(EXIT_FAILURE);
    }
    strcpy(response, http_header);
    strcat(response, html_content);

    // 1. Create the socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        free(html_content);
        free(response);
        exit(EXIT_FAILURE);
    }

    // 2. Bind the socket to the address and port
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        close(server_fd);
        free(html_content);
        free(response);
        exit(EXIT_FAILURE);
    }

    // 3. Listen for incoming connections
    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        close(server_fd);
        free(html_content);
        free(response);
        exit(EXIT_FAILURE);
    }
    printf("Server running on http://localhost:%d\n", PORT);

    // 4. Accept and handle incoming connections
    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
            perror("Accept failed");
            close(server_fd);
            free(html_content);
            free(response);
            exit(EXIT_FAILURE);
        }

        // Read the incoming request (discarding actual content here)
        read(new_socket, buffer, BUFFER_SIZE);
        printf("Received request:\n%s\n", buffer);

        // Send the HTML response
        write(new_socket, response, strlen(response));
        close(new_socket);
    }

    // Clean up
    close(server_fd);
    free(html_content);
    free(response);
    return 0;
}
