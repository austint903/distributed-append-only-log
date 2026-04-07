#include <iostream>
#include <csignal>
#include <atomic>
#include "../../../vendor/httplib.h"

static const char* HOST = "localhost";
static const int PORT = 9090;

void usage() {
    std::cerr << "Usage:\n"
              << "  dlog produce --topic <topic> <message>\n"
              << "  dlog consume --topic <topic> --offset <offset>\n"
              << "  dlog tail   --topic <topic> [--offset <offset>] [--timeout-ms <ms>]\n"
              << "  dlog topics list\n";
}

// Returns the value of --flag from argv, or "" if not found.
static std::string get_flag(int argc, char** argv, const std::string& flag) {
    for (int i = 2; i < argc - 1; i++) {
        if (argv[i] == flag) return argv[i + 1];
    }
    return "";
}

int main(int argc, char** argv) {
    if (argc < 2) {
        usage();
        return 1;
    }

    const std::string command = argv[1];
    httplib::Client client(HOST, PORT);

    if (command == "produce") {
        const std::string topic = get_flag(argc, argv, "--topic");
        if (topic.empty()) {
            std::cerr << "Error: --topic required\n";
            return 1;
        }
        // message is the last non-flag argument
        const std::string message = argv[argc - 1];
        auto res = client.Post("/produce?topic=" + topic, message, "text/plain");
        if (!res) {
            std::cerr << "Error: could not connect to server\n";
            return 1;
        }
        std::cout << res->body << "\n";
    }
    else if (command == "consume") {
        const std::string topic = get_flag(argc, argv, "--topic");
        const std::string offset = get_flag(argc, argv, "--offset");
        if (topic.empty()) {
            std::cerr << "Error: --topic required\n";
            return 1;
        }
        if (offset.empty()) {
            std::cerr << "Error: --offset required\n";
            return 1;
        }
        auto res = client.Get("/consume?topic=" + topic + "&offset=" + offset);
        if (!res) {
            std::cerr << "Error: could not connect to server\n";
            return 1;
        }
        if (res->status == 404) {
            std::cerr << "Not found\n";
            return 1;
        }
        std::cout << res->body << "\n";
    }
    else if (command == "topics") {
        if (argc < 3 || std::string(argv[2]) != "list") {
            usage();
            return 1;
        }
        auto res = client.Get("/topics");
        if (!res) {
            std::cerr << "Error: could not connect to server\n";
            return 1;
        }
        std::cout << res->body << "\n";
    }
    else if (command == "tail") {
        const std::string topic = get_flag(argc, argv, "--topic");
        if (topic.empty()) {
            std::cerr << "Error: --topic required\n";
            return 1;
        }
        const std::string offset_str    = get_flag(argc, argv, "--offset");
        const std::string timeout_str   = get_flag(argc, argv, "--timeout-ms");
        uint64_t    offset     = offset_str.empty()  ? 0     : std::stoull(offset_str);
        int         timeout_ms = timeout_str.empty() ? 30000 : std::stoi(timeout_str);

        client.set_read_timeout(timeout_ms / 1000 + 10, 0);
        client.set_write_timeout(timeout_ms / 1000 + 10, 0);


        std::cerr << "Tailing topic '" << topic << "' from offset " << offset
                  << "... (Ctrl+C to stop)\n";

        while (true) {
            const std::string url = "/tail?topic=" + topic
                                  + "&offset="     + std::to_string(offset)
                                  + "&timeout_ms=" + std::to_string(timeout_ms);
            auto res = client.Get(url);

            if (!res) {
                std::cerr << "Error: could not connect to server\n";
                return 1;
            }
            if (res->status == 200) {
                const std::string seq = res->get_header_value("X-Sequence-Number");
                std::cout << "[seq=" << seq << "] " << res->body << "\n";
                offset = std::stoull(seq) + 1;
            } else if (res->status == 408) {
                continue;
            } else {
                std::cerr << "Error: server returned " << res->status << "\n";
                return 1;
            }
        }
    }
    else {
        usage();
        return 1;
    }

    return 0;
}
