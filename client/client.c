#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <signal.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include "../settings.h"                         //deal with this file for settings, might be necessary to move it if you decide to move it before compiling
char **tokenizer(char *temp1, char *delimiters); //this function tokenises the given input
int total = 0, untilnow = 0;                     //variables for progress printing
float percentage = 0;

//signal handler for SIGPIP
void sigpip_handler(int signum)
{

    fprintf(stderr, "The Server quit in between.. Sed.. Try Putting the server up or Try changing to different one..Exiting for now..\n\n");
    exit(EXIT_FAILURE);
}

//CTRL-C Signal Handler
void CTRL_C_handler(int signum)
{

    fprintf(stderr, "\n\nReceived Interrupt to exit: Complete and use exit command...\n\n");
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

//this function checks if file exists or not and does the necessary stuff... It tells to skip or not to for the file loop..
int skipTransfer(char *fname)
{
    int found = 0;        //the flag variable
    int skip = 0;         //another flag
    DIR *di;              // directory structure
    struct dirent *entry; //dirent structure
    di = opendir("./");   // open the directory
    while ((entry = readdir(di)) != NULL)
    {
        if (strcmp(entry->d_name, fname) == 0)
        {
            found = 1; //setting flag
        }
    }
    closedir(di); // close dir
    if (found)    // if found
    {
        char buffer[BUFFERSIZE];
    prompt: //we may come back here because of an idiot...
        memset(&buffer, '\0', sizeof(buffer));
        printf("File already exists in client Directory. Do you want to overwrite the file (yes/no) ?\n");
        memset(&buffer, '\0', sizeof(buffer));
        fgets(buffer, BUFFERSIZE, stdin);
        buffer[strcspn(buffer, "\n")] = 0;
        if (strcmp(buffer, "yes") == 0)
        {
            skip = 0;
            printf("Overwriting selected: Will be overwritten in case file exists on server.. and is received..\n\n");
        }
        else if (strcmp(buffer, "no") == 0)
        {
            skip = 1;
            printf("Skipping this file...\n\n");
        }
        else
        {
            fprintf(stderr, "What do you mean bruh? It should be (yes/no)... \n\n");
            goto prompt;
        }
    }
    return skip;
}

// The function that receives file from server and writes...
int receive_file(int socket, char *fname, int fileSize)
{
    char buffer[BUFFERSIZE];
    char fpath[BUFFERSIZE];
    strcpy(fpath, "./");
    strcat(fpath, fname);
    FILE *out_file = fopen(fpath, "wb"); //open the file to write with overwrite
    if (out_file == NULL)
    {
        fprintf(stderr, "File %s : cannot create file on client side.\n", fname);
        perror("File Creation: ");
        memset(&buffer, '\0', sizeof(buffer));
        strcpy(buffer, "no");
        send(socket, buffer, BUFFERSIZE, 0);
        memset(&buffer, '\0', sizeof(buffer));
    }

    else
    {
        memset(&buffer, '\0', sizeof(buffer));
        strcpy(buffer, "yes");
        send(socket, buffer, BUFFERSIZE, 0);
        memset(&buffer, '\0', sizeof(buffer));
        int out_file_block_sz = 0;
        memset(&buffer, '\0', sizeof(buffer));
        total = fileSize;
        untilnow = 0;
        int j = 0;
        while ((out_file_block_sz = recv(socket, buffer, BUFFERSIZE, 0)) > 0)
        {
            if (j == 0 && strcmp(buffer, "CANCEL") == 0) //we send the server a "yes" but it might have received it corrupted... It might tell us to cancel.. Meh..
            {
                fprintf(stderr, "\nUnexpected Response from Server..\n\n");
                break;
            }
            j = 1;
            int write_sz = fwrite(buffer, sizeof(char), out_file_block_sz, out_file);
            untilnow += out_file_block_sz;
            progress();

            if (write_sz < out_file_block_sz)
            {
                fprintf(stderr, "\nFile write failed. File may be Corrupted\n\n");
                break;
            }

            memset(&buffer, '\0', sizeof(buffer));
            if (untilnow == fileSize)
            {
                break;
            }
        }
        if (out_file_block_sz < 0)
        {
            perror("In read(): ");
            fprintf(stderr, "File %s might be corrupted..\n\n", fname);
            fclose(out_file);
            return -1;
        }
        printf("\nReceived the file %s from Server\n\n", fname);
        fclose(out_file);
    }
    return 0;
}
void requestFile(int client_sock, char *fname)
{
    char buffer[BUFFERSIZE];
    memset(&buffer, '\0', sizeof(buffer));
    recv(client_sock, buffer, BUFFERSIZE, 0); //receive File request feedback...

    if (strcmp(buffer, "CANCEL") == 0) //if server said to cancel request...
    {
        printf("Something weird happened at server...Server said to cancel this request for now... : ( Try again later for file %s.\n\n", fname);
        return;
    }
    if (strcmp(buffer, "-1") != 0) // the file exist at server if server returns size >= 0 for request.. Otherwise -1...
    {
        int fileSize = atoi(buffer); // the server returns file size.. so store it in fileSize for progress...
        receive_file(client_sock, fname, fileSize);
        memset(&buffer, '\0', sizeof(buffer));
    }
    else
    {
        printf("File %s not found on server... Skipping...\n", fname);
    }
}

// Go through the list of files and ask the Server for each file...
void getFiles(char **file, int client_sock)
{
    char *filename;
    char *command;
    for (int i = 1; file[i] != NULL; i++)
    {
        printf("File: %s :\n", file[i]);
        if (skipTransfer(file[i])) //should we skip..?
            continue;
        filename = (char *)malloc(BUFFERSIZE);
        command = (char *)malloc(BUFFERSIZE);
        strcpy(command, "get ");
        strcat(command, file[i]);
        strcpy(filename, file[i]);
        send(client_sock, command, BUFFERSIZE, 0); // tell the server get <file>
        requestFile(client_sock, filename);        // go to requestFile()
        free(filename);
        free(command);
    }
}

//this starts execution of command
int startExecution(char *input, int client_sock)
{
    char **tokenisedInput = tokenizer(input, "\n\t\r "); //tokenize the input
    int argumentCount = 0;                               // count the arguments
    for (int i = 0; tokenisedInput[i] != NULL; i++)
    {
        argumentCount++;
    }
    if (argumentCount == 0) // we shouldn't have come here in the first place, anyways...
    {
        return 0;
    }
    if (strcmp(tokenisedInput[0], "get") == 0) //if the command is get
    {
        if (argumentCount == 1)
        {
            fprintf(stderr, "Files to fetch not given...Invalid Syntax... Try again.\n\n");
            return 0;
        }
        printf("Starting execution of get...\n\n");
        getFiles(tokenisedInput, client_sock); //get the files specified by tokenised input from server...
        return 0;                              //we continue the CLI
    }
    if (strcmp(tokenisedInput[0], "exit") == 0) // if it is exit
    {
        if (argumentCount > 1)
        {
            printf("exit syntax is wrong. If you wish to quit- type exit.\n");
            return 0;
        }
        printf("\nExiting..\n");
        send(client_sock, tokenisedInput[0], sizeof(tokenisedInput[0]), 0); // tell the server that we are exiting so that it can take more connections..
        return 1;                                                           // exit the CLI
    }
    fprintf(stderr, "\nInvalid Command.\n\n");      //what we got is an INVALID Command
    for (int i = 0; tokenisedInput[i] != NULL; i++) //memory management, good practices
    {
        free(tokenisedInput[i]);
    }
    if (tokenisedInput != NULL)
        free(tokenisedInput);
    return 0;
}

// main function

int main(int argc, char const *argv[])
{
    signal(SIGPIPE, sigpip_handler);
    signal(SIGINT, CTRL_C_handler);
    char *input = (char *)malloc(INPUT_LENGTH);
    int sizein;
    size_t size = INPUT_LENGTH;
    struct sockaddr_in address;
    int client_sock, valRead = 0;
    struct sockaddr_in serv_addr;
    memset(&serv_addr, '0', sizeof(serv_addr));              //memset to 0 to avoid rubbish
    if ((client_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) //SOCK_STREAM for TCP, 0 for default Protocol, AF_INET is ipv4 Protocol Family
    {
        perror("Socket:");
        fprintf(stderr, "\n Socket creation error :(\n");
        exit(EXIT_FAILURE);
    }
    //From this point client_sock is our connection point, it is a file descriptor pointing to the point of communication...
    serv_addr.sin_family = AF_INET;                                // set the server's family of Protocol as ipv4
    serv_addr.sin_port = htons(PORT);                              //convert from host byte order to network byte order
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) //converts 2nd argument string src to the family of argument 1  af as network address structure and stores in 3rd argument
    {
        perror("inet_pton:");
        fprintf(stderr, "\nInvalid address\n");
        exit(EXIT_FAILURE);
    }
    //try again: Useful if the server didn't start yet- We will try again...

tryagain:
    printf("Trying to connect....\n");
    if (connect(client_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) // connect to the server address,specified...to client
    {
        perror("Client trying to connect:");
        fprintf(stderr, "\nConnection Failed :(. Enter 1 to try again.. Otherwise 0..\n");
        int a = 0;
        fflush(stdin);
        scanf("%d", &a);
        fflush(stdin);
        if (a == 1)
        {
            fflush(stdin);
            goto tryagain;
        }
        exit(EXIT_FAILURE);
    }

    printf("\nConnected to Server: Success... Starting CLI\n\n");
    printf("USAGE: \n");
    printf("get <file1> <file2> ...  : gets the files specified from server.\n");
    printf("exit : exits the CLI\n\n\n");

    // Simulation of CLI
    while (1)
    {
        printf("client ~> ");
        sizein = getline(&input, &size, stdin);
        if (sizein < 0)
        {
            printf("Received Exit Signal CTRL-D.. Client Exiting\n.");
            exit(EXIT_SUCCESS);
        }
        int isQuit = startExecution(input, client_sock);
        if (isQuit)
        {
            break;
        }
    }

    exit(EXIT_SUCCESS);
}

//tokenizer for CLI
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
    tokens = (char **)malloc(10000);

    char *temp = strtok(str, delimiters);

    int start = 0;

    while (temp)
    {
        tokens[start] = (char *)malloc(strlen(temp) + 1);
        strcpy(tokens[start], temp);
        temp = strtok(NULL, delimiters);
        start++;
    }

    tokens[start] = NULL;

    return tokens;
}