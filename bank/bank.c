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
    bank->user_list_head = NULL;

    return bank;
}

// Free the login attempts list
void free_users(Bank *bank)
{
    User *current = bank->user_list_head;
    while (current != NULL)
    {
        User *tmp = current;
        current = current->next;
        free(tmp);
    }
    bank->user_list_head = NULL;
}

void bank_free(Bank *bank)
{
    if (bank != NULL)
    {
        close(bank->sockfd);
        free_users(bank);
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

// bank->users functions
User *get_user(Bank *bank, char *username)
{
    User *current = bank->user_list_head;

    while (current != NULL)
    {
        if (strcmp(current->username, username) == 0)
        {
            return current;
        }
        current = current->next;
    }

    return NULL;
}

void create_user(Bank *bank, char *username, char *balance)
{
    // Add new user to head of list
    User *new_user = malloc(sizeof(User));
    if (!new_user)
    {
        perror("malloc failed");
        exit(EXIT_FAILURE);
    }
    strncpy(new_user->username, username, sizeof(new_user->username) - 1);
    new_user->username[sizeof(new_user->username) - 1] = '\0';
    new_user->balance = atoi(balance);
    new_user->next = bank->user_list_head;
    bank->user_list_head = new_user;
    return;
}

// Functions to extract the AES key used to encrypt pins (first 32 bytes of .bank and .atm)
//                  and the AES key used to encrypt messages (last 32 bytes of .bank and .atm)
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

int extract_msg_key(char *bank_file, unsigned char *key)
{
    // Ensure that exactly one argument is provided
    FILE *bank_fd = fopen(bank_file, "rb");
    if (bank_fd == NULL)
    {
        perror("Error opening bank initialization file");
        return 1;
    }

    if (fseek(bank_fd, AES_KEY_SIZE, SEEK_SET) != 0)
    {
        perror("Error seeking to the second key");
        fclose(bank_fd);
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

// Checks to validate usernames, pins, amounts
int valid_username(char *username)
{
    if (strlen(username) > MAX_USERNAME_LEN || strlen(username) == 0)
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

// Create a <username>.card file for the user containing their encrypted pin and initialization vector
void create_card(Bank *bank, char *username, unsigned char *plaintext_pin)
{
    size_t card_file_size = strlen(username) + strlen(".card") + 1;
    char *card_file = (char *)malloc(card_file_size);

    if (card_file == NULL)
    {
        fprintf(stderr, "Error: Memory allocation failed.\n");
        return; 
    }

    strncpy(card_file, username, card_file_size - 1);
    card_file[card_file_size - 1] = '\0'; 

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

        // since each pin is 4 bytes, it will only need one block = 16 bytes (12 bytes padded).
        unsigned char encrypted_pin[AES_BLOCK_SIZE];

        // encrypt the pin in AES-256-CBC mode
        encrypt(plaintext_pin, strlen((char *)plaintext_pin), pin_key, iv, encrypted_pin);

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

    // remove newline at end of command
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

        if (get_user(bank, username) != NULL)
        {
            printf("Error: user %s already exists\n", username);
            return;
        }

        // add the user to the users list and create <username>.card
        create_user(bank, username, init_balance);
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

        User *user = get_user(bank, username);
        if (user == NULL)
        {
            printf("No such user\n");
            return;
        }

        // since we previously checked that amount is a valid integer, convert it to one
        int balance = user->balance;
        int deposit_amt = atoi(amount);

        // check that deposit doesn't cause integer overflow
        if (balance > INT_MAX - deposit_amt || deposit_amt > INT_MAX - balance)
        {
            printf("Too rich for this program\n");
            return;
        }

        // deposit the money
        user->balance += deposit_amt;

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

        User *user = get_user(bank, username);
        if (user == NULL)
        {
            printf("No such user\n");
            return;
        }

        printf("$%d\n", user->balance);
        return;
    }

    else
    {
        printf("Invalid command\n");
    }

    return;
}

// Create the entire message to send to the ATM, consisting of the encrypted command, the initialization vector,
// and the tag.
unsigned char *encrypt_message(Bank *bank, unsigned char *plaintext, size_t *sendline_len)
{
    unsigned char msg_key[AES_KEY_SIZE];
    extract_msg_key(bank->bank_file, msg_key);

    unsigned char iv[GCM_IV_SIZE];
    generate_rand_bytes(GCM_IV_SIZE, iv);

    // Encrypt the message
    unsigned char ciphertext[strlen((char *)plaintext) + AES_BLOCK_SIZE]; // make sure the ciphertext buffer is large enough to store the encrypted message
    unsigned char tag[TAG_SIZE];

    int c_len = gcm_encrypt(plaintext, strlen((char *)plaintext), NULL, 0, msg_key, iv, GCM_IV_SIZE, ciphertext, tag);
    int length_ciphertext = c_len;

    *sendline_len = sizeof(int) + length_ciphertext + GCM_IV_SIZE + TAG_SIZE;

    // Allocate buffer for sending the message to the bank
    unsigned char *sendline = malloc(*sendline_len);

    if (sendline == NULL)
    {
        printf("Memory allocation failed!\n");
        exit(EXIT_FAILURE);
    }

    // Write the message into sendline
    int offset = 0;
    memcpy(sendline + offset, &length_ciphertext, sizeof(int)); // length of ciphertext
    offset += sizeof(int);

    memcpy(sendline + offset, ciphertext, length_ciphertext); // ciphertext
    offset += length_ciphertext;

    memcpy(sendline + offset, iv, GCM_IV_SIZE); // IV
    offset += GCM_IV_SIZE;

    memcpy(sendline + offset, tag, TAG_SIZE); // tag

    return sendline;
}

// Process an authenticated command sent by the ATM
void bank_process_remote_command(Bank *bank, char *command, size_t len)
{
    size_t sendline_len = 0;
    unsigned char response[1000];
    memset(response, 0, sizeof(response));

    if (strstr(command, "begin-session"))
    {
        char username[MAX_USERNAME_LEN] = {0};

        // Extract username from the command
        if (sscanf(command, "begin-session %s", username) == 1)
        {
            // Check if the user exists
            if (get_user(bank, username) != NULL)
            {
                snprintf((char *)response, sizeof(response), "success");
            }
            else
            {
                snprintf((char *)response, sizeof(response), "No such user");
            }
        }
    }
    else if (strstr(command, "withdraw"))
    {
        char username[MAX_USERNAME_LEN] = {0};
        char amount[MAX_INT_BYTES] = {0};

        // Extract username and amount
        int matches = sscanf(command, "withdraw %s %s", username, amount);

        if (matches == 2)
        {
            User *curr_user = get_user(bank, username);
            if (curr_user)
            {
                int curr_balance = curr_user->balance;
                int withdraw_amt = atoi(amount);
                if (withdraw_amt > curr_balance)
                {
                    snprintf((char *)response, sizeof(response), "Insufficient funds");
                }
                else
                {
                    curr_user->balance -= withdraw_amt;
                    snprintf((char *)response, sizeof(response), "$%d dispensed", withdraw_amt);
                }
            }
            else
            {
                snprintf((char *)response, sizeof(response), "User not found");
            }
        }
        else
        {
            snprintf((char *)response, sizeof(response), "Invalid withdraw command");
        }
    }

    else if (strstr(command, "balance"))
    {
        char username[MAX_USERNAME_LEN] = {0};

        // Extract username from the command
        if (sscanf(command, "balance %s", username) == 1)
        {
            User *curr_user = get_user(bank, username);
            int curr_balance = curr_user->balance;
            snprintf((char *)response, sizeof(response), "$%d", curr_balance);
        }
    }

    unsigned char *sendline = encrypt_message(bank, response, &sendline_len);
    bank_send(bank, (char *)sendline, sendline_len);
    free(sendline);

    return;
}
