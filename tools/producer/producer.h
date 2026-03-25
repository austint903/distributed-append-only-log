#ifndef PRODUCER_H
#define PRODUCER_H

#include <string>
#include "../../src/protocol/protocol.h"

static constexpr const char* SERVER_IP  = "127.0.0.1";
static constexpr int         TIMEOUT_SEC = 2;

std::string random_payload(size_t len);

#endif // PRODUCER_H
