#include "bank.h"
#include "ports.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>
#include <fcntl.h>

#define ERROR_FILE_CREATION 64

Bank *bank_create(const char *filename)
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
    // TODO set up more, as needed

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
    if (balance_str[strlen(balance_str) - 1] == '\n')
    {
        balance_str[strlen(balance_str) - 1] = '\0';
    }

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

int check_create_user(char *create_user, char *username, char *pin, char *balance_str)
{
    return strcmp(create_user, "create-user") == 0 && username != NULL && pin != NULL && balance_str != NULL && strtok(NULL, " ") == NULL && valid_username(username) && valid_pin(pin) && valid_balance(balance_str);
}

int check_deposit(char *deposit, char *username, char *amount)
{
    return strcmp(deposit, "deposit") == 0 && username != NULL && amount != NULL && strtok(NULL, " ") == NULL && valid_username(username) && valid_balance(amount);
}

void withdraw(Bank *bank, char *command, char *sendline)
{
    char *username = strtok(command, " ");
    char *withdraw_command = strtok(NULL, " ");
    char *amount = strtok(NULL, " ");

    if (!valid_balance(amount)) {
        printf("HI\n");
    }

    if (strcmp(withdraw_command, "withdraw") == 0 && amount != NULL && strtok(NULL, " ") == NULL && valid_balance(amount))
    {
        User *user = get_user(bank, username);
        int int_amount = atoi(amount);
        if (int_amount > user->balance)
        {
            sprintf(sendline, "Insufficient funds\n");
        }
        else
        {
            user->balance -= int_amount;
            sprintf(sendline, "$%s dispensed\n", amount);
        }
    }
    else
    {
        sprintf(sendline, "Usage: withdraw <amt>\n");
    }
    return;
}

void print_balance(Bank *bank, char *first, char *username, char *sendline)
{
    if (strcmp(first, "balance") == 0 && username != NULL && strtok(NULL, " ") == NULL)
    {
        User *user = get_user(bank, username);
        if (user)
        {
            sprintf(sendline, "$%d\n", user->balance);
        }
        else
        {
            sprintf(sendline, "No such user\n");
        }
    }
    else
    {
        sprintf(sendline, "Usage: balance <user-name>\n");
    }

    return;
}

void bank_process_local_command(Bank *bank, char *command, size_t len)
{
    if (strstr(command, "create-user"))
    {
        char *first = strtok(command, " ");
        char *username = strtok(NULL, " ");
        char *pin = strtok(NULL, " ");
        char *init_balance = strtok(NULL, " ");
        if (!check_create_user(first, username, pin, init_balance))
        {
            printf("Usage: create-user <user-name> <pin> <balance>\n");
            return;
        }

        if (get_user(bank, username) != NULL)
        {
            printf("Error: user %s already exists\n", username);
            return;
        }

        // add the new user to the users list
        create_user(bank, username, init_balance);

        // try to create a .card file for the user
        char *card_file = (char *)malloc(strlen(username) + 1);
        strcpy(card_file, username);
        strcat(card_file, ".card");

        FILE *file = fopen(card_file, "r");
        // if .card file already exists for user
        if (file)
        {
            fclose(file);
            printf("Error: user %s already exists\n", username);
            return;
        }

        int ptr = open(card_file, O_WRONLY | O_CREAT | O_EXCL, 0666);
        if (ptr == -1)
        {
            printf("Error creating card file for %s\n", username);
        }
        else
        {
            // write username and pin to .card file
            char temp_str[271] = "";
            sprintf(temp_str, "%s\n%s", pin, username);
            const char *str = temp_str;
            write(ptr, str, strlen(str));
            close(ptr);
            free(card_file);
            printf("Created user %s\n", username);
        }
    }
    else if (strstr(command, "deposit"))
    {
        char *first = strtok(command, " ");
        char *username = strtok(NULL, " ");
        char *amount = strtok(NULL, " ");

        if (!check_deposit(first, username, amount))
        {
            printf("Usage: deposit <user-name> <amt>\n");
            return;
        }

        User *user = get_user(bank, username);
        if (user == NULL)
        {
            printf("No such user\n");
            return;
        }
        int balance = user->balance;
        int deposit_amt = atoi(amount);

        // check that deposit doesn't cause integer overflow
        if (balance > INT_MAX - deposit_amt || deposit_amt > INT_MAX - balance)
        {
            printf("Too rich for this program\n");
            return;
        }
        user->balance += deposit_amt;
        printf("$%d added to %s's account\n", deposit_amt, username);
    }
    else if (strstr(command, "balance"))
    {
        char sendline[10000];
        char *first = strtok(command, " ");
        char *username = strtok(NULL, " ");
        if (username != NULL && username[strlen(username) - 1] == '\n')
        {
            username[strlen(username) - 1] = '\0';
            print_balance(bank, first, username, sendline);
            printf("%s", sendline);
        }
        else
        {
            printf("Usage: balance <user-name>\n");
        }
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

    if (strstr(command, "withdraw"))
    {
        withdraw(bank, command, sendline);
    }
    else if (strstr(command, "balance"))
    {

        char *username = strtok(command, " ");
        char *balance = strtok(NULL, " ");
        balance[strlen(balance) - 1] = '\0';

        if (strcmp(balance, "balance") != 0)
        {
            sprintf(sendline, "Usage: balance\n");
        }
        else
        {
            print_balance(bank, balance, username, sendline);
        }
    }
    else
    {
        sprintf(sendline, "Invalid command");
    }

    bank_send(bank, sendline, strlen(sendline));
    return;
}
