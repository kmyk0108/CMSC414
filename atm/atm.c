#include "atm.h"
#include "ports.h"
#include "encryption/enc.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <limits.h>
#include <signal.h>

#define MAX_ATTEMPTS 3
#define MAX_USERNAME_LEN 250

ATM *atm_create(char *atm_file)
{
    ATM *atm = (ATM *)malloc(sizeof(ATM));
    if (atm == NULL)
    {
        perror("Could not allocate ATM");
        exit(1);
    }

    // Set up the network state
    atm->sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    bzero(&atm->rtr_addr, sizeof(atm->rtr_addr));
    atm->rtr_addr.sin_family = AF_INET;
    atm->rtr_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    atm->rtr_addr.sin_port = htons(ROUTER_PORT);

    bzero(&atm->atm_addr, sizeof(atm->atm_addr));
    atm->atm_addr.sin_family = AF_INET;
    atm->atm_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    atm->atm_addr.sin_port = htons(ATM_PORT);
    bind(atm->sockfd, (struct sockaddr *)&atm->atm_addr, sizeof(atm->atm_addr));

    // Set up the protocol state
    // TODO set up more, as needed
    atm->is_logged_in = 0;
    atm->curr_user = NULL;
    atm->atm_file = atm_file;

    return atm;
}

void atm_free(ATM *atm)
{
    if (atm != NULL)
    {
        close(atm->sockfd);
        free(atm);
    }
}

ssize_t atm_send(ATM *atm, char *data, size_t data_len)
{
    // Returns the number of bytes sent; negative on error
    return sendto(atm->sockfd, data, data_len, 0,
                  (struct sockaddr *)&atm->rtr_addr, sizeof(atm->rtr_addr));
}

ssize_t atm_recv(ATM *atm, char *data, size_t max_data_len)
{
    // Returns the number of bytes received; negative on error
    return recvfrom(atm->sockfd, data, max_data_len, 0, NULL, NULL);
}

int extract_pin_key(char *atm_file, unsigned char *key)
{
    // Ensure that exactly one argument is provided
    FILE *atm_fd = fopen(atm_file, "rb");
    if (atm_fd == NULL)
    {
        perror("Error opening bank initialization file");
        return 1;
    }
    size_t bytes_read = fread(key, sizeof(char), AES_KEY_SIZE, atm_fd);
    if (bytes_read != AES_KEY_SIZE)
    {
        perror("Error reading file");
        fclose(atm_fd);
        return 1;
    }
    fclose(atm_fd);
    return 0;
}

int extract_msg_key(char *atm_file, unsigned char *key)
{
    // Ensure that exactly one argument is provided
    FILE *atm_fd = fopen(atm_file, "rb");
    if (atm_fd == NULL)
    {
        perror("Error opening bank initialization file");
        return 1;
    }

    if (fseek(atm_fd, AES_KEY_SIZE, SEEK_SET) != 0)
    {
        perror("Error seeking to the second key");
        fclose(atm_fd);
        return 1;
    }

    size_t bytes_read = fread(key, sizeof(char), AES_KEY_SIZE, atm_fd);
    if (bytes_read != AES_KEY_SIZE)
    {
        perror("Error reading file");
        fclose(atm_fd);
        return 1;
    }
    fclose(atm_fd);
    return 0;
}

int card_contents(char *card, char *username, unsigned char *pin, unsigned char *iv)
{
    // Ensure that exactly one argument is provided
    FILE *atm_fd = fopen(card, "rb");
    if (atm_fd == NULL)
    {
        printf("Unable to access %s's card\n", username);
        return 1;
    }
    if (fread(pin, 1, AES_BLOCK_SIZE, atm_fd) != AES_BLOCK_SIZE)
    {
        perror("Error reading file");
        fclose(atm_fd);
        return 1;
    }
    if (fread(iv, 1, IV_SIZE, atm_fd) != IV_SIZE)
    {
        perror("Error reading file");
        fclose(atm_fd);
        return 1;
    }
    fclose(atm_fd);
    return 0;
}

int valid_username(char *username)
{
    if (strlen(username) > 250)
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

int check_input(char *username)
{
    // check for exactly one argument
    if (strtok(NULL, " ") != NULL || username == NULL)
    {
        return 0;
    }

    // remove newline character
    size_t len = strlen(username);
    if (len > 0 && username[len - 1] == '\n')
    {
        username[len - 1] = '\0';
    }

    return valid_username(username);
}

int check_pin(char *atm_file, char *card_file, char *username, char *plaintext_pin)
{
    // extract the pin key from .atm
    unsigned char pin_key[AES_KEY_SIZE];
    if (extract_pin_key(atm_file, pin_key) != 0)
    {
        printf("Error extracting key\n");
        return 1;
    }

    // extract the contents of .card
    unsigned char stored_pin[AES_BLOCK_SIZE];
    unsigned char iv[IV_SIZE];
    if (card_contents(card_file, username, stored_pin, iv) != 0)
    {
        // printf("Error extracting card contents\n");
        return 1;
    }

    unsigned char encrypted_attempt_pin[AES_BLOCK_SIZE];

    // encrypt the plaintext pin
    int c_len = encrypt((unsigned char *)plaintext_pin, strlen((char *)plaintext_pin), pin_key, iv, encrypted_attempt_pin);
    // printf("Ciphertext length: %d\n", c_len);
    // printf("%s\n%s\n", stored_pin, encrypted_attempt_pin);
    // compare stored_pin with encrypted_attempt_pin
    if (memcmp(stored_pin, encrypted_attempt_pin, AES_BLOCK_SIZE) == 0)
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

LoginAttempt *get_login(ATM *atm, char *username)
{
    LoginAttempt *current = atm->attempts_list_head;

    // Search for the username in the list
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

LoginAttempt *add_new(ATM *atm, char *username)
{
    // Not found, create a new user and add to front of list
    LoginAttempt *new_user = malloc(sizeof(LoginAttempt));
    if (!new_user)
    {
        perror("malloc failed");
        exit(EXIT_FAILURE);
    }
    strncpy(new_user->username, username, sizeof(new_user->username) - 1);
    new_user->username[sizeof(new_user->username) - 1] = '\0';
    new_user->attempts = 1;
    new_user->next = atm->attempts_list_head;
    atm->attempts_list_head = new_user;

    return new_user;
}

unsigned char *encrypt_message(ATM *atm, unsigned char *plaintext, size_t *sendline_len)
{
    unsigned char msg_key[AES_KEY_SIZE];
    extract_msg_key(atm->atm_file, msg_key);

    unsigned char iv[GCM_IV_SIZE];
    generate_rand_bytes(GCM_IV_SIZE, iv);

    // Encrypt the message
    unsigned char ciphertext[strlen((char *)plaintext) + AES_BLOCK_SIZE]; // make sure the ciphertext buffer is large enough to store the encrypted message
    unsigned char tag[TAG_SIZE];

    int c_len = gcm_encrypt(plaintext, strlen((char *)plaintext), NULL, 0, msg_key, iv, GCM_IV_SIZE, ciphertext, tag);
    // printf("Ciphertext: %s\n", ciphertext);   // Debugging
    // printf("Ciphertext length: %d\n", c_len); // Debugging

    // Prepare the lengths
    int length_ciphertext = c_len;

    // Calculate total sendline length
    *sendline_len = sizeof(int) + length_ciphertext + GCM_IV_SIZE + TAG_SIZE;

    // Allocate buffer for sending the message to the bank
    unsigned char *sendline = malloc(*sendline_len);

    if (sendline == NULL)
    {
        printf("Memory allocation failed!\n");
        exit(EXIT_FAILURE);
    }

    // Pack the message into sendline
    int offset = 0;
    memcpy(sendline + offset, &length_ciphertext, sizeof(int)); // Length of ciphertext
    offset += sizeof(int);

    memcpy(sendline + offset, ciphertext, length_ciphertext); // Ciphertext
    offset += length_ciphertext;

    memcpy(sendline + offset, iv, GCM_IV_SIZE); // IV
    offset += GCM_IV_SIZE;

    memcpy(sendline + offset, tag, TAG_SIZE); // Tag

    return sendline; // Return the pointer to sendline
}

int decrypt_message(ATM *atm, char *command, size_t len, char *plaintext_buffer, size_t buffer_size)
{
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
    extract_msg_key(atm->atm_file, msg_key);

    // Decrypt the message
    int p_len = gcm_decrypt(ciphertext, length_ciphertext, NULL, 0, tag, msg_key, iv, GCM_IV_SIZE, (unsigned char *)plaintext_buffer);

    // Handle decryption errors
    if (p_len < 0)
    {
        printf("Untrustworthy source\n");
        free(ciphertext); // Free allocated memory
        free(iv);         // Free allocated memory
        free(tag);        // Free allocated memory
        return -1;        // Return -1 to indicate decryption failure
    }

    plaintext_buffer[p_len] = '\0'; // Null-terminate the plaintext

    free(ciphertext); // Free allocated memory
    free(iv);         // Free allocated memory
    free(tag);        // Free allocated memory

    return p_len; // Return the length of the decrypted plaintext
}

int begin_session(ATM *atm, char *username, char *recvline)
{
    // Create the plaintext
    unsigned char plaintext[MAX_USERNAME_LEN + strlen("begin-session ") + 1];
    snprintf((char *)plaintext, sizeof(plaintext), "begin-session %s", username);

    // Sendline length (will be set by encrypt_message)
    size_t sendline_len = 0;

    unsigned char *sendline = encrypt_message(atm, plaintext, &sendline_len);

    atm_send(atm, (char *)sendline, sendline_len);
    free(sendline);
    int n = atm_recv(atm, recvline, 10000);

    char plaintext_buf[1000];
    if (decrypt_message(atm, recvline, n, plaintext_buf, 1000) == -1)
    {
        atm_free(atm);
        exit(-1);
    }

    if (strcmp(plaintext_buf, "No such user") == 0)
    {
        printf("%s\n", plaintext_buf);
        return 1;
    }

    return 0; // Success
}

int withdraw(ATM *atm, char *username, char *amount, char *recvline)
{
    unsigned char plaintext[MAX_USERNAME_LEN + strlen("withdraw") + strlen(amount) + 3];
    snprintf((char *)plaintext, sizeof(plaintext), "withdraw %s %s", username, amount);

    // Sendline length (will be set by encrypt_message)
    size_t sendline_len = 0;

    unsigned char *sendline = encrypt_message(atm, plaintext, &sendline_len);

    atm_send(atm, (char *)sendline, sendline_len);
    free(sendline);
    int n = atm_recv(atm, recvline, 10000);

    char plaintext_buf[1000];
    if (decrypt_message(atm, recvline, n, plaintext_buf, 1000) == -1)
    {
        atm_free(atm);
        exit(-1);
    }

    printf("%s\n", plaintext_buf);

    if (strcmp(plaintext_buf, "Insufficient funds") == 0)
    {
        return 1;
    }

    return 0; // Success
}

int balance(ATM *atm, char *username, char *recvline) {
    unsigned char plaintext[MAX_USERNAME_LEN + strlen("balance ") + 1];
    snprintf((char *)plaintext, sizeof(plaintext), "balance %s", username);

    // Sendline length (will be set by encrypt_message)
    size_t sendline_len = 0;

    unsigned char *sendline = encrypt_message(atm, plaintext, &sendline_len);

    atm_send(atm, (char *)sendline, sendline_len);
    free(sendline);
    int n = atm_recv(atm, recvline, 10000);

    char plaintext_buf[1000];
    if (decrypt_message(atm, recvline, n, plaintext_buf, 1000) == -1)
    {
        atm_free(atm);
        exit(-1);
    }

    printf("%s\n", plaintext_buf);

    return 0; // Success
}

void atm_process_command(ATM *atm, char *command)
{
    char command_copy[1000];
    char recvline[1000];

    // Ensure null-termination
    if (strlen(command) >= sizeof(command_copy))
    {
        fprintf(stderr, "Error: command too long\n");
        return;
    }

    strncpy(command_copy, command, sizeof(command_copy) - 1);
    command_copy[sizeof(command_copy) - 1] = '\0';

    // remove \n at end of command
    command_copy[strlen(command_copy) - 1] = '\0';

    if (strstr(command, "begin-session"))
    {
        if (atm->is_logged_in)
        {
            printf("A user is already logged in\n");
            return;
        }

        // read in username
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
                printf("Usage:  begin-session <user-name>\n");
                return;
            }
            token = strtok(NULL, " ");
        }

        // too few args
        if (arg_count < 2)
        {
            printf("Usage:  begin-session <user-name>\n");
            return;
        }

        char *command = args[0];
        char *username = args[1];

        if (strcmp(command, "begin-session") || !check_input(username))
        {
            printf("Usage: begin-session <user-name>\n");
            return;
        }

        // check if the user is in the bank system
        if (begin_session(atm, username, recvline) != 0)
        {
            return;
        }

        // ask for their pin
        char user_input[1000];
        printf("PIN? ");
        char *pin = fgets(user_input, 1000, stdin);

        if (pin != NULL)
        {
            size_t len = strlen(pin);
            if (len > 0 && pin[len - 1] == '\n')
            {
                pin[len - 1] = '\0'; // Replace newline with null terminator
            }
        }

        if (!get_login(atm, username))
        {
            add_new(atm, username);
        }

        LoginAttempt *curr = get_login(atm, username);

        if (curr->attempts > MAX_ATTEMPTS)
        {
            printf("Too many attempts. %s's card file has been locked.\n", username);
            return;
        }

        // check the pin against the stored pin in their card
        char card_file[MAX_USERNAME_LEN + 6];
        strncpy(card_file, username, MAX_USERNAME_LEN);
        strcat(card_file, ".card");

        if (check_pin(atm->atm_file, card_file, username, pin) != 0)
        {
            printf("Not authorized\n");
            LoginAttempt *curr = get_login(atm, username);
            curr->attempts++;

            if (curr->attempts > MAX_ATTEMPTS)
            {
                printf("Too many attempts. %s's card file has been locked.\n", username);
                return;
            }
            return;
        }
        memset(card_file, 0, MAX_USERNAME_LEN + 6);

        printf("Authorized\n");

        // set state of ATM
        atm->is_logged_in = 1;
        atm->curr_user = strdup(username);
    }
    else if (strstr(command, "withdraw"))
    {
        char *args[2]; // Expected arguments: command, amount
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
                printf("Usage: withdraw <amt>\n");
                return;
            }
            token = strtok(NULL, " ");
        }

        // too few args
        if (arg_count < 2)
        {
            printf("Usage: withdraw <amt>\n");
            return;
        }

        char *command = args[0];
        char *amount = args[1];

        if (strcmp(command, "withdraw") != 0 || !valid_balance(amount))
        {
            printf("Usage: withdraw <amt>\n");
            return;
        }

        // encrypt message "withdraw <username> <amount>"
        if (withdraw(atm, atm->curr_user, amount, recvline) != 0)
        {
            return;
        }
    }
    else if (strstr(command, "balance"))
    {
        char *token = strtok(command_copy, " ");

        if (strcmp(token, "balance") != 0 || strtok(NULL, " ") != NULL)
        {
            printf("Usage: balance\n");
            return;
        }

        balance(atm, atm->curr_user, recvline);
        return;
    }
    else if (strstr(command, "end-session\n"))
    {
        if (!atm->is_logged_in)
        {
            printf("No user logged in\n");
            return;
        }

        // reset attempts to 0
        get_login(atm, atm->curr_user)->attempts = 0;
        atm->is_logged_in = 0;
        atm->curr_user = NULL;
        printf("User logged out\n");
        return;
    }
    else
    {
        printf("Invalid command\n");
        return;
    }
    /*
     * The following is a toy example that simply sends the
     * user's command to the bank, receives a message from the
     * bank, and then prints it to stdout.
     */

    /*
    char recvline[10000];
    int n;

    atm_send(atm, command, strlen(command));
    n = atm_recv(atm,recvline,10000);
    recvline[n]=0;
    fputs(recvline,stdout);
    */
}
