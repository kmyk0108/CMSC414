#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "encryption/enc.h"

#define ERROR_USAGE 62
#define ERROR_FILE_EXISTS 63
#define ERROR_FILE_CREATION 64
#define SUCCESS 0

// argv[0]: <path1>/init 
// argv[1]: <path2>/<init-fname>
int main(int argc, char *argv[])
{
    // Ensure that exactly one argument is provided
    if (argc != 2)
    {
        printf("Usage:  init <filename>\n");
        return ERROR_USAGE;
    }

    // Create the directories specified in <path2> if they don't exist
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

    // Create the <init-fname>.bank and <init-fname>.atm files
    char bank_file[1024];
    char atm_file[1024];

    snprintf(bank_file, sizeof(bank_file), "%s.bank", argv[1]);
    snprintf(atm_file, sizeof(atm_file), "%s.atm", argv[1]);

    // Remove leading slash if it exists
    if (bank_file[0] == '/')
    {
        memmove(bank_file, bank_file + 1, strlen(bank_file));
    }

    if (atm_file[0] == '/')
    {
        memmove(atm_file, atm_file + 1, strlen(atm_file));
    }

    // Check if files already exist
    struct stat st;
    if (stat(bank_file, &st) == 0 || stat(atm_file, &st) == 0)
    {
        printf("Error: one of the files already exists\n");
        return ERROR_FILE_EXISTS;
    }

    FILE *bank_fp = fopen(bank_file, "wb");
    if (!bank_fp)
    {
        perror("Error creating initialization files");
        return ERROR_FILE_CREATION;
    }

    FILE *atm_fp = fopen(atm_file, "wb");
    if (!atm_fp)
    {
        perror("Error creating initialization files");
        fclose(bank_fp);
        return ERROR_FILE_CREATION;
    }

    // Generate keys
    unsigned char aes_pin_key[AES_KEY_SIZE];
    unsigned char aes_message_key[AES_KEY_SIZE];

    if (!generate_rand_bytes(AES_KEY_SIZE, aes_pin_key) ||
        !generate_rand_bytes(AES_KEY_SIZE, aes_message_key))
    {
        fclose(bank_fp);
        fclose(atm_fp);
        return 1;
    }

    // Write keys to the .bank file
    if (fwrite(aes_pin_key, 1, AES_KEY_SIZE, bank_fp) != AES_KEY_SIZE ||
        fwrite(aes_message_key, 1, AES_KEY_SIZE, bank_fp) != AES_KEY_SIZE)
    {
        perror("Error writing to .bank file");
        fclose(bank_fp);
        fclose(atm_fp);
        return ERROR_FILE_CREATION;
    }

    // Write keys to the .atm file
    if (fwrite(aes_pin_key, 1, AES_KEY_SIZE, atm_fp) != AES_KEY_SIZE ||
        fwrite(aes_message_key, 1, AES_KEY_SIZE, atm_fp) != AES_KEY_SIZE)
    {
        perror("Error writing to .atm file");
        fclose(bank_fp);
        fclose(atm_fp);
        return ERROR_FILE_CREATION;
    }

    memset(aes_pin_key, 0, AES_KEY_SIZE);
    memset(aes_message_key, 0, AES_KEY_SIZE);

    fclose(bank_fp);
    fclose(atm_fp);

    printf("Successfully initialized bank state\n");
    return SUCCESS;
}
