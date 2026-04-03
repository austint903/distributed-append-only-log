#include <iostream>
#include "../../../vendor/httplib.h"

static const char* HOST = "localhost";
static const int PORT = 8080;

void usage() {
    std::cerr << "Usage:\n"
              << "  dlog produce --topic <topic> <message>\n"
              << "  dlog consume --topic <topic> --offset <offset>\n"
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
    else {
        usage();
        return 1;
    }

    return 0;
}
