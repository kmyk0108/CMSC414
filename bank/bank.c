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

    char bank_file[strlen(filename) + 2];
    bank_file[0] = '\0';
    strcat(bank_file, ".");
    strcat(bank_file, filename);
    bank->bank_file = strdup(bank_file);
    bank->user_index = 0;
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

void bank_process_local_command(Bank *bank, char *command, size_t len)
{
    if (strstr(command, "create-user")) {
        char * user_info[4];
        char * token = strtok(command, " ");
        int index = 0;

        while (token != NULL) {
            user_info[index] = token;
            index += 1;
            token = strtok(NULL, " ");
        }
        //if less than 4 args
        if (index != 4) {
            printf("Usage:  create-user <user-name> <pin> <balance>\n");
            return;
        }

        // check username
        char *username = user_info[1];
        if (strlen(username) > 250)
        {
            printf("Usage:  create-user <user-name> <pin> <balance>\n");
            return;
        }
        else
        {
            index = 0;
            while (username[index] != '\0')
            {
                // non-letter characters in username - invalid
                if (isalpha(username[index]) == 0)
                {
                    printf("Usage:  create-user <user-name> <pin> <balance>\n");
                    return;
                }
                index += 1;
            }
        }
        
        char * str_pin;
        //check pin
        if (strlen(user_info[2]) != 4) {
            printf("Usage:  create-user <user-name> <pin> <balance>\n");
            return;
        } else {
            str_pin = user_info[2];
            index = 0;
            while (str_pin[index] != '\0') {
                //non-numeric characters in pin - invalid
                if (isdigit(str_pin[index]) == 0) {
                    printf("Usage:  create-user <user-name> <pin> <balance>\n");
                    return;
                }
                index += 1;
            }
        }

        //check balance
        if (user_info[3][0] == '-') { //if negative
             printf("Usage:  create-user <user-name> <pin> <balance>\n");
            return;
        }
        char *endptr;
        errno = 0;
        char * str_bal = user_info[3];
        long n = strtol(str_bal, &endptr, 0);
        //check if greater than largest int in c or non-numeric characters
        if (errno == ERANGE || n > INT_MAX || str_bal == endptr) {
            printf("Usage:  create-user <user-name> <pin> <balance>\n");
            return;
        }

        char * card_file = (char*)malloc(strlen(username) + 1);
        strcpy(card_file, username);
        strcat(card_file, ".card");

        FILE *file = fopen(card_file, "r");
        //if .card file already exists for user
        if (file) {
            fclose(file);
            printf("Error: user %s already exists\n", username);
            return;
        } 

        int ptr = open(card_file, O_WRONLY | O_CREAT | O_EXCL, 0666);
        if (ptr == -1) {
            printf("Error creating card file for %s\n", username);
        } else { //.card file does not exist yet
            char temp_str[271] = "";
            sprintf(temp_str, "%s\n%s", username, str_pin);
            const char * str = temp_str;
            //write username and pin to .card file
            write(ptr, str, strlen(str));
            close(ptr);
            free(card_file);
            printf("Created user %s\n", username);
            char new_user[262];
            strcpy(new_user, username);
            strcat(new_user, " ");
            strcat(new_user, str_bal);
            strcpy(bank->users[bank->user_index], new_user);
            bank->user_index += 1;
        }
    } else if (strstr(command, "deposit")) {
        char * username;
        int amount;
        char * user_info[3];
        char * token = strtok(command, " ");
        int index = 0;

        while (token != NULL) {
            user_info[index] = token;
            index += 1;
            token = strtok(NULL, " ");
        }

        //less than 3 args
        if (index != 3) {
            printf("Usage:  deposit <user-name> <amt>\n");
            return;
        }
        
        //check username
        username = user_info[1];
        if (strlen(username) > 250) {
            printf("Usage:  deposit <user-name> <amt>\n");
            return;
        } else {
            index = 0;
            while (username[index] != '\0') {
                //non-alphabetic characters - invalid
                if (isalpha(username[index]) == 0) {
                    printf("Usage:  deposit <user-name> <amt>\n");
                    return;
                }
                index += 1;
            }
        }
        //amount negative - invalid
        if (user_info[2][0] == '-') {
             printf("Usage:  deposit <user-name> <amt>\n");
            return;
        }
        char *endptr;
        errno = 0;
        char * str_amt = user_info[2];
        long n = strtol(str_amt, &endptr, 0);
        if (str_amt == endptr) { //non-numeric characters - invalid
            printf("Usage:  deposit <user-name> <amt>\n");
            return;
        } else if (errno == ERANGE || n > INT_MAX) { //larger than greatest int in c
            printf("Too rich for this program\n");
            return;
        }
        amount = atoi(str_amt);

        //check if .card file exists
        char * card_file = (char*)malloc(strlen(username) + 1);
        strcpy(card_file, username);
        strcat(card_file, ".card");

        if (access(card_file, F_OK) != 0) {
            printf("No such user\n");
            return;
        } else { //.card file exists
            printf("$%d added to %s's account\n", amount, username);
            int found_index = 0;
            char updated_user[263];
            //iterate through all current users
            for (int i = 0; i < bank->user_index; i++) {
                char curr_user[263];
                updated_user[0] = '\0';
                strcpy(curr_user, bank->users[i]);
                //if current line contains username 
                if (strstr(curr_user, username)) {
                    found_index = i;
                    char * curr_user_info = strtok(curr_user, " ");
                    int times_run = 0;
                    while (curr_user_info != NULL) {
                        if (times_run == 1) { //true on third arg, amount to add
                            int temp_bal = atoi(curr_user_info);
                            temp_bal += amount;
                            sprintf(curr_user_info, "%d", temp_bal);
                            strcat(updated_user, " ");
                            strcat(updated_user, curr_user_info);
                            strcat(updated_user, "\n");
                        } else {
                            //use updated user to store the new line of information
                            strcat(updated_user, curr_user_info);
                        }
                        times_run += 1;
                        curr_user_info = strtok(NULL, " ");
                    }

                    break;
                }
            }

            //copy new line into existing bank->users variable
            strcpy(bank->users[found_index], updated_user);
        }

    } else if (strstr(command, "balance")) {
        char * username;
        char * user_info[2];
        char * token = strtok(command, " ");
        int index = 0;

        while (token != NULL) {
            user_info[index] = token;
            index += 1;
            token = strtok(NULL, " ");
        }

        //less than 2 args
        if (index != 2) {
            printf("Usage:  balance <user-name> (less than 2 args)\n");
            return;
        }
        
        //check username
        username = user_info[1];
        username[strlen(username) - 1] = '\0';
        if (strlen(username) > 250) {
            printf("Usage:  balance <user-name> (pick shorter username)\n");
            return;
        } else {
            index = 0;
            while (username[index] != '\0') {
                //non-alphabetic characters - invalid
                if (isalpha(username[index]) == 0) {
                    printf("Usage:  balance <user-name> (only alphabetic chars allowed)\n");
                    return;
                }
                index += 1;
            }
        }

        //check if .card file exists
        char * card_file = (char*)malloc(strlen(username) + 1);
        strcpy(card_file, username);
        strcat(card_file, ".card");

        if (access(card_file, F_OK) != 0) {
            printf("No such user\n");
            return;
        } else { //.card file exists
            char curr_user[263];
            for (int i = 0; i < bank->user_index; i++) {
                strcpy(curr_user, bank->users[i]);
                //if current line contains username 
                if (strstr(curr_user, username)) {
                    char * curr_user_info = strtok(curr_user, " ");
                    int times_run = 0;
                    while (curr_user_info != NULL) {
                        if (times_run == 1) {
                            printf("$%s", curr_user_info);
                        }
                        curr_user_info = strtok(NULL, " ");
                        times_run += 1;
                    }
                }
            }
                
        }

    } else {
        printf("Invalid command\n");
        return;
    }
}

void bank_process_remote_command(Bank *bank, char *command, size_t len)
{
    if (strstr(command, "withdraw")) {
        strtok(command, " ");
        char * amount = strtok(NULL, " ");
        
    }
    bank_process_local_command(bank, command, len);

    
    /*
     * The following is a toy example that simply receives a
     * string from the ATM, prepends "Bank got: " and echoes
     * it back to the ATM before printing it to stdout.
     */

    /*
    char sendline[1000];
    command[len]=0;
    sprintf(sendline, "Bank got: %s", command);
    bank_send(bank, sendline, strlen(sendline));
    printf("Received the following:\n");
    fputs(command, stdout);
    */
}
