#include "pico/stdlib.h"

#define LED 15             // external LED in the diagram, through 330 ohms
#define BUTTON 14          // pushbutton to ground, active low
#define UNIT_MS 200
#define POLL_MS 5

// Check release at least every 5 ms, including during a dash or gap.
static bool wait_held(uint32_t ms) {
    while (ms > 0) {
        if (gpio_get(BUTTON)) return false;
        uint32_t tick = ms < POLL_MS ? ms : POLL_MS;
        sleep_ms(tick);
        ms -= tick;
    }
    return gpio_get(BUTTON) == 0;
}

static bool flash(uint32_t on_ms) {
    gpio_put(LED, 1);
    bool held = wait_held(on_ms);
    gpio_put(LED, 0);       // always off on cancellation
    return held;
}

static bool play_sos(void) {
    const char *sos[] = { "...", "---", "..." };
    for (int i = 0; i < 3; ++i) {
        for (const char *s = sos[i]; *s; ++s) {
            if (!flash(*s == '.' ? UNIT_MS : 3 * UNIT_MS)) return false;
            if (s[1] && !wait_held(UNIT_MS)) return false;
        }
        if (!wait_held((i == 2 ? 7 : 3) * UNIT_MS)) return false;
    }
    return true;
}

int main(void) {
    gpio_init(LED);
    gpio_set_dir(LED, GPIO_OUT);
    gpio_put(LED, 0);
    gpio_init(BUTTON);
    gpio_set_dir(BUTTON, GPIO_IN);
    gpio_pull_up(BUTTON);
    while (true) {
        // Require a stable press before starting; repeat while held.
        if (gpio_get(BUTTON) == 0 && wait_held(20)) play_sos();
        gpio_put(LED, 0);
        sleep_ms(POLL_MS);
    }
}
