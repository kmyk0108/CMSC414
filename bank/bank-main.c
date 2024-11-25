/*
 * The main program for the Bank.
 *
 * You are free to change this as necessary.
 */

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

#define ERROR_USAGE 62
<<<<<<< HEAD
#define ERROR_FILE_EXISTS 63
#define ERROR_FILE_CREATION 64
#define SUCCESS 0
=======
#define ERROR_FILE_OPEN 64
>>>>>>> 8ef981c3d52724e196d9f19fdc5e985f8e9a842b

static const char prompt[] = "BANK: ";

int main(int argc, char **argv)
{
    int n;
    char sendline[1000];
    char recvline[1000];

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

    char * filename = strdup(argv[1]);
    Bank * bank = bank_create(filename);

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
            bank_process_remote_command(bank, recvline, n);
        }
    }

    return EXIT_SUCCESS;
}
