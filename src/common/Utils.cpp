/*
 * Jimmy Paputto 2021
 */

#include "Utils.hpp"

#include <chrono>
#include <cstdio>
#include <thread>

#include <gpiod.h>


bool try3times(std::function<bool()> task)
{
    bool result = false;
    for (uint8_t i = 0; i < 3; i++)
    {
        result = task();
        if (result)
        {
            break;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return result;
}

void setGpio(const char* chipname, const uint32_t line_num, int value)
{
#if LIBGPIO_VERSION < 2
    gpiod_chip* chip = gpiod_chip_open_by_name(chipname);
    if (!chip)
    {
        fprintf(stderr, "[Utils] Error opening chip GPIO\r\n");
        return;
    }
    
    gpiod_line* line = gpiod_chip_get_line(chip, line_num);
    if (!line)
    {
        fprintf(stderr, "[Utils] Error getting GPIO line %d\r\n", line_num);
        gpiod_chip_close(chip);
        return;
    }

    if (gpiod_line_request_output(line, "GnssHat", 1) < 0)
    {
        fprintf(stderr, "[Utils] Error requesting GPIO line %d as output\r\n", line_num);
        gpiod_chip_close(chip);
        return;
    }

    gpiod_line_set_value(line, value);
    gpiod_chip_close(chip);
#else
    struct gpiod_chip *chip;
    struct gpiod_line_settings *settings;
    struct gpiod_line_config *line_cfg;
    struct gpiod_request_config *req_cfg;
    struct gpiod_line_request *request;
    int ret;
    
    chip = gpiod_chip_open(chipname);
    if (!chip)
    {
        fprintf(stderr, "[Utils] Error opening GPIO chip %s\r\n", chipname);
        return;
    }
    
    settings = gpiod_line_settings_new();
    if (!settings)
    {
        fprintf(stderr, "[Utils] Error creating line settings\r\n");
        gpiod_chip_close(chip);
        return;
    }
    
    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);
    gpiod_line_settings_set_output_value(settings, 
        value ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE);
    
    line_cfg = gpiod_line_config_new();
    if (!line_cfg)
    {
        fprintf(stderr, "[Utils] Error creating line config\r\n");
        gpiod_line_settings_free(settings);
        gpiod_chip_close(chip);
        return;
    }
    
    ret = gpiod_line_config_add_line_settings(line_cfg, &line_num, 1, settings);
    if (ret)
    {
        fprintf(stderr, "[Utils] Error adding line settings\r\n");
        gpiod_line_config_free(line_cfg);
        gpiod_line_settings_free(settings);
        gpiod_chip_close(chip);
        return;
    }
    
    req_cfg = gpiod_request_config_new();
    if (!req_cfg)
    {
        fprintf(stderr, "[Utils] Error creating request config\r\n");
        gpiod_line_config_free(line_cfg);
        gpiod_line_settings_free(settings);
        gpiod_chip_close(chip);
        return;
    }
    
    gpiod_request_config_set_consumer(req_cfg, "GnssHat");
    
    request = gpiod_chip_request_lines(chip, req_cfg, line_cfg);
    if (!request)
    {
        fprintf(stderr, "[Utils] Error requesting GPIO line %d\r\n", line_num);
        gpiod_request_config_free(req_cfg);
        gpiod_line_config_free(line_cfg);
        gpiod_line_settings_free(settings);
        gpiod_chip_close(chip);
        return;
    }

    gpiod_line_request_release(request);
    gpiod_request_config_free(req_cfg);
    gpiod_line_config_free(line_cfg);
    gpiod_line_settings_free(settings);
    gpiod_chip_close(chip);
#endif
}

bool getBit(uint8_t val, uint8_t pos)
{
    return 1 == ((val >> pos) & 1);
}

void setBit(uint32_t& val, uint8_t pos, bool bit)
{
    val = bit ? val | (1 << pos) : val & ~(1 << pos);
}
