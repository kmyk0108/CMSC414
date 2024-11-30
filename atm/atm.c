#include "atm.h"
#include "ports.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <limits.h>

static const int MAX_ATTEMPTS = 3;

ATM *atm_create()
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

int check_pin(FILE *card, char *pin)
{
    size_t len = strlen(pin);
    if (len > 0 && pin[len - 1] == '\n')
    {
        pin[len - 1] = '\0';
    }

    char card_content[5];
    char *stored_pin = fgets(card_content, 5, card);

    if (strcmp(pin, stored_pin) != 0)
    {
        return 0;
    }

    return 1;
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
    new_user->attempts = 0;
    new_user->next = atm->attempts_list_head;
    atm->attempts_list_head = new_user;

    return new_user;
}

void atm_process_command(ATM *atm, char *command)
{
    if (strstr(command, "begin-session"))
    {
        if (atm->is_logged_in)
        {
            printf("A user is already logged in\n");
            return;
        }

        // read in username
        char *begin_session = strtok(command, " ");
        char *username = strtok(NULL, " ");

        if (strcmp(begin_session, "begin-session") || !check_input(username))
        {
            printf("Usage: begin-session <user-name>\n");
            return;
        }

        // check if <user-name>.card file exists
        char card_file[256];

        strcpy(card_file, username);
        strcat(card_file, ".card");

        if (access(card_file, F_OK) != 0)
        {
            printf("No such user\n");
            return;
        }

        FILE *card = fopen(card_file, "r");
        if (card == NULL)
        {
            fclose(card);
            printf("Unable to access %s's card\n", username);
            return;
        }

        // ask for their pin
        char user_input[1000];
        printf("PIN? ");
        char *pin = fgets(user_input, 1000, stdin);

        if (!get_login(atm, username)) {
            add_new(atm, username);
        }
        LoginAttempt *curr = get_login(atm, username);
        curr->attempts++;

        if (curr->attempts > MAX_ATTEMPTS)
        {
            printf("Too many attempts. %s's card file has been locked.\n", username);
            return;
        }

        if (!check_pin(card, pin))
        {
            printf("Not authorized\n");
            return;
        }

        printf("Authorized\n");

        // set state of ATM
        atm->is_logged_in = 1;
        atm->curr_user = strdup(username);
        fclose(card);
    }
    else if (strstr(command, "withdraw") || strstr(command, "balance"))
    {
        if (!atm->is_logged_in)
        {
            printf("No user logged in\n");
            return;
        }

        // handle command inside bank file
        char recvline[10000];
        int n;
        char sendline[10000];

        snprintf(sendline, sizeof(sendline), "%s %s", atm->curr_user, command);
        atm_send(atm, sendline, strlen(sendline));
        n = atm_recv(atm, recvline, 10000);
        recvline[n] = 0;
        fputs(recvline, stdout);
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
