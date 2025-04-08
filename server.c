#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>

#define PORT 12345
#define MAX_CLIENTS 5

int clients[MAX_CLIENTS];
char usernames[MAX_CLIENTS][32];
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void send_to_all(char *msg, int sender) {
    pthread_mutex_lock(&lock);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] != 0 && clients[i] != sender) {
            send(clients[i], msg, strlen(msg), 0);
        }
    }
    pthread_mutex_unlock(&lock);
}

void *handle_client(void *arg) {
    int client = *(int *)arg;
    free(arg);

    char name[32];
    recv(client, name, sizeof(name), 0); // get username

    // Save username
    pthread_mutex_lock(&lock);
    int index = -1;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] == client) {
            strcpy(usernames[i], name);
            index = i;
            break;
        }
    }
    pthread_mutex_unlock(&lock);

    // Send join message
    char join_msg[128];
    sprintf(join_msg, "%s joined the chat.\n", name);
    send_to_all(join_msg, client);
    printf("%s", join_msg);

    // Receive messages
    char msg[512];
    while (1) {
        int bytes = recv(client, msg, sizeof(msg) - 1, 0);
        if (bytes <= 0) break;
        msg[bytes] = '\0';

        char full_msg[600];
        sprintf(full_msg, "[%s]: %s", name, msg);
        send_to_all(full_msg, client);
        printf("%s", full_msg);
    }

    // Remove on disconnect
    pthread_mutex_lock(&lock);
    clients[index] = 0;
    usernames[index][0] = '\0';
    pthread_mutex_unlock(&lock);

    char leave_msg[128];
    sprintf(leave_msg, "%s left the chat.\n", name);
    send_to_all(leave_msg, client);
    printf("%s", leave_msg);
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

        pthread_mutex_lock(&lock);
        int added = 0;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i] == 0) {
                clients[i] = client;
                added = 1;
                break;
            }
        }
        pthread_mutex_unlock(&lock);

        if (!added) {
            char *full = "Server full.\n";
            send(client, full, strlen(full), 0);
            close(client);
            continue;
        }

        int *pclient = malloc(sizeof(int));
        *pclient = client;
        pthread_t t;
        pthread_create(&t, NULL, handle_client, pclient);
        pthread_detach(t);
    }

    return 0;
}
