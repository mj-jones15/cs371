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
#define WINDOW_SIZE 32
#define TIMEOUT_USEC 50000
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

typedef struct {
    int seq;
    char payload[MESSAGE_SIZE];
} packet_t;

long current_time_us() {

    struct timeval tv;
    gettimeofday(&tv, NULL);

    return tv.tv_sec * 1000000LL + tv.tv_usec;
}

void *client_thread_func(void *arg) {

    client_thread_data_t *data = (client_thread_data_t *)arg;

    socklen_t addr_len = sizeof(data->server_addr);

    packet_t window[WINDOW_SIZE];
    long send_time[WINDOW_SIZE];

    int base = 0;
    int next_seq = 0;

    while (data->rx_cnt < num_requests) {

        while (next_seq < base + WINDOW_SIZE &&
               next_seq < num_requests) {

            packet_t pkt;
            pkt.seq = next_seq;

            sendto(data->socket_fd, &pkt, sizeof(pkt), 0,
                   (struct sockaddr *)&data->server_addr, addr_len);

            window[next_seq % WINDOW_SIZE] = pkt;
            send_time[next_seq % WINDOW_SIZE] = current_time_us();

            data->tx_cnt++;
            next_seq++;
        }

        packet_t ack_pkt;

        int n = recvfrom(data->socket_fd, &ack_pkt, sizeof(ack_pkt),
                         MSG_DONTWAIT, NULL, NULL);

        if (n > 0) {

            int ack = ack_pkt.seq;

            if (ack >= base) {
                base = ack + 1;
                data->rx_cnt = base;
            }
        }

        long now = current_time_us();

        if (base < next_seq) {

            if (now - send_time[base % WINDOW_SIZE] > TIMEOUT_USEC) {

                for (int i = base; i < next_seq; i++) {

                    sendto(data->socket_fd,
                           &window[i % WINDOW_SIZE],
                           sizeof(packet_t), 0,
                           (struct sockaddr *)&data->server_addr,
                           addr_len);

                    send_time[i % WINDOW_SIZE] = now;
                }
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

        memset(&thread_data[i].server_addr, 0,
               sizeof(struct sockaddr_in));

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

    if (total_tx == total_rx)
        printf("Reliable transfer successful\n");
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

    packet_t pkt;

    while (1) {

        int n = recvfrom(server_fd, &pkt, sizeof(pkt), 0,
                         (struct sockaddr *)&client_addr,
                         &addr_len);

        if (n > 0) {

            sendto(server_fd, &pkt, sizeof(pkt), 0,
                   (struct sockaddr *)&client_addr,
                   addr_len);
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