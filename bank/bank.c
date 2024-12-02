#include "bank.h"
#include "ports.h"
#include "encryption/enc.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>
#include <fcntl.h>
#include <openssl/conf.h>
#include <openssl/evp.h>
#include <openssl/err.h>

#define ERROR_FILE_CREATION 64

#define AES_BLOCK_SIZE 16
#define AES_KEY_SIZE 32
#define IV_SIZE 16
#define TAG_SIZE 12

#define MAX_USERNAME_LEN 250
#define MAX_INT_BYTES 11

Bank *bank_create(char *bank_file)
{
    Bank *bank = (Bank *)malloc(sizeof(Bank));
    if (bank == NULL)
    {
        perror("Could not allocate Bank");
        exit(1);
    }

    // Set up the network state
    bank->sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    bzero(&bank->rtr_addr, sizeof(bank->rtr_addr));
    bank->rtr_addr.sin_family = AF_INET;
    bank->rtr_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    bank->rtr_addr.sin_port = htons(ROUTER_PORT);

    bzero(&bank->bank_addr, sizeof(bank->bank_addr));
    bank->bank_addr.sin_family = AF_INET;
    bank->bank_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    bank->bank_addr.sin_port = htons(BANK_PORT);
    bind(bank->sockfd, (struct sockaddr *)&bank->bank_addr, sizeof(bank->bank_addr));

    // Set up the protocol state
    bank->bank_file = bank_file;
    bank->users = list_create(10);

    return bank;
}

void bank_free(Bank *bank)
{
    if (bank != NULL)
    {
        close(bank->sockfd);
        free(bank);
    }
}

ssize_t bank_send(Bank *bank, char *data, size_t data_len)
{
    // Returns the number of bytes sent; negative on error
    return sendto(bank->sockfd, data, data_len, 0,
                  (struct sockaddr *)&bank->rtr_addr, sizeof(bank->rtr_addr));
}

ssize_t bank_recv(Bank *bank, char *data, size_t max_data_len)
{
    // Returns the number of bytes received; negative on error
    return recvfrom(bank->sockfd, data, max_data_len, 0, NULL, NULL);
}

int extract_pin_key(char *bank_file, unsigned char *key)
{
    // Ensure that exactly one argument is provided
    FILE *bank_fd = fopen(bank_file, "rb");
    if (bank_fd == NULL)
    {
        perror("Error opening bank initialization file");
        return 1;
    }
    size_t bytes_read = fread(key, sizeof(char), AES_KEY_SIZE, bank_fd);
    if (bytes_read != AES_KEY_SIZE)
    {
        perror("Error reading file");
        fclose(bank_fd);
        return 1;
    }
    fclose(bank_fd);
    return 0;
}

int valid_username(char *username)
{
    if (strlen(username) > MAX_USERNAME_LEN)
    {
        return 0;
    }

    int i = 0;
    while (username[i] != '\0')
    {
        if (!isalpha(username[i++]))
        {
            return 0;
        }
    }
    return 1;
}

int valid_pin(char *pin)
{
    if (strlen(pin) != 4)
    {
        return 0;
    }
    return isdigit(pin[0]) && isdigit(pin[1]) && isdigit(pin[2]) && isdigit(pin[3]);
}

int valid_balance(char *balance_str)
{
    for (const char *p = balance_str; *p != '\0'; p++)
    {
        if (!isdigit(*p))
        {
            return 0; // Non-numeric character found
        }
    }

    long balance = strtol(balance_str, NULL, 10); // Base 10 conversion
    if (balance < 0 || balance > INT_MAX)
    {
        return 0; // Out of range
    }
    return 1;
}

int check_create_user(char *command, char *username, char *pin, char *init_balance)
{
    return strcmp(command, "create-user") == 0 && valid_username(username) && valid_pin(pin) && valid_balance(init_balance);
}

void create_card(Bank *bank, char *username, unsigned char *plaintext_pin)
{
    // printf("AA\n");
    // try to create a .card file for the user
    size_t card_file_size = strlen(username) + strlen(".card") + 1;
    char *card_file = (char *)malloc(card_file_size);

    if (card_file == NULL)
    {
        fprintf(stderr, "Error: Memory allocation failed.\n");
        return; // Handle memory allocation failure
    }

    // printf("BB\n");

    // Safely copy the username into the buffer
    strncpy(card_file, username, card_file_size - 1);
    card_file[card_file_size - 1] = '\0'; // Ensure null termination

    // Safely append ".card" to the username
    strncat(card_file, ".card", card_file_size - strlen(card_file) - 1);

    FILE *file = fopen(card_file, "wb");
    if (file == NULL)
    {
        printf("Error creating card file for %s\n", username);
        free(card_file);
        return;
    }
    else
    {
        unsigned char pin_key[AES_KEY_SIZE];
        extract_pin_key(bank->bank_file, pin_key);
        unsigned char iv[IV_SIZE];
        generate_rand_bytes(IV_SIZE, iv);

        // int block_size = calculate_block_size(strlen((char *)plaintext_pin));

        unsigned char encrypted_pin[AES_BLOCK_SIZE];

        int c_len = encrypt(plaintext_pin, strlen((char *)plaintext_pin), pin_key, iv, encrypted_pin);
        // printf("Ciphertext length: %d\n", c_len);
        // printf("Ciphertext is:\n");
        // BIO_dump_fp(stdout, (const char *)encrypted_pin, ciphertext_len);

        // write the encrypted pin and IV into .card file
        size_t written = fwrite(encrypted_pin, 1, AES_BLOCK_SIZE, file);
        if (written != AES_BLOCK_SIZE)
        {
            printf("Error\n");
            fclose(file);
            free(card_file);
            return;
        }
        written = fwrite(iv, 1, IV_SIZE, file);
        if (written != IV_SIZE)
        {
            printf("Error\n");
            fclose(file);
            free(card_file);
            return;
        }

        fclose(file);
        free(card_file);
        printf("Created user %s\n", username);
        return;
    }
}

int check_deposit(char *command, char *username, char *amount)
{
    return strcmp(command, "deposit") == 0 && valid_username(username) && valid_balance(amount);
}

// void withdraw(Bank *bank, char *command, char *sendline)
// {
//     char *username = strtok(command, " ");
//     char *withdraw_command = strtok(NULL, " ");
//     char *amount = strtok(NULL, " ");
//     // printf("Amount: %s\n", amount);

//     if (!valid_balance(amount))
//     {
//         printf("HI\n");
//     }

//     if (strcmp(withdraw_command, "withdraw") == 0 && amount != NULL && strtok(NULL, " ") == NULL && valid_balance(amount))
//     {
//         User *user = get_user(bank, username);
//         int int_amount = atoi(amount);
//         if (int_amount > user->balance)
//         {
//             sprintf(sendline, "Insufficient funds\n");
//         }
//         else
//         {
//             user->balance -= int_amount;
//             sprintf(sendline, "$%s dispensed\n", amount);
//         }
//     }
//     else
//     {
//         sprintf(sendline, "Usage: withdraw <amt>\n");
//     }
//     return;
// }

void bank_process_local_command(Bank *bank, char *command, size_t len)
{
    char command_copy[1000];

    // Ensure null-termination
    if (len >= sizeof(command_copy))
    {
        fprintf(stderr, "Error: command too long\n");
        return;
    }

    strncpy(command_copy, command, sizeof(command_copy) - 1);
    command_copy[sizeof(command_copy) - 1] = '\0';

    // remove \n at end of command
    command_copy[strlen(command_copy) - 1] = '\0';

    if (strstr(command, "create-user"))
    {
        char *args[4]; // Expected arguments: command, username, pin, amount
        char *token = strtok(command_copy, " ");
        int arg_count = 0;

        while (token != NULL)
        {
            if (arg_count < 4)
            {
                args[arg_count++] = token;
            }
            else
            { // too many args
                printf("Usage: create-user <user-name> <pin> <balance>\n");
                return;
            }
            token = strtok(NULL, " ");
        }

        // too few args
        if (arg_count < 4)
        {
            printf("Usage: create-user <user-name> <pin> <balance>\n");
            return;
        }

        char *command = args[0];
        char *username = args[1];
        char *pin = args[2];
        char *init_balance = args[3];

        if (!check_create_user(command, username, pin, init_balance))
        {
            printf("Usage: create-user <user-name> <pin> <balance>\n");
            return;
        }

        if (list_find(bank->users, username) != NULL)
        {
            printf("Error: user %s already exists\n", username);
            return;
        }

        int balance_int = atoi(init_balance);
        int user_len = strlen(username);

        // add the new user to the users list
        char *bal_str = calloc(MAX_INT_BYTES, 1);
        char *usr_str = calloc(user_len + 1, 1);
        sprintf(bal_str, "%d", balance_int); // conversion to string for list
        strncpy(usr_str, username, user_len);
        list_add(bank->users, usr_str, bal_str);
        usr_str = NULL;
        bal_str = NULL;
        // list_add(bank->users, username, (void *)(intptr_t)init_balance);

        create_card(bank, username, (unsigned char *)pin);
    }
    else if (strstr(command, "deposit"))
    {
        char *args[3]; // Expected arguments: command, username, amount
        char *token = strtok(command_copy, " ");
        int arg_count = 0;

        while (token != NULL)
        {
            if (arg_count < 3)
            {
                args[arg_count++] = token;
            }
            else
            { // too many args
                printf("Usage:  deposit <user-name> <amt>\n");
                return;
            }
            token = strtok(NULL, " ");
        }

        // too few args
        if (arg_count < 3)
        {
            printf("Usage:  deposit <user-name> <amt>\n");
            return;
        }

        char *command = args[0];
        char *username = args[1];
        char *amount = args[2];

        if (!check_deposit(command, username, amount))
        {
            printf("Usage:  deposit <user-name> <amt>\n");
            return;
        }

        char *balance = list_find(bank->users, username);
        if (balance == NULL)
        {
            printf("No such user\n");
            return;
        }

        // since we previously checked that amount is a valid integer, convert it to one
        int balance_int = atoi(balance);
        int deposit_amt = atoi(amount);

        // check that deposit doesn't cause integer overflow
        if (balance_int > INT_MAX - deposit_amt || deposit_amt > INT_MAX - balance_int)
        {
            printf("Too rich for this program\n");
            return;
        }

        char *amt_str = calloc(MAX_INT_BYTES, 1);
        char *username_for_list;
        sprintf(amt_str, "%d", balance_int + deposit_amt);

        list_del(bank->users, username);

        username_for_list = calloc(MAX_USERNAME_LEN + 1, 1);
        strncpy(username_for_list, username, strlen(username));

        list_add(bank->users, username_for_list, amt_str);
        printf("$%d added to %s's account\n", deposit_amt, username);
        return;
    }
    else if (strstr(command, "balance"))
    {
        char *args[2]; // Expected arguments: command, username
        char *token = strtok(command_copy, " ");
        int arg_count = 0;

        while (token != NULL)
        {
            if (arg_count < 2)
            {
                args[arg_count++] = token;
            }
            else
            { // too many args
                printf("Usage:  balance <user-name>\n");
                return;
            }
            token = strtok(NULL, " ");
        }

        // too few args
        if (arg_count < 2)
        {
            printf("Usage:  balance <user-name>\n");
            return;
        }

        char *command = args[0];
        char *username = args[1];

        if (strcmp(command, "balance") != 0 || !valid_username(username))
        {
            printf("Usage:  balance <user-name>\n");
            return;
        }

        char *balance = list_find(bank->users, username);
        if (balance == NULL)
        {
            printf("No such user\n");
            return;
        }
        printf("$%s\n", (char *)balance);
        return;
    }

    else
    {
        printf("Invalid command\n");
    }

    return;
}

void bank_process_remote_command(Bank *bank, char *command, size_t len)
{
    char sendline[10000];

    

    // bank_send(bank, sendline, strlen(sendline));
    return;
}
