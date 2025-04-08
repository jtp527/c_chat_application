#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 12345

int sock;

void *receive(void *arg) {
    char msg[512];
    while (1) {
        int bytes = recv(sock, msg, sizeof(msg) - 1, 0);
        if (bytes <= 0) break;
        msg[bytes] = '\0';
        printf("%s", msg);
    }
    return NULL;
}

int main() {
    sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    connect(sock, (struct sockaddr *)&addr, sizeof(addr));

    // Enter username
    char name[32];
    printf("Enter your name: ");
    fgets(name, sizeof(name), stdin);
    name[strcspn(name, "\n")] = 0;
    send(sock, name, strlen(name), 0);

    // Start thread to receive messages
    pthread_t t;
    pthread_create(&t, NULL, receive, NULL);

    // Send messages
    char msg[512];
    while (1) {
        fgets(msg, sizeof(msg), stdin);
        send(sock, msg, strlen(msg), 0);
    }

    close(sock);
    return 0;
}
