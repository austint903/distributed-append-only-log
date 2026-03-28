#include <iostream>
#include "../../../vendor/httplib.h"

static const char* HOST = "localhost";
static const int PORT = 8080;

void usage() {
    std::cerr << "Usage:\n"
              << "  dlog produce <message>\n"
              << "  dlog consume <seq>\n";
}

int main(int argc, char** argv) {
    if (argc < 3) {
        usage();
        return 1;
    }

    const std::string command = argv[1];
    httplib::Client client(HOST, PORT);

    if (command == "produce") {
        const std::string message = argv[2];
        auto res = client.Post("/produce", message, "text/plain");
        if (!res) {
            std::cerr << "Error: could not connect to server\n";
            return 1;
        }
        std::cout << res->body << "\n";
    }
    else if (command == "consume") {
        const std::string seq = argv[2];
        auto res = client.Get("/consume?seq=" + seq);
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
    else {
        usage();
        return 1;
    }

    return 0;
}
