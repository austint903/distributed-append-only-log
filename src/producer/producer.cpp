#include "producer.h"

#include <random>

std::string random_payload(size_t len) {
    static const char charset[] =
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "0123456789";
    std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<size_t> dist(0, sizeof(charset) - 2);
    std::string s(len, '\0');
    for (char& c : s) c = charset[dist(rng)];
    return s;
}
