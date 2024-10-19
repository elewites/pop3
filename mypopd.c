#include "netbuffer.h"
#include "mailuser.h"
#include "server.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <ctype.h>

#define MAX_LINE_LENGTH 1024

typedef enum state {
    Undefined,
    Authorization,
    Transaction,
    Update
} State;

typedef struct serverstate {
    int fd;
    net_buffer_t nb;
    char recvbuf[MAX_LINE_LENGTH + 1];
    char *words[MAX_LINE_LENGTH];
    int nwords;
    State state;
    struct utsname my_uname;
    // TODO: Add additional fields as necessary
    char current_user[MAX_USERNAME_SIZE];  // Add this field to store the username

} serverstate;

static void handle_client(void *new_fd);
// Function to handle incoming commands
int handle_command(serverstate *ss, const char *command);

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Invalid arguments. Expected: %s <port>\n", argv[0]);
        return 1;
    }
    run_server(argv[1], handle_client);
    return 0;
}

// syntax_error returns
//   -1 if the server should exit
//    1 otherwise
int syntax_error(serverstate *ss) {
    if (send_formatted(ss->fd, "-ERR %s\r\n", "Syntax error in parameters or arguments") <= 0) return -1;
    return 1;
}

// checkstate returns
//   -1 if the server should exit
//    0 if the server is in the appropriate state
//    1 if the server is not in the appropriate state
int checkstate(serverstate *ss, State s) {
    if (ss->state != s) {
        if (send_formatted(ss->fd, "-ERR %s\r\n", "Bad sequence of commands") <= 0) return -1;
        return 1;
    }
    return 0;
}

// All the functions that implement a single command return
//   -1 if the server should exit
//    0 if the command was successful
//    1 if the command was unsuccessful

int do_quit(serverstate *ss) {
    // Note: This method has been filled in intentionally!
    dlog("Executing quit\n");
    send_formatted(ss->fd, "+OK Service closing transmission channel\r\n");
    ss->state = Update;
    return -1;
}

// int do_user(serverstate *ss) {
//     dlog("Executing user\n");
//     // TODO: Implement this function
//     return 0;
// }
int do_user(serverstate *ss, const char *username) {
    dlog("Executing USER command\n");
    // Check if the username is NULL
    if (username == NULL) {
        send_formatted(ss->fd, "-ERR Missing username\r\n");
        return 1;  // Indicate failure
    }
    // Ensure that the command is only allowed in the Authorization state
    if (ss->state != Authorization) {
        send_formatted(ss->fd, "-ERR Command USER only allowed in AUTHORIZATION state\r\n");
        return 1; // Indicate failure
    }
    // Validate the username using the is_valid_user function
    if (is_valid_user(username, NULL)) {
        // Store the username in the server state (for later use in PASS command)
        strncpy(ss->current_user, username, sizeof(ss->current_user) - 1);
        return send_formatted(ss->fd, "+OK User accepted, proceed with PASS\r\n") <= 0 ? 1 : 0;
    } else {
        // Respond with an error if the user is not found
        send_formatted(ss->fd, "-ERR user not found\r\n");
        return 1;  // Indicate failure
    }
}

// int do_pass(serverstate *ss) {
//     dlog("Executing pass\n");
//     // TODO: Implement this function
//     return 0;
// }
int do_pass(serverstate *ss, const char *password) {
    if (ss->state != Authorization) {
        return send_formatted(ss->fd, "-ERR Command PASS only allowed in AUTHORIZATION state\r\n") <= 0 ? 1 : 1;
    }
    if (ss->current_user[0] == '\0') {  // Check if current_user is not set
        return send_formatted(ss->fd, "-ERR No user set\r\n") <= 0 ? 1 : 1;
    }
    if (password == NULL) {
        return send_formatted(ss->fd, "-ERR Missing password\r\n") <= 0 ? 1 : 1;
    }
    if (is_valid_user(ss->current_user, password)) {
        ss->state = Transaction;
        return send_formatted(ss->fd, "+OK User authenticated, proceed\r\n") <= 0 ? 1 : 0;
    } else {
        return send_formatted(ss->fd, "-ERR Invalid password\r\n") <= 0 ? 1 : 1;
    }
}

// int do_stat(serverstate *ss) {
//     dlog("Executing stat\n");
//     // TODO: Implement this function
//     return 0;
// }

int do_stat(serverstate *ss){
    if(ss->state!=Transaction){
        return send_formatted(ss->fd,"-ERR STAT command only allowed in TRANSACTION state\r\n")<=0?1:1;
    }
    mail_list_t mail_list=load_user_mail(ss->current_user);
    if(!mail_list){
        return send_formatted(ss->fd,"-ERR Could not retrieve maildrop\r\n")<=0?1:1;
    }
    int num_messages=mail_list_length(mail_list,0);
    size_t total_size=mail_list_size(mail_list);
    return send_formatted(ss->fd,"+OK %d %zu\r\n",num_messages,total_size)<=0?1:0;
}


int do_list(serverstate *ss) {
    dlog("Executing list\n");
    // TODO: Implement this function
    return 0;
}

int do_retr(serverstate *ss) {
    dlog("Executing retr\n");
    // TODO: Implement this function
    return 0;
}

int do_rset(serverstate *ss) {
    dlog("Executing rset\n");
    // TODO: Implement this function
    return 0;
}

// int do_noop(serverstate *ss) {
//     dlog("Executing NOOP command\n");
//     if (send_formatted(ss->fd, "+OK\r\n") <= 0) {
//         return 1;  // Return 1 to indicate that the command was unsuccessful
//     }
//     return 0;  // Return 0 to indicate that the command was successful
// }

int do_noop(serverstate *ss) {
    dlog("Executing NOOP command\n");
    // Ensure that the command is only allowed in the TRANSACTION state
    if (ss->state != Transaction) {
        // Send error response if not in TRANSACTION state
        send_formatted(ss->fd, "-ERR Command NOOP only allowed in TRANSACTION state\r\n"); 
        // No need to check return value since indicating failure anyway
        return 1;  // Indicate failure
    }
    // Send OK response if in the correct state
    if (send_formatted(ss->fd, "+OK\r\n") <= 0) {
        return 1;  // Indicate failure
    }
    return 0;  // Indicate success
}


int do_dele(serverstate *ss) {
    dlog("Executing dele\n");
    // TODO: Implement this function
    return 0;
}

void handle_client(void *new_fd) {
    int fd = *(int *)(new_fd);

    size_t len;
    serverstate mstate, *ss = &mstate;

    ss->fd = fd;
    ss->nb = nb_create(fd, MAX_LINE_LENGTH);
    ss->state = Authorization;
    uname(&ss->my_uname);
    // TODO: Initialize additional fields in `serverstate`, if any
    if (send_formatted(fd, "+OK POP3 Server on %s ready\r\n", ss->my_uname.nodename) <= 0) return;

    while ((len = nb_read_line(ss->nb, ss->recvbuf)) >= 0) {
        if (ss->recvbuf[len - 1] != '\n') {
            // command line is too long, stop immediately
            send_formatted(fd, "-ERR Syntax error, command unrecognized\r\n");
            break;
        }
        if (strlen(ss->recvbuf) < len) {
            // received null byte somewhere in the string, stop immediately.
            send_formatted(fd, "-ERR Syntax error, command unrecognized\r\n");
            break;
        }
        // Remove CR, LF and other space characters from end of buffer
        while (isspace(ss->recvbuf[len - 1])) ss->recvbuf[--len] = 0;

        dlog("%x: Command is %s\n", fd, ss->recvbuf);
        if (strlen(ss->recvbuf) == 0) {
            send_formatted(fd, "-ERR Syntax error, blank command unrecognized\r\n");
            break;
        }
        // Split the command into its component "words"
        ss->nwords = split(ss->recvbuf, ss->words);
        char *command = ss->words[0];

        /* TODO: Handle the different values of `command` and dispatch it to the correct implementation
         *  TOP, UIDL, APOP commands do not need to be implemented and therefore may return an error response */
        int response = handle_command(ss, command);
        if (response == -1) {
            // Server should exit
            dlog("Server should exit. received -1 from handle_command\n");
            break;
        } else if (response == 1) {
            // Command was unsuccessful
            // send_formatted(fd, "-ERR Command not recognized\r\n");
            dlog("Received 1 from handle_command\n");
        } else {
            // Command was successful
            // send_formatted(fd, "+OK Command successful\r\n");
            dlog("Received 0 from handle_command\n");
        }
    }
    // TODO: Clean up fields in `serverstate`, if required
    nb_destroy(ss->nb);
    close(fd);
    free(new_fd);
}

int handle_command(serverstate *ss, const char *command) {
    // USER command can be handled in Authorization state
    if (strcasecmp(command, "USER") == 0) {
        // Pass NULL if no username is supplied
        return do_user(ss, ss->words[1] ? ss->words[1] : NULL);
    }
    // QUIT command can be handled in any state
    else if (strcasecmp(command, "QUIT") == 0) {
        return do_quit(ss);
    }
    // PASS command can be handled in Authorization state
    else if (strcmp(command, "PASS") == 0) {
        return do_pass(ss, ss->words[1] ? ss->words[1] : NULL);
    }
    // STAT command can be handled in Transaction state
    else if (strcmp(command, "STAT") == 0) {
        return do_stat(ss);
    }
    // LIST command can be handled in Transaction state
    else if (strcmp(command, "LIST") == 0) {
        if (checkstate(ss, Transaction)) {
            // Handle LIST command
        }
    }
    // RETR command can be handled in Transaction state
    else if (strcmp(command, "RETR") == 0) {
        if (checkstate(ss, Transaction)) {
            // Handle RETR command
        }
    }
    // RSET command can be handled in Transaction state
    else if (strcmp(command, "RSET") == 0) {
        if (checkstate(ss, Transaction)) {
            // Handle RSET command
        }
    }
    // NOOP command can be handled in Transaction state
    else if (strcasecmp(command, "NOOP") == 0) {
        return do_noop(ss);
    } else {
        // Command not recognized
        send_formatted(ss->fd, "-ERR Command not recognized\r\n");
        return 1;
    }
    return 1;
}

