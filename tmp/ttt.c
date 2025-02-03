#include <stdlib.h>
#include <locale.h>
#include <stdio.h>
#include <stdint.h>
#include <gio/gio.h>

int main(int argc, char **argv)
{
    GSettings *settings;

    g_type_init();
    settings = g_settings_new("org.gnome.settings-daemon.plugins.color");
    GVariant *value = g_settings_get_value(settings, "night-light-temperature");
    uint32_t *result;
    g_variant_get(value, "u", &result);

    printf("Current night-light-temperature value is %u\n", result);

    // g_settings_set(settings, "night-light-temperature", "u", 1000);
    // g_settings_sync();

    return 0;
}

