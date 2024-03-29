#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#include "msg_handler.h"
#include "global_vars.h"
#include "output.h"
#include "led.h"
#include "fan.h"
#include "buzzer.h"

static char* itoa(int value, char* string);

static void append_str(char* dest, const char* src, int dest_max_len) {
    strncat(dest, src, dest_max_len - strlen(dest));
}
static char* get_next_arg(char* response, const char* err_msg, int max_len) {
    char* next_arg = strtok(NULL, ":");
    if (next_arg == NULL) {
        strncat(response, err_msg, max_len);
        return NULL;
    }
    return next_arg;
}

void handle_msg(char* buf, char* response, int max_len) {
    // max_len is the maximum length of the string that can be fitted in buf
    // so the buffer must be at least max_len+1 long
    char temp_str[12] = {0};  // for doing itoa conversions
    response[0] = '\0';  // make a blank string

    char* next_arg = strtok(buf, ":");
    if (strcmp(next_arg, "OUT") == 0) {
        next_arg = get_next_arg(response, "NACK:Missing output number", max_len);
        if(next_arg == NULL) {return;}

        unsigned long int output_num;

        if (isdigit((int)next_arg[0])) {
            output_num = strtoul(next_arg, NULL, 10);
            // bounds check
            if (output_num > OUT_5V) {
                append_str(response, "NACK:Invalid output number", max_len);
                return;
            }
        } else {
            append_str(response, "NACK:Missing output number", max_len);
            return;
        }

        next_arg = get_next_arg(response, "NACK:Missing output command", max_len);
        if(next_arg == NULL) {return;}

        if (strcmp(next_arg, "SET") == 0) {
            // inhibit setting brain port
            if (output_num == BRAIN_OUTPUT) {
                append_str(response, "NACK:Brain output cannot be controlled", max_len);
                return;
            }
            next_arg = get_next_arg(response, "NACK:Missing output enable argument", max_len);
            if(next_arg == NULL) {return;}

            // Enable output
            if (next_arg[0] == '1') {
                enable_output(output_num, true);

                append_str(response, "ACK", max_len);
                return;
            } else if (next_arg[0] == '0') {
                enable_output(output_num, false);

                append_str(response, "ACK", max_len);
                return;
            }

            append_str(response, "NACK:Invalid output enable argument", max_len);
            return;
        } else if (strcmp(next_arg, "GET?") == 0) {
            append_str(response, output_enabled(output_num)?"1":"0", max_len);
            return;
        } else if (strcmp(next_arg, "I?") == 0) {
            if (output_num == OUT_5V) {
                append_str(response, itoa(reg_5v.current, temp_str), max_len);
            } else {
                append_str(response, itoa(output_current[output_num], temp_str), max_len);
            }
            return;
        } else {
            append_str(response, "NACK:Unknown output command", max_len);
            return;
        }
    } else if (strcmp(next_arg, "LED") == 0) {
        next_arg = get_next_arg(response, "NACK:Missing LED name", max_len);
        if(next_arg == NULL) {return;}

        uint32_t led;

        if (strcmp(next_arg, "RUN") == 0) {
            led = LED_RUN;
        } else if (strcmp(next_arg, "ERR") == 0) {
            led = LED_ERROR;
        } else {
            append_str(response, "NACK:Invalid LED name", max_len);
            return;
        }

        next_arg = get_next_arg(response, "NACK:Missing LED argument", max_len);
        if(next_arg == NULL) {return;}

        if (strcmp(next_arg, "SET") == 0) {
            next_arg = get_next_arg(response, "NACK:Missing LED value", max_len);
            if(next_arg == NULL) {return;}

            if (strcmp(next_arg, "0") == 0) {
                clear_led(led);
                append_str(response, "ACK", max_len);
                return;
            } else if (strcmp(next_arg, "1") == 0) {
                set_led(led);
                append_str(response, "ACK", max_len);
                return;
            } else if (strcmp(next_arg, "F") == 0) {
                toggle_led(led);
                set_led_flash(led);
                append_str(response, "ACK", max_len);
                return;
            }

            append_str(response, "NACK:Invalid LED value", max_len);
            return;
        } else if (strcmp(next_arg, "GET?") == 0) {
            switch(get_led_state(led)) {
                case 0:
                    append_str(response, "0", max_len);
                    return;
                case 1:
                    append_str(response, "1", max_len);
                    return;
                case 2:
                    append_str(response, "F", max_len);
                    return;
                default:
                    append_str(response, "NACK:Failed to get pin state", max_len);
                    return;
            }
        }
        append_str(response, "NACK:Invalid LED argument", max_len);
        return;
    } else if (strcmp(next_arg, "BATT") == 0) {
        next_arg = get_next_arg(response, "NACK:Missing argument", max_len);
        if(next_arg == NULL) {return;}

        if (strcmp(next_arg, "I?") == 0) {
            // Get stored current value
            append_str(response, itoa(battery.current, temp_str), max_len);
            return;
        } else if (strcmp(next_arg, "V?") == 0) {
            // Get stored voltage value
            append_str(response, itoa(battery.voltage, temp_str), max_len);
            return;
        }
        append_str(response, "NACK:Unknown battery command", max_len);
        return;
    } else if (strcmp(next_arg, "BTN") == 0) {
        next_arg = get_next_arg(response, "NACK:Missing button name", max_len);
        if(next_arg == NULL) {return;}

        if (strcmp(next_arg, "START") == 0) {
            next_arg = get_next_arg(response, "NACK:Missing button command", max_len);
            if(next_arg == NULL) {return;}

            if (strcmp(next_arg, "GET?") == 0) {
                append_str(response, int_button_pressed?"1":"0", max_len);
                append_str(response, ":", max_len);
                append_str(response, ext_button_pressed?"1":"0", max_len);

                // Clear button state
                int_button_pressed = false;
                ext_button_pressed = false;
                return;
            }

            append_str(response, "NACK:Invalid button command", max_len);
            return;
        }
        append_str(response, "NACK:Invalid button name", max_len);
        return;
    } else if (strcmp(next_arg, "NOTE") == 0) {
        next_arg = get_next_arg(response, "NACK:Missing note frequency", max_len);
        if(next_arg == NULL) {return;}

        unsigned long int note_freq;

        if (isdigit((int)next_arg[0])) {
            note_freq = strtoul(next_arg, NULL, 10);
            // bounds check
            if (note_freq > UINT16_MAX) {
                append_str(response, "NACK:Invalid note frequency", max_len);
                return;
            }
        } else if (strcmp(next_arg, "GET?") == 0) {
            append_str(response, itoa(buzzer_get_freq(), temp_str), max_len);
            append_str(response, ":", max_len);
            append_str(response, itoa(buzzer_remaining(), temp_str), max_len);
            return;
        } else {
            append_str(response, "NACK:Invalid note frequency", max_len);
            return;
        }

        next_arg = get_next_arg(response, "NACK:Missing note duration", max_len);
        if(next_arg == NULL) {return;}

        unsigned long int note_dur;

        if (isdigit((int)next_arg[0])) {
            note_dur = strtoul(next_arg, NULL, 10);
            // bounds check
            if (note_dur > UINT32_MAX) {
                append_str(response, "NACK:Invalid note duration", max_len);
                return;
            }
        } else {
            append_str(response, "NACK:Invalid note duration", max_len);
            return;
        }

        // Generate note
        buzzer_note(note_freq, note_dur);

        append_str(response, "ACK", max_len);
        return;
    } else if (strcmp(next_arg, "*IDN?") == 0) {
        append_str(response, "Student Robotics:" BOARD_NAME_SHORT ":", max_len);
        append_str(response, (const char *)SERIALNUM_BOOTLOADER_LOC, max_len);
        append_str(response, ":" FW_VER, max_len);
        return;
    } else if (strcmp(next_arg, "*STATUS?") == 0) {
        for (output_t out=OUT_H0; out <= OUT_5V; out++) {
            append_str(response, (output_inhibited[out])?"1,":"0,", max_len);
        }
        uint8_t final_char = strlen(response) - 1;
        if (response[final_char] == ',') {  // clear final comma
            response[final_char] = '\0';
        }
        append_str(response, ":", max_len);
        append_str(response, itoa(board_temp, temp_str), max_len);
        append_str(response, ":", max_len);
        append_str(response, (fan_running()?"1":"0"), max_len);
        append_str(response, ":", max_len);
        append_str(response, itoa(reg_5v.voltage, temp_str), max_len);
        return;
    } else if (strcmp(next_arg, "*RESET") == 0) {
        reset_board();
        append_str(response, "ACK", max_len);
        return;
    } else if (strcmp(next_arg, "*SYS") == 0) {
        next_arg = get_next_arg(response, "NACK:Missing system command", max_len);
        if(next_arg == NULL) {return;}

        if (strcmp(next_arg, "DELAY_COEFF") == 0) {
            next_arg = get_next_arg(response, "NACK:Missing coefficient command", max_len);
            if(next_arg == NULL) {return;}

            if (strcmp(next_arg, "SET") == 0) {
                uint16_t new_coeffs[5];
                unsigned long coeff;

                for(uint8_t i=0; i < 5; i++) {
                    next_arg = get_next_arg(response, "NACK:Missing coefficient set argument", max_len);
                    if(next_arg == NULL) {return;}

                    if (isdigit((int)next_arg[0])) {
                        coeff = strtoul(next_arg, NULL, 10);

                        // bounds check value
                        if (coeff > (UINT16_MAX - 1)) {
                            append_str(response, "NACK:Coefficient must fit in uint16", max_len);
                            return;
                        }
                        // add value to array
                        new_coeffs[i] = coeff;
                    } else {
                        append_str(response, "NACK:Coefficients must be positive integers", max_len);
                        return;
                    }
                }

                ADC_OVERCURRENT_DELAY = new_coeffs[0];
                BATT_OVERCURRENT_DELAY = new_coeffs[1];
                REG_OVERCURRENT_DELAY = new_coeffs[2];
                UVLO_DELAY = new_coeffs[3];
                NEG_CURRENT_DELAY = new_coeffs[4];

                append_str(response, "ACK", max_len);
                return;
            } else if (strcmp(next_arg, "GET?") == 0) {
                append_str(response, itoa(ADC_OVERCURRENT_DELAY, temp_str), max_len);
                append_str(response, ":", max_len);
                append_str(response, itoa(BATT_OVERCURRENT_DELAY, temp_str), max_len);
                append_str(response, ":", max_len);
                append_str(response, itoa(REG_OVERCURRENT_DELAY, temp_str), max_len);
                append_str(response, ":", max_len);
                append_str(response, itoa(UVLO_DELAY, temp_str), max_len);
                append_str(response, ":", max_len);
                append_str(response, itoa(NEG_CURRENT_DELAY, temp_str), max_len);
                return;
            } else {
                append_str(response, "NACK:Unknown coefficient command", max_len);
                return;
            }
        } else if (strcmp(next_arg, "BRAIN") == 0) {
            next_arg = get_next_arg(response, "NACK:Missing brain command", max_len);
            if(next_arg == NULL) {return;}
            if (strcmp(next_arg, "SET") == 0) {
                next_arg = get_next_arg(response, "NACK:Missing brain enable argument", max_len);
                if(next_arg == NULL) {return;}

                // Enable output
                if (next_arg[0] == '1') {
                    enable_output(BRAIN_OUTPUT, true);

                    append_str(response, "ACK", max_len);
                    return;
                } else if (next_arg[0] == '0') {
                    enable_output(BRAIN_OUTPUT, false);

                    append_str(response, "ACK", max_len);
                    return;
                }

                append_str(response, "NACK:Invalid brain enable argument", max_len);
                return;
            } else {
                append_str(response, "NACK:Unknown brain command", max_len);
                return;
            }
        } else if (strcmp(next_arg, "FAN") == 0) {
            next_arg = get_next_arg(response, "NACK:Missing fan command", max_len);
            if(next_arg == NULL) {return;}
            if (strcmp(next_arg, "SET") == 0) {
                next_arg = get_next_arg(response, "NACK:Missing fan override argument", max_len);
                if(next_arg == NULL) {return;}

                if (next_arg[0] == '1') {
                    fan_override = true;

                    append_str(response, "ACK", max_len);
                    return;
                } else if (next_arg[0] == '0') {
                    fan_override = false;

                    append_str(response, "ACK", max_len);
                    return;
                }

                append_str(response, "NACK:Invalid fan override argument", max_len);
                return;
            } else {
                append_str(response, "NACK:Unknown fan command", max_len);
                return;
            }
        }

        append_str(response, "NACK:Invalid system command", max_len);
        return;
    } else if (strcmp(next_arg, "ECHO") == 0) {
        next_arg = strtok(NULL, ":");

        append_str(response, next_arg, max_len);
        return;
    } else {
        append_str(response, "NACK:Unknown command: '", max_len);
        append_str(response, next_arg, max_len);
        append_str(response, "'", max_len);
        return;
    }

    // This should be unreachable
    append_str(response, "NACK:Unknown error", max_len);
    return;
}

static char* itoa(int value, char* string) {
    // string must be a buffer of at least 12 chars
    // including stdio.h to get sprintf overflows the rom
    char tmp[11];
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
