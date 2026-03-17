#include "server.h"

int main() {
    Server server("src/logs/basic_append.log");
    server.run();
    return 0;
}
