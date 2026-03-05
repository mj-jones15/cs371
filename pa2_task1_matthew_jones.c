/*
# Student #1: Matthew Jones
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <sys/time.h>

#define MESSAGE_SIZE 16
#define PIPELINE 32
#define DEFAULT_CLIENT_THREADS 4

char *server_ip = "127.0.0.1";
int server_port = 12345;
int num_client_threads = DEFAULT_CLIENT_THREADS;
int num_requests = 1000000;

typedef struct {
    int socket_fd;
    struct sockaddr_in server_addr;

    long tx_cnt;
    long rx_cnt;

} client_thread_data_t;

void *client_thread_func(void *arg) {

    client_thread_data_t *data = (client_thread_data_t *)arg;

    char send_buf[MESSAGE_SIZE] = "ABCDEFGHIJKLMNO";
    char recv_buf[MESSAGE_SIZE];

    socklen_t addr_len = sizeof(data->server_addr);

    // Set a 100 ms receive timeout
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000; // 100 ms
    setsockopt(data->socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    int outstanding = 0;
    long sent = 0;

    while (1) {

        while (outstanding < PIPELINE && sent < num_requests) {

            sendto(data->socket_fd, send_buf, MESSAGE_SIZE, 0,
                   (struct sockaddr *)&data->server_addr, addr_len);

            data->tx_cnt++;
            sent++;
            outstanding++;
        }

        int n = recvfrom(data->socket_fd, recv_buf, MESSAGE_SIZE, 0,
                         NULL, NULL);

        if (n > 0) {
            data->rx_cnt++;
            if (outstanding > 0) {
                outstanding--;
            }
        } else {
            // Timeout occurred
            if (sent >= num_requests && outstanding == 0) {
                break;
            }
            if (sent >= num_requests) {
                // Assume remaining packets were lost
                break;
            }
        }
    }

    close(data->socket_fd);
    return NULL;
}

void run_client() {

    pthread_t threads[num_client_threads];
    client_thread_data_t thread_data[num_client_threads];

    for (int i = 0; i < num_client_threads; i++) {

        thread_data[i].socket_fd = socket(AF_INET, SOCK_DGRAM, 0);

        memset(&thread_data[i].server_addr, 0, sizeof(struct sockaddr_in));
        thread_data[i].server_addr.sin_family = AF_INET;
        thread_data[i].server_addr.sin_port = htons(server_port);

        inet_pton(AF_INET, server_ip,
                  &thread_data[i].server_addr.sin_addr);

        thread_data[i].tx_cnt = 0;
        thread_data[i].rx_cnt = 0;

        pthread_create(&threads[i], NULL,
                       client_thread_func, &thread_data[i]);
    }

    long total_tx = 0;
    long total_rx = 0;

    for (int i = 0; i < num_client_threads; i++) {
        pthread_join(threads[i], NULL);

        total_tx += thread_data[i].tx_cnt;
        total_rx += thread_data[i].rx_cnt;
    }

    printf("Total TX: %ld\n", total_tx);
    printf("Total RX: %ld\n", total_rx);
    printf("Lost Packets: %ld\n", total_tx - total_rx);
}

void run_server() {

    int server_fd = socket(AF_INET, SOCK_DGRAM, 0);

    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(server_port);

    bind(server_fd, (struct sockaddr *)&server_addr,
         sizeof(server_addr));

    char buffer[MESSAGE_SIZE];

    while (1) {

        int n = recvfrom(server_fd, buffer, MESSAGE_SIZE, 0,
                         (struct sockaddr *)&client_addr, &addr_len);

        if (n > 0) {

            sendto(server_fd, buffer, MESSAGE_SIZE, 0,
                   (struct sockaddr *)&client_addr, addr_len);
        }
    }

    close(server_fd);
}

int main(int argc, char *argv[]) {

    if (argc > 1 && strcmp(argv[1], "server") == 0) {

        if (argc > 2) server_ip = argv[2];
        if (argc > 3) server_port = atoi(argv[3]);

        run_server();

    } else if (argc > 1 && strcmp(argv[1], "client") == 0) {

        if (argc > 2) server_ip = argv[2];
        if (argc > 3) server_port = atoi(argv[3]);
        if (argc > 4) num_client_threads = atoi(argv[4]);
        if (argc > 5) num_requests = atoi(argv[5]);

        run_client();

    } else {

        printf("Usage: %s <server|client> [ip port threads requests]\n",
               argv[0]);
    }

    return 0;
}