#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>


// Define return codes for errors and success
#define ERROR_USAGE 62
#define ERROR_FILE_EXISTS 63
#define ERROR_FILE_CREATION 64
#define SUCCESS 0


// Main function that handles input, checks for errors, and creates the files
int main(int argc, char *argv[])
{
    // Ensure that exactly one argument is provided
    if (argc != 2)
    {
        printf("Usage:  init <filename>\n");
        return ERROR_USAGE;
    }


    char *path_copy = malloc(strlen(argv[1]) + 1);
    strcpy(path_copy, argv[1]);
    char *token = strtok(path_copy, "/");
    char *next_token = strtok(NULL, "/");
    char *string = malloc(strlen(token) + 1);
    strcat(string, token);


    while (next_token != NULL)
    {
        mkdir(string, 0777);


        token = next_token;
        next_token = strtok(NULL, "/");


        char *string_copy = malloc(strlen(string));
        strcpy(string_copy, string);


        string = malloc(strlen(string) + strlen(token) + 2);
        strcat(string, string_copy);
        strcat(string, "/");
        strcat(string, token);


        free(string_copy);
    }
   


    char bank_file[1024];
    char atm_file[1024];


    snprintf(bank_file, sizeof(bank_file), "%s.bank", string);
    snprintf(atm_file, sizeof(bank_file), "%s.atm", string);


    struct stat st;
    if (stat(bank_file, &st) == 0 || stat(atm_file, &st) == 0)
    {
        printf("Error: one of the files already exists\n");
        return ERROR_FILE_EXISTS;
    }


    // Create the .bank file with the O_EXCL flag to avoid race conditions
    int bank_fd = open(bank_file, O_WRONLY | O_CREAT | O_EXCL, 0666);
    if (bank_fd == -1)
    {
        perror("Errordkfsjl");
        return ERROR_FILE_CREATION;
    }


    int atm_fd = open(atm_file, O_WRONLY | O_CREAT | O_EXCL, 0666);
    if (atm_fd == -1)
    {
        perror("Error creating initialization files");
        return ERROR_FILE_CREATION;
    }


    // Write a placeholder content to the .bank file
    const char *bank_content = "This is a placeholder for the bank's configuration.\n";
    write(bank_fd, bank_content, strlen(bank_content));
    close(bank_fd); // Close the bank file after writing


    // Write a placeholder content to the .atm file
    const char *atm_content = "This is a placeholder for the ATM's configuration.\n";
    write(atm_fd, atm_content, strlen(atm_content));
    close(atm_fd); // Close the ATM file after writing




    free(string);
    free(path_copy);


    return SUCCESS;
}
