#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include "../settings.h"
int total = 0, untilnow = 0; //variables for progress printing
float percentage = 0;

//check if file exists
int fileExists(char *fname)
{
    int found = 0; //flag
    DIR *di;
    struct dirent *dir;
    di = opendir("./");
    while ((dir = readdir(di)) != NULL)
    {
        if (strcmp(dir->d_name, fname) == 0)
        {
            found = 1;
        }
    }
    closedir(di);
    return found;
}

//function that is called to print progress
void progress()
{
    char str[100];
    if (total == 0)
    {
        return;
    }
    percentage = ((double)untilnow) / total * 100;
    percentage = (int)(percentage * 100 + 0.5);
    percentage = (float)percentage / 100;
    sprintf(str, "\rProgress %.2f %%", percentage);
    printf("%s", str);
}

//sigpipe handler
void sigpip_handler(int signum)
{
    fprintf(stderr, "The client quit in between.. Moving on to next client... Errors might have occured\n\n");
}
//CTRL-C handler
void CTRL_C_handler(int signum)
{
    fprintf(stdout, "\n\nServer going down...Clients have to wait until back up...\n\n");
    exit(EXIT_SUCCESS);
}
//sends the file
int send_file(int socket, char *fname)
{
    char buffer[BUFFERSIZE] = {0};
    char fpath[BUFFERSIZE];
    strcpy(fpath, "./");
    strcat(fpath, fname);
    FILE *file = fopen(fpath, "r");
    if (file == NULL)
    {
        perror("File: opening");
        fprintf(stderr, "File %s not found.\n", fname);
        strcpy(buffer, "CANCEL");
        send(socket, buffer, BUFFERSIZE, 0);
        return -1;
    }
    memset(&buffer, '\0', sizeof(buffer));

    int fs_block_sz;
    untilnow = 0;
    while ((fs_block_sz = fread(buffer, sizeof(char), BUFFERSIZE, file)) > 0)
    {
        if (send(socket, buffer, fs_block_sz, 0) < 0)
        {
            fprintf(stderr, "Failed to send file %s. (errno = %d) File might be corrupted at Client...\n", fname, errno);
            break;
        }
        untilnow += fs_block_sz;
        progress();
        memset(&buffer, '\0', sizeof(buffer));
    }
    progress();
    printf("\nFTP Completed\n\n");
    fclose(file);
    return 0;
}
void sendFile(int accept_sockfd, char *fname)
{
    char buffer[BUFFERSIZE];
    if (fileExists(fname))
    {
        printf("File %s exists on server\n", fname);
        memset(&buffer, '\0', sizeof(buffer));
        struct stat statbuf;
        stat(fname, &statbuf);
        int fileSize = statbuf.st_size;
        sprintf(buffer, "%d", fileSize);
        total = fileSize;
        send(accept_sockfd, buffer, BUFFERSIZE, 0); //send file size..
        recv(accept_sockfd, buffer, BUFFERSIZE, 0); //wait for confirmation to send file..
        if (strcmp(buffer, "yes") == 0)
        {
            send_file(accept_sockfd, fname);
        }
        else if (strcmp(buffer, "no") == 0)
        {
            printf("Recieved no:\n Skipping..\n");
        }
        else
        { //server didn't get a confirmation as yes.. wierd.. cancel the operation...
            memset(&buffer, '\0', sizeof(buffer));
            strcpy(buffer, "CANCEL");
            send(accept_sockfd, buffer, BUFFERSIZE, 0);
            printf("File transfer for file %s aborted\n", fname);
        }
    }
    else
    {
        memset(&buffer, '\0', sizeof(buffer));
        strcpy(buffer, "-1");
        printf("File requested: %s not available on server.\n", fname);
        send(accept_sockfd, buffer, BUFFERSIZE, 0);
    }
}
char **tokenizer(char *temp1, char *delimiters)
{
    char str[(int)INPUT_LENGTH + 1];
    strcpy(str, temp1);
    int index, i, j;

    index = 0;
    while (str[index] == ' ' || str[index] == '\t' || str[index] == '\n')
    {
        index++;
    }

    if (index != 0)
    {
        i = 0;
        while (str[i + index] != '\0')
        {
            str[i] = str[i + index];
            i++;
        }
        str[i] = '\0';
    }
    int last = -1;
    for (i = 0; str[i] != '\0'; i++)
    {
        if (str[i] != ' ' && str[i] != '\t' && str[i] != '\n')
        {
            last = i;
        }
    }
    str[last + 1] = '\0';
    char **tokens;
    tokens = malloc(10000);

    char *temp = strtok(str, delimiters);

    int start = 0;

    while (temp)
    {
        tokens[start] = malloc(strlen(temp) + 1);
        strcpy(tokens[start], temp);
        temp = strtok(NULL, delimiters);
        start++;
    }

    tokens[start] = NULL;

    return tokens;
}
int main(int argc, char const *argv[])
{

    signal(SIGPIPE, sigpip_handler);
    signal(SIGINT, CTRL_C_handler);
    int server_sock, new_socket;
    struct sockaddr_in client_address;
    int accept_sockfd;
    int opt = 1;
    int addr_len = sizeof(client_address);
    char buffer[BUFFERSIZE];
    memset(&buffer, '\0', sizeof(buffer));
    // Creating socket file descriptor
    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket creation failed: ");
        exit(EXIT_FAILURE);
    }
    // This is to lose the pesky "Address already in use" error message
    if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) // SOL_SOCKET is the socket layer itself
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    client_address.sin_family = AF_INET;         // Address family. For IPv6, it's AF_INET6. 29 others exist like AF_UNIX etc.
    client_address.sin_addr.s_addr = INADDR_ANY; // Accept connections from any IP address - listens from all interfaces.
    client_address.sin_port = htons(PORT);       // Server port to open. Htons converts to Big Endian - Left to Right. RTL is Little Endian

    // Forcefully attaching socket to the port 8080
    if (bind(server_sock, (struct sockaddr *)&client_address,
             sizeof(client_address)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Port bind is done. You want to wait for incoming connections and handle them in some way.
    // The process is two step: first you listen(), then you accept()
    if (listen(server_sock, MAX_CONN) < 0) // MAX_CONN is the maximum size of queue - connections you haven't accepted
    {
        perror("listen: ");
        exit(EXIT_FAILURE);
    }
    while (1)
    {
        printf("Waiting for a connection...\n");
        if ((accept_sockfd = accept(server_sock, (struct sockaddr *)&client_address, (socklen_t *)&addr_len)) < 0)
        {
            perror("Accept:");
            continue;
        }
        printf("Connected\n");
        char **buffertoken;
        while (1)
        {

            if (recv(accept_sockfd, buffer, BUFFERSIZE, 0) < 0)
            {
                perror("recv in main():");
                break;
            }
            buffertoken = tokenizer(buffer, "\r\t\n ");
            if (buffertoken[0] == NULL)
            {
                printf("Disconnecting...\n\n");
                break;
            }

            if (strcmp(buffertoken[0], "exit") == 0)
            {
                printf("Disconnecting from this client ...\n\n");
                break;
            }
            if (strcmp(buffertoken[0], "get") == 0)
            {
                char *fname = buffertoken[1];
                printf("Client requested file: %s\n", fname);
                sendFile(accept_sockfd, fname);
            }
            else
            {
                printf("Invalid Command from Client\n\n");
                memset(&buffer, '\0', sizeof(buffer));
                strcpy(buffer, "CANCEL");
                send(accept_sockfd, buffer, BUFFERSIZE, 0);
            }
            memset(&buffer, '\0', sizeof(buffer));
        }
    }
    return 0;
}
