#include <string.h>
#include <sys/select.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "bank.h"
#include "ports.h"
#include "encryption/enc.h"

#define ERROR_USAGE 62
#define ERROR_FILE_OPEN 64

static const char prompt[] = "BANK: ";

/* 
    Decrypt an AES-256-GCM encoded message sent to the bank. If the extracted authentication tag differs from that
    created by gcm_encrypt(), the program will terminate.
*/
int decrypt_message(Bank *bank, char *command, size_t len, char *plaintext_buffer, size_t buffer_size) {
    char received_data[10000];
    memcpy(received_data, command, len);

    int length_ciphertext;
    int offset = 0;
    memcpy(&length_ciphertext, received_data + offset, sizeof(int)); // Get length of ciphertext
    offset += sizeof(int);

    // Extract the ciphertext, iv, and tag
    unsigned char *ciphertext = malloc(length_ciphertext);
    memcpy(ciphertext, received_data + offset, length_ciphertext);
    offset += length_ciphertext;

    unsigned char *iv = malloc(GCM_IV_SIZE);
    memcpy(iv, received_data + offset, GCM_IV_SIZE);
    offset += GCM_IV_SIZE;

    unsigned char *tag = malloc(TAG_SIZE);
    memcpy(tag, received_data + offset, TAG_SIZE);

    // Retrieve the AES message key from .bank
    unsigned char msg_key[AES_KEY_SIZE];
    extract_msg_key(bank->bank_file, msg_key);

    // Decrypt the message
    int p_len = gcm_decrypt(ciphertext, length_ciphertext, NULL, 0, tag, msg_key, iv, GCM_IV_SIZE, (unsigned char *)plaintext_buffer);

    // Handle decryption errors
    if (p_len < 0) {
        free(ciphertext);  
        free(iv);  
        free(tag);  
        return -1;
    }

    plaintext_buffer[p_len] = '\0';  

    free(ciphertext);  
    free(iv);  
    free(tag);  

    return p_len;
}



int main(int argc, char **argv)
{
    int n;
    char sendline[10000];
    char recvline[10000];

    if (argc != 2)
    {
        printf("Usage:  init <filename>\n");
        return ERROR_USAGE;
    }

    char *bank_file = argv[1];
    char *bank_ext = strstr(argv[1], ".");

    if (bank_ext == NULL || strcmp(bank_ext, ".bank") != 0)
    {
        perror("access denied");
        return ERROR_FILE_OPEN;
    }

    // If first char of the bank file is a slash, remove it
    if (bank_file[0] == '/') {
        bank_file++;
    }

    // Ensure that exactly one argument is provided
    FILE *bank_fd = fopen(bank_file, "r");
    if (bank_fd == NULL)
    {
        perror("Error opening bank initialization file");
        return ERROR_FILE_OPEN;
    }
    fclose(bank_fd);

    Bank * bank = bank_create(bank_file);

    printf("%s", prompt);
    fflush(stdout);

    while (1)
    {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(0, &fds);
        FD_SET(bank->sockfd, &fds);
        select(bank->sockfd + 1, &fds, NULL, NULL, NULL);

        if (FD_ISSET(0, &fds))
        {
            fgets(sendline, 10000, stdin);
            bank_process_local_command(bank, sendline, strlen(sendline));
            printf("%s", prompt);
            fflush(stdout);
        }
        else if (FD_ISSET(bank->sockfd, &fds))
        {
            n = bank_recv(bank, recvline, 10000);
            char plaintext_buf[1000];
            if (decrypt_message(bank, recvline, n, plaintext_buf, 1000) == -1) {
                bank_free(bank);
                exit(-1);
            }
            bank_process_remote_command(bank, plaintext_buf, n);
        }
    }
    bank_free(bank);
    
    return EXIT_SUCCESS;
}
