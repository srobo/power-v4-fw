#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#include "msg_handler.h"
#include "global_vars.h"
#include "led.h"

static char* itoa(int value, char* string);

int parse_msg(char* buf, char* response, int max_len)
{
    char temp_str[11] = {0};  // for doing itoa conversions
    response[0] = '\0';  // make a blank string

    char* next_arg = strtok(buf, ":");
    if (strcmp(next_arg, "OUT") == 0) {
        next_arg = strtok(NULL, ":");
        if (next_arg == NULL) {
            strncat(response, "NACK:Missing output number", max_len);
            return strlen(response);
        }

        int output_num;

        if (isdigit((int)next_arg[0])) {
            output_num = atoi(next_arg);
            // bounds check
            if (output_num < 0 || output_num > OUT_5V) {
                strncat(response, "NACK:Invalid output number", max_len);
                return strlen(response);
            }
        } else {
            strncat(response, "NACK:Missing output number", max_len);
            return strlen(response);
        }

        next_arg = strtok(NULL, ":");
        if (next_arg == NULL) {
            strncat(response, "NACK:Missing output command", max_len);
            return strlen(response);
        }

        if (strcmp(next_arg, "SET") == 0) {
            next_arg = strtok(NULL, ":");
            if (next_arg == NULL) {
                strncat(response, "NACK:Missing output enable argument", max_len);
                return strlen(response);
            }

            // Enable output
            if (next_arg[0] == '1') {
                enable_output(output_num, true);

                strncat(response, "ACK", max_len);
                return strlen(response);
            } else if (next_arg[0] == '0') {
                enable_output(output_num, false);

                strncat(response, "ACK", max_len);
                return strlen(response);
            }

            strncat(response, "NACK:Invalid output enable argument", max_len);
            return strlen(response);
        } else if (strcmp(next_arg, "GET?") == 0) {
            strncat(response, output_enabled(output_num)?"1":"0", max_len);
            return strlen(response);
        } else if (strcmp(next_arg, "I?") == 0) {
            if (output_num == OUT_5V) {
                strncat(response, itoa(reg_5v.current, temp_str), max_len);
            } else {
                strncat(response, itoa(output_current[output_num], temp_str), max_len);
            }
            return strlen(response);
        } else {
            strncat(response, "NACK:Unknown output command", max_len);
            return strlen(response);
        }
    } else if (strcmp(next_arg, "LED") == 0) {
        next_arg = strtok(NULL, ":");
        if (next_arg == NULL) {
            strncat(response, "NACK:Missing LED name", max_len);
            return strlen(response);
        }

        uint32_t led;

        if (strcmp(next_arg, "RUN") == 0) {
            led = LED_RUN;
        } else if (strcmp(next_arg, "ERR") == 0) {
            led = LED_ERROR;
        } else {
            strncat(response, "NACK:Invalid LED name", max_len);
            return strlen(response);
        }

        next_arg = strtok(NULL, ":");
        if (next_arg == NULL) {
            strncat(response, "NACK:Missing LED argument", max_len);
            return strlen(response);
        }

        if (strcmp(next_arg, "SET") == 0) {
            next_arg = strtok(NULL, ":");
            if (next_arg == NULL) {
                strncat(response, "NACK:Missing LED value", max_len);
                return strlen(response);
            }

            if (strcmp(next_arg, "0") == 0) {
                /// TODO disable LED flashing
                clear_led(led);
                strncat(response, "ACK", max_len);
                return strlen(response);
            } else if (strcmp(next_arg, "1") == 0) {
                /// TODO disable LED flashing
                set_led(led);
                strncat(response, "ACK", max_len);
                return strlen(response);
            } else if (strcmp(next_arg, "F") == 0) {
                /// TODO enable LED flashing
                toggle_led(led);
                strncat(response, "ACK", max_len);
                return strlen(response);
            }

            strncat(response, "NACK:Invalid LED value", max_len);
            return strlen(response);
        }
        strncat(response, "NACK:Invalid LED argument", max_len);
        return strlen(response);
    } else if (strcmp(next_arg, "BATT") == 0) {
        next_arg = strtok(NULL, ":");
        if (next_arg == NULL) {
            strncat(response, "NACK:Missing argument", max_len);
            return strlen(response);
        }
        if (strcmp(next_arg, "I?") == 0) {
            // Get stored current value
            strncat(response, itoa(battery.current, temp_str), max_len);
            return strlen(response);
        } else if (strcmp(next_arg, "V?") == 0) {
            // Get stored voltage value
            strncat(response, itoa(battery.voltage, temp_str), max_len);
            return strlen(response);
        }
    } else if (strcmp(next_arg, "BTN") == 0) {
        next_arg = strtok(NULL, ":");
        if (next_arg == NULL) {
            strncat(response, "NACK:Missing button name", max_len);
            return strlen(response);
        }

        if (strcmp(next_arg, "START") == 0) {
            next_arg = strtok(NULL, ":");
            if (next_arg == NULL) {
                strncat(response, "NACK:Missing button command", max_len);
                return strlen(response);
            }

            if (strcmp(next_arg, "GET?") == 0) {
                strncat(response, int_button_pressed?"1":"0", max_len);
                strncat(response, ":", max_len - strlen(response));
                strncat(response, ext_button_pressed?"1":"0", max_len - strlen(response));

                // Clear button state
                int_button_pressed = false;
                ext_button_pressed = false;
                return strlen(response);
            }

            strncat(response, "NACK:Invalid button command", max_len);
            return strlen(response);
        }
        strncat(response, "NACK:Invalid button name", max_len);
        return strlen(response);
    } else if (strcmp(next_arg, "NOTE") == 0) {
        next_arg = strtok(NULL, ":");
        if (next_arg == NULL) {
            strncat(response, "NACK:Missing note frequency", max_len);
            return strlen(response);
        }

        int note_freq;

        if (isdigit((int)next_arg[0])) {
            note_freq = atoi(next_arg);
            // bounds check
            if (note_freq < 0) {
                strncat(response, "NACK:Invalid note frequency", max_len);
                return strlen(response);
            }
        } else {
            strncat(response, "NACK:Invalid note frequency", max_len);
            return strlen(response);
        }

        next_arg = strtok(NULL, ":");
        if (next_arg == NULL) {
            strncat(response, "NACK:Missing note duration", max_len);
            return strlen(response);
        }

        int note_dur;

        if (isdigit((int)next_arg[0])) {
            note_dur = atoi(next_arg);
            // bounds check
            if (note_dur < 0) {
                strncat(response, "NACK:Invalid note duration", max_len);
                return strlen(response);
            }
        } else {
            strncat(response, "NACK:Invalid note duration", max_len);
            return strlen(response);
        }

        /// TODO generate note

        strncat(response, "ACK", max_len);
        return strlen(response);
    } else if (strcmp(next_arg, "*IDN?") == 0) {
        strncat(response, "Student Robotics:", max_len);
        strncat(response, BOARD_NAME_SHORT, max_len - strlen(response));
        strncat(response, ":", max_len - strlen(response));
        strncat(response, (const char *)SERIALNUM_BOOTLOADER_LOC, max_len - strlen(response));
        strncat(response, ":", max_len - strlen(response));
        strncat(response, FW_VER, max_len - strlen(response));
        return strlen(response);
    } else if (strcmp(next_arg, "*STATUS?") == 0) {
        for (output_t out=OUT_H0; out <= OUT_5V; out++) {
            strncat(response, (output_inhibited[out])?"1,":"0,", max_len - strlen(response));
        }
        uint8_t final_char = strlen(response) - 1;
        if (response[final_char] == ',') {  // clear final comma
            response[final_char] = '\0';
        }
        strncat(response, ":", max_len - strlen(response));
        strncat(response, itoa(board_temp, temp_str), max_len);
        return strlen(response);
    } else if (strcmp(next_arg, "*RESET") == 0) {
        for (uint8_t i = 0; i < 7; i++) {
            output_inhibited[i] = false;
            enable_output(i, false);
        }

        // Disable LEDs
        clear_led(LED_RUN);
        clear_led(LED_ERROR);
        /// TODO Disable LED flashes
        /// TODO Disable buzzer
        // Clear button state
        int_button_pressed = false;
        ext_button_pressed = false;
        strncat(response, "ACK", max_len);
        return strlen(response);
    if (strcmp(next_arg, "ECHO") == 0) {
    } else if (strcmp(next_arg, "ECHO") == 0) {
        next_arg = strtok(NULL, ":");

        strncat(response, next_arg, max_len);
        return strlen(response);
    } else {
        strncat(response, "NACK:Unknown command: '", max_len);
        strncat(response, next_arg, max_len - strlen(response));
        strncat(response, "'", max_len - strlen(response));
        return strlen(response);
    }

    // This should be unreachable
    strncat(response, "NACK:Unknown error", max_len);
    return strlen(response);
}

static char* itoa(int value, char* string) {
    // including stdio.h to get sprintf overflows the rom
    char tmp[33];
    char* tmp_ptr = tmp;
    char* sp = string;
    unsigned int digit;
    unsigned int remaining;
    bool sign;

    if ( string == NULL ) {
        return 0;
    }

    sign = (value < 0);
    if (sign) {
        remaining = -value;
    } else {
        remaining = (unsigned int)value;
    }

    while (remaining || tmp_ptr == tmp) {
        digit = remaining % 10;
        remaining /= 10;
        *tmp_ptr = digit + '0';
        tmp_ptr++;
    }

    if (sign) {
        *sp = '-';
        sp++;
    }

    // string is in reverse at this point
    while (tmp_ptr > tmp) {
        tmp_ptr--;
        *sp = *tmp_ptr;
        sp++;
    }
    *sp = '\0';

    return string;
}
