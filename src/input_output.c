#include <driver/gpio.h>
#include "input_output.h"
#include <nvs_flash.h>

static nvs_handle_t storage_open(nvs_open_mode_t mode) {
    esp_err_t err;
    nvs_handle_t my_handle;
    err = nvs_open("storage", mode, &my_handle);
    if (err != 0) {
        nvs_flash_init();
        err = nvs_open("storage", mode, &my_handle);
    }
    return my_handle;
}

int storage_read_int(char *name, int def) {
    nvs_handle_t handle = storage_open(NVS_READONLY);
    int32_t val = def;
    nvs_get_i32(handle, name, &val);
    nvs_close(handle);
    return val;
}

void storage_write_int(char *name, int val) {
    nvs_handle_t handle = storage_open(NVS_READWRITE);
    nvs_set_i32(handle, name, val);
    nvs_commit(handle);
    nvs_close(handle);
}

void input_output_init() {
    gpio_set_direction(1, GPIO_MODE_INPUT);
    gpio_set_pull_mode(1, GPIO_PULLUP_ONLY);

    gpio_set_direction(2, GPIO_MODE_INPUT);
    gpio_set_pull_mode(2, GPIO_PULLUP_ONLY);

    gpio_set_direction(3, GPIO_MODE_INPUT);
    gpio_set_pull_mode(3, GPIO_PULLUP_ONLY);
}
