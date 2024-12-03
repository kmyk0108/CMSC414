/* 
 * The main program for the ATM.
 *
 * You are free to change this as necessary.
 */

#include "atm.h"
#include <stdio.h>
#include <stdlib.h>

#define ERROR_USAGE 62
#define ERROR_FILE_OPEN 64

static const char prompt[] = "ATM: ";

int main(int argc, char **argv)
{
    char user_input[10000];

    if (argc != 2)
    {
        printf("Usage:  init <filename>\n");
        return ERROR_USAGE;
    }

    char *atm_file = argv[1];

    // If first char of the bank file is a slash, remove it
    if (atm_file[0] == '/') {
        atm_file++;
    }

    FILE *bank_fd = fopen(atm_file, "r");
    if (bank_fd == NULL)
    {
        perror("Error opening bank initialization file");
        return ERROR_FILE_OPEN;
    }

    ATM *atm = atm_create(atm_file);

    printf("%s", prompt);
    fflush(stdout);

    while (fgets(user_input, 10000,stdin) != NULL)
    {
        atm_process_command(atm, user_input);

        // change the prompt to "ATM (<username>): " if a user is logged in
        if (atm->is_logged_in) {
            printf("ATM (%s): ", atm->curr_user);
        } else {
            printf("%s", prompt);
        }
        fflush(stdout);
    }
    atm_free(atm);
	return EXIT_SUCCESS;
}
