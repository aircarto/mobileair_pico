#include "pico/stdlib.h"
#include "version.h"
#include <cstdio>

int main() {
    stdio_init_all();

    // Wait for USB serial connection
    sleep_ms(2000);

    printf("MobileAir Pico 2W - firmware v%s\n", FW_VERSION);

    while (true) {
        printf("Hello from MobileAir Pico 2W!\n");
        sleep_ms(1000);
    }

    return 0;
}
