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
#if LIBGPIOD_VERSION < 2
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

int getGpio(const char* chipname, const uint32_t line_num)
{
#if LIBGPIOD_VERSION < 2
    gpiod_chip* chip = gpiod_chip_open_by_name(chipname);
    if (!chip)
    {
        fprintf(stderr, "[Utils] Error opening chip GPIO\r\n");
        return -1;
    }

    gpiod_line* line = gpiod_chip_get_line(chip, line_num);
    if (!line)
    {
        fprintf(stderr, "[Utils] Error getting GPIO line %d\r\n", line_num);
        gpiod_chip_close(chip);
        return -1;
    }

    if (gpiod_line_request_input(line, "GnssHat") < 0)
    {
        fprintf(stderr, "[Utils] Error requesting GPIO line %d as input\r\n", line_num);
        gpiod_chip_close(chip);
        return -1;
    }

    int value = gpiod_line_get_value(line);
    gpiod_chip_close(chip);
    return value;
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
        return -1;
    }

    settings = gpiod_line_settings_new();
    if (!settings)
    {
        fprintf(stderr, "[Utils] Error creating line settings\r\n");
        gpiod_chip_close(chip);
        return -1;
    }

    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_INPUT);

    line_cfg = gpiod_line_config_new();
    if (!line_cfg)
    {
        fprintf(stderr, "[Utils] Error creating line config\r\n");
        gpiod_line_settings_free(settings);
        gpiod_chip_close(chip);
        return -1;
    }

    ret = gpiod_line_config_add_line_settings(line_cfg, &line_num, 1, settings);
    if (ret)
    {
        fprintf(stderr, "[Utils] Error adding line settings\r\n");
        gpiod_line_config_free(line_cfg);
        gpiod_line_settings_free(settings);
        gpiod_chip_close(chip);
        return -1;
    }

    req_cfg = gpiod_request_config_new();
    if (!req_cfg)
    {
        fprintf(stderr, "[Utils] Error creating request config\r\n");
        gpiod_line_config_free(line_cfg);
        gpiod_line_settings_free(settings);
        gpiod_chip_close(chip);
        return -1;
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
        return -1;
    }

    enum gpiod_line_value val = gpiod_line_request_get_value(request, line_num);
    int value = (val == GPIOD_LINE_VALUE_ACTIVE) ? 1 : 0;

    gpiod_line_request_release(request);
    gpiod_request_config_free(req_cfg);
    gpiod_line_config_free(line_cfg);
    gpiod_line_settings_free(settings);
    gpiod_chip_close(chip);
    return value;
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

std::string base64Encode(const std::string &input)
{
    static const char table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((input.size() + 2) / 3) * 4);

    for (size_t i = 0; i < input.size(); i += 3)
    {
        uint32_t v = static_cast<uint8_t>(input[i]) << 16;
        if (i + 1 < input.size()) v |= static_cast<uint8_t>(input[i + 1]) << 8;
        if (i + 2 < input.size()) v |= static_cast<uint8_t>(input[i + 2]);

        out.push_back(table[(v >> 18) & 0x3F]);
        out.push_back(table[(v >> 12) & 0x3F]);
        out.push_back((i + 1 < input.size()) ? table[(v >> 6) & 0x3F] : '=');
        out.push_back((i + 2 < input.size()) ? table[v & 0x3F] : '=');
    }
    return out;
}

std::string base64Decode(const std::string &in)
{
    static const int T[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
        52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
        15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
        41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    };
    std::string out;
    out.reserve(in.size() * 3 / 4);
    int v = 0, bits = -8;
    for (unsigned char c : in)
    {
        if (T[c] < 0) break;
        v = (v << 6) + T[c];
        bits += 6;
        if (bits >= 0)
        {
            out.push_back(static_cast<char>((v >> bits) & 0xFF));
            bits -= 8;
        }
    }
    return out;
}
