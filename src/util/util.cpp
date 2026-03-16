#include "util.h"

#include <unistd.h>

char * get_working_directory() {
    return getcwd(nullptr, 0); // malloc's the buffer; caller must free()
}

