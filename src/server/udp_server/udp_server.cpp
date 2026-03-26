#include "udp_server.h"
#include "../../protocol/protocol.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

static constexpr size_t MAX_UDP = 65535;

UdpServer::UdpServer(AppendLog& log) : log_(log) {
    udp_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_fd_ < 0)
        throw std::runtime_error("socket() failed");

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SERVER_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(udp_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(udp_fd_);
        throw std::runtime_error("bind() failed");
    }

    std::cout << "listening on UDP port " << SERVER_PORT << "\n";
}

UdpServer::~UdpServer() {
    if (udp_fd_ >= 0) close(udp_fd_);
}

void UdpServer::run() {
    std::vector<uint8_t> buf(MAX_UDP);

    while (true) {
        sockaddr_in sender{};
        socklen_t   sender_len = sizeof(sender);

        ssize_t n = recvfrom(udp_fd_, buf.data(), buf.size(), 0,
                             reinterpret_cast<sockaddr*>(&sender), &sender_len);
        if (n < 0) { std::cerr << "recvfrom failed\n"; continue; }

        if (static_cast<size_t>(n) < sizeof(ProducerPacket)) {
            printf("Dropping short packet (%zd bytes)\n", n);
            continue;
        }

        ProducerPacket hdr{};
        memcpy(&hdr, buf.data(), sizeof(hdr));
        uint32_t producer_seq = ntohl(hdr.seq);
        uint32_t payload_len  = ntohl(hdr.payload_len);

        if (sizeof(ProducerPacket) + payload_len != static_cast<size_t>(n)) {
            printf("Dropping malformed packet (expected %zu bytes, got %zd)\n",
                   sizeof(ProducerPacket) + payload_len, n);
            continue;
        }

        const uint8_t* payload = buf.data() + sizeof(ProducerPacket);

        uint64_t committed_seq;
        try {
            committed_seq = log_.append_and_seq({ payload, payload_len });
        } catch (const std::exception& e) {
            std::cerr << "append failed: " << e.what() << "\n";
            continue;
        }

        std::cout << "seq=" << producer_seq << " committed at log offset " << committed_seq << "\n";

        AckPacket ack{};
        ack.producer_seq  = htonl(producer_seq);
        ack.committed_seq = htobe64(committed_seq);

        sendto(udp_fd_, &ack, sizeof(ack), 0,
               reinterpret_cast<sockaddr*>(&sender), sender_len);
    }
}
