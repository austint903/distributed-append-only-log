#include "producer.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <vector>

int main() {
    const std::string payload = random_payload(32);
    const uint32_t    seq     = 1;

    const size_t pkt_size = sizeof(ProducerPacket) + payload.size();
    std::vector<uint8_t> pkt(pkt_size);
    auto* hdr        = reinterpret_cast<ProducerPacket*>(pkt.data());
    hdr->seq         = htonl(seq);
    hdr->payload_len = htonl(static_cast<uint32_t>(payload.size()));
    std::memcpy(pkt.data() + sizeof(ProducerPacket), payload.data(), payload.size());

    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) { perror("socket"); return 1; }

    struct timeval tv{};
    tv.tv_sec  = TIMEOUT_SEC;
    tv.tv_usec = 0;
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("setsockopt SO_RCVTIMEO");
        close(fd);
        return 1;
    }

    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_port   = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &server.sin_addr);

    printf("Sending payload (%zu bytes): %s\n", payload.size(), payload.c_str());

    while (true) {
        ssize_t sent = sendto(fd, pkt.data(), pkt_size, 0,
                              reinterpret_cast<sockaddr*>(&server), sizeof(server));
        if (sent < 0) { perror("sendto"); close(fd); return 1; }

        AckPacket ack{};
        ssize_t n = recvfrom(fd, &ack, sizeof(ack), 0, nullptr, nullptr);

        if (n < 0) {
            printf("Timeout waiting for ACK, retrying (seq=%u)...\n", seq);
            continue;
        }
        if (n < static_cast<ssize_t>(sizeof(ack))) {
            printf("Short ACK (%zd bytes), retrying...\n", n);
            continue;
        }

        uint32_t acked_seq     = ntohl(ack.producer_seq);
        uint64_t committed_seq = be64toh(ack.committed_seq);

        if (acked_seq != seq) {
            printf("Stale ACK (seq=%u != expected %u), retrying...\n", acked_seq, seq);
            continue;
        }

        printf("Record committed at log offset %llu\n",
               static_cast<unsigned long long>(committed_seq));
        break;
    }

    close(fd);
    return 0;
}
