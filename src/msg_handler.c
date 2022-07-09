#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#include "msg_handler.h"

static char* itoa(int value, char* string);

int parse_msg(char* buf, char* response, int max_len)
{
    char temp_str[11] = {0};  // for doing itoa conversions
    response[0] = '\0';  // make a blank string

    char* next_arg = strtok(buf, ":");
    if (strcmp(next_arg, "ECHO") == 0) {
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
