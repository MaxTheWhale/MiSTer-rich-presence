#include <stdint.h>
#include <stddef.h>

int network_init(uint16_t port);

void network_accept();

void network_broadcast(const void *message, size_t length);
