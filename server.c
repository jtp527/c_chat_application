#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>
#include <ctype.h>
#include <time.h>

#define PORT 12345
#define MAX_CLIENTS 5

int clients[MAX_CLIENTS];
char usernames[MAX_CLIENTS][32];
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

// Get current time as [HH:MM]
void get_time(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, size, "[%H:%M]", tm_info);
}

void send_to_all(char *msg, int sender) {
    pthread_mutex_lock(&lock);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] != 0 && clients[i] != sender) {
            send(clients[i], msg, strlen(msg), 0);
        }
    }
    pthread_mutex_unlock(&lock);
}
int valid_message(char *msg, int sender){
    char* errormessage = "Invalid Input: Message must contain non-space characters.\n";
    pthread_mutex_lock(&lock);
    for (int i = 0; i < strlen(msg); ++i){
        // Check if the character is whitespace
        if  (!isspace(msg[i])){
            pthread_mutex_unlock(&lock);
            return 0;
        }
    }
    // If the message contains only whitespace, send an error message to the sender
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] != 0 && clients[i] == sender) {
            send(clients[i],errormessage , strlen(errormessage), 0);
        }
    }
    pthread_mutex_unlock(&lock);
    return 1;
}


void *handle_client(void *arg) {
    int client = *(int *)arg;
    free(arg);

    char name[32];
    while (1) {
        memset(name, 0, sizeof(name));                        // Clear buffer
        int bytes = recv(client, name, sizeof(name), 0);      // Receive username
        if (bytes <= 0) {
            close(client);
            return NULL;
        }
        name[strcspn(name, "\n")] = 0;                         // Remove newline

        int name_taken = 0;
        pthread_mutex_lock(&lock);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i] != 0 && strcmp(usernames[i], name) == 0) {
                name_taken = 1;
                break;
            }
        }
        pthread_mutex_unlock(&lock);

        if (name_taken) {
            char *msg = "Username taken. Try another:\n";
            send(client, msg, strlen(msg), 0);                // Ask for another
        }
        else if (valid_message(name, client) == 1) {
            // If the message isn't ONLY whitespace, send it to client
            char *msg = "Invalid Input: Username must contain non-space characters.\n";
            send(client, msg, strlen(msg), 0);                // Ask for another
        } 
        else {
            break; // Valid username
        }
    }
    char *ok = "Welcome to the chat!\n";
    send(client, ok, strlen(ok), 0);
    
    // Add client
    pthread_mutex_lock(&lock);
    int index = -1;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] == 0) {
            clients[i] = client;
            strcpy(usernames[i], name);
            index = i;
            break;
        }
    }
    pthread_mutex_unlock(&lock);

    // Announce join
    char timebuf[16];
    char join_msg[128];
    get_time(timebuf, sizeof(timebuf));
    sprintf(join_msg, "%s %s joined the chat.\n", timebuf, usernames[index]);
    send_to_all(join_msg, client);
    printf("%s", join_msg);

    // Chat loop
    char msg[512];
    while (1) {
        int bytes = recv(client, msg, sizeof(msg) - 1, 0);
        if (bytes <= 0) break;
        msg[bytes] = '\0';

        if (strncmp(msg, "/quit", 5) == 0) {
            break;
        }

        char full_msg[600];
        get_time(timebuf, sizeof(timebuf));
        sprintf(full_msg, "%s [%s]: %s", timebuf, usernames[index], msg);
        if (valid_message(msg, client) == 0){
            // If the message isn't ONLY whitespace, send it to all clients
            send_to_all(full_msg, client);
            printf("%s", full_msg);
        }
        
    }

    // Remove client
    get_time(timebuf, sizeof(timebuf));
    char leave_msg[128];
    sprintf(leave_msg, "%s %s left the chat.\n", timebuf, usernames[index]);
    send_to_all(leave_msg, client);
    printf("%s", leave_msg);

    pthread_mutex_lock(&lock);
    clients[index] = 0;
    usernames[index][0] = '\0';
    pthread_mutex_unlock(&lock);


    close(client);
    return NULL;
}

int main() {
    int server = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(server, (struct sockaddr *)&addr, sizeof(addr));
    listen(server, MAX_CLIENTS);
    printf("Server started on port %d\n", PORT);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t len = sizeof(client_addr);
        int client = accept(server, (struct sockaddr *)&client_addr, &len);

        int *pclient = malloc(sizeof(int));
        *pclient = client;
        pthread_t t;
        pthread_create(&t, NULL, handle_client, pclient);
        pthread_detach(t);
    }

    return 0;
}
