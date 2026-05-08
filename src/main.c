#include "server.h"
#include <syslog.h>

int main() {
        init_server("0.0.0.0", 6969);
        start_listening();
        deinit_server();

        return 0;
}
