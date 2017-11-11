/**
 * \file            regex.c
 * \brief           Regular expression implementation file
 */

/*
 * Copyright (c) 2017, Tilen MAJERLE
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *  * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *  * Neither the name of the author nor the
 *      names of its contributors may be used to endorse or promote products
 *      derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * \author          Tilen MAJERLE <tilen@majerle.eu>
 */
#include "regex.h"

/*
 * Regular expression library will match these examples:
 *
 * /\\d/g                           Match any number
 * /\\D/g                           Match any NON-number
 * /\\w/g                           Match any alphanumeric
 * /\\W/g                           Match any NON-alphanumeric
 * /\\s/g                           Match any space
 * /\\W/g                           Match any NON-space
 * /[a-z]/g                         Match any lowercase character
 * /[A-Z]/g                         Match any uppercase character
 * /[0-5]/g                         Match any digit between 0 and 5
 * /^[0-9]/g                        Match any digit on beginning of input
 * /ab(cd|ef)ij/g                   Match literal "abcdij" or "abefij" sequence
 * /test/g                          Match literal string "test"
 * /a\\d{1,3}b/                     Match character 'a' followed by digit between 1 and 3 characters followed by 'b' (example: "a1b", "a12b", "a432b")
 * /[^\\d]/                         Match any NON-digit character. This is the same as "/\\D/g
 * /[^\\D]/g                        Match any number. This is the same as "/\\d/g"
 * /.*a/g                           Match any character followed by 'a' 0 or more times
 * /.+/g                            Match any character at least once
 * /^ab[0-9}{1,2}%/g                Match any string starting with "ab" and ending with number between 1 and 2 digits (example: "ab1", "ab12")
 * /[Hh]ello/g                      Match any word "Hello" or "hello"
 * /^[Hh]ello/g                     Match any string starting with "Hello" or "hello"
 * /c{3,10}/g                       Match character 'c' between 3 and 10 times in a row
 * /c?/g                            Match character 'c' 0 or 1 times
 * /\\d?/g                          Match digit 0 or 1 times
 * /[abc]/g                         Match character if it is 'a', 'b' or 'c'
 * /[^abc]/g                        Match character if it is NOT 'a', 'b' or 'c'
 * /a(ab){1,2}c/g                   Match character 'a', followed by sequence "ab" between 1 or 2 times followed by character 'c' (Valid inputs: "aabc" or "aababc")
 *
 * Regular expression library will not work with these examples (behavior is undefined):
 *
 * /a(a|b|c(cd|ef))/g               Match character 'a', followed by either 'a', 'b' or ('c' followed by either 'cd' or 'ef')
 * /(ab|cd)+/g                      Match "ab" or "cd" strings 1 or more times
 *
 * TODO:
 * - Implement unlimited groups /(ab(cd|ef(gdh))+/g
 * - Add option for + and * characters fo
 * - Implement recursive patterns (children of current)
 */

/**
 * \brief           Single pattern entry
 */
typedef regex_pattern_t p_t;

#define RANGE_MAX                               (0x7FFF)

/* List of internal functions */
static uint8_t match_pattern(regex_t* r, const p_t* p, const char* str, uint8_t prev_result);

#define PTR_INC() do { p++, len = len > 0 ? len - 1 : 0; } while (0);

/**
 * List of special character (s, f, w) values
 */
#define IS_DIGIT(x)             ((x) >= '0' && (x) <= '9')
#define IS_SPECIAL_META_CHAR(x) ((x) == 's' || (x) == 'S' || (x) == 'w' || (x) == 'W' || (x) == 'd' || (x) == 'D')
#define IS_SPECIAL_CHAR(x)      ((x) == '^' || (x) == '$' || (x) == '.' || (x) == '*' || (x) == '+' || (x) == '?' || (x) == '|' || (x) == '(' || (x) == ')' || (x) == '{' || (x) == '}' || (x) == '[' || (x) == '}')
#define IS_SPECIAL_MOD_CHAR(x)  (IS_SPECIAL_CHAR(x) && !((x) == '[' || (x) == '(' || (x) == '|' || (x) == ']' || (x) == ')' || (x) == '$'))
#define IS_S_CHAR(x)            ((x) == ' ' || (x) == '\n' || (x) == '\r' || (x) == '\t' || (x) == '\v' || (x) == '\f')
#define IS_D_CHAR(x)            IS_DIGIT(x)
#define IS_W_CHAR(x)            (((x) >= 'a' && (x) <= 'z') || ((x) >= 'A' && (x) <= 'Z') || ((x) >= '0' && (x) <= '9') || (x) == '_')
#define IS_C_UPPER(x)           ((x) >= 'A' && (x) <= 'Z')
#define CHAR_TO_NUM(x)          ((x) - '0')
#define CAN_MATCH_MORE(p)       (!((p[1].type == P_EMPTY) || (p[1].type == P_CAPTURE_END && p[2].type == P_EMPTY)))

/**
 * \brief           Compiles input pattern to library valid entries
 * \param[in]       p: Pointer to input pattern
 * \param[in]       len: Length of pattern
 * \return          1 if compiled, 0 otherwise
 */
static uint8_t
compile_pattern(regex_t* r, const char* p, size_t len) {
    size_t i = 0;
    p_t* patterns = r->p;
    while (len) {                               /* Process entire pattern char by char */
        if (i >= r->p_totlen) {                 /* End of available patterns? */
            return 0;                           /* Stop execution */
        }
        memset(&patterns[i], 0x00, sizeof(patterns[0]));
        switch (*p) {
            case '^': patterns[i].type = P_BEGIN; break;
            case '$': patterns[i].type = P_END; break;
            case '.': patterns[i].type = P_DOT; break;
            case '*':
                if (i > 1 && (patterns[i - 1].type == P_CAPTURE_START || patterns[i - 1].type == P_CAPTURE_END)) {
                    patterns[i - 2].min = 0;
                    patterns[i - 2].max = RANGE_MAX;
                    goto ignore;
                } else if (i > 0) {
                    patterns[i - 1].min = 0;
                    patterns[i - 1].max = RANGE_MAX;
                    goto ignore;
                }
                break;
            case '+':
                if (i > 1 && (patterns[i - 1].type == P_CAPTURE_START || patterns[i - 1].type == P_CAPTURE_END)) {
                    patterns[i - 2].min = 1;
                    patterns[i - 2].max = RANGE_MAX;
                    goto ignore;
                } else if (i > 0) {
                    patterns[i - 1].min = 1;
                    patterns[i - 1].max = RANGE_MAX;
                    goto ignore;
                }
                break;
            //case '?': patterns[i].type = P_QM; break;
            case '?':
                if (i > 1 && (patterns[i - 1].type == P_CAPTURE_START || patterns[i - 1].type == P_CAPTURE_END)) {
                    patterns[i - 2].min = 0;
                    patterns[i - 2].max = 1;
                    goto ignore;
                } else if (i > 0) {
                    patterns[i - 1].min = 0;
                    patterns[i - 1].max = 1;
                    goto ignore;
                }
                break;
            case '|': patterns[i].type = P_OR; break;
            case '(':
                //goto ignore;
                patterns[i].type = P_CAPTURE_START; /* Start of capturing group */
                break;
            case ')':
                //goto ignore;
                patterns[i].type = P_CAPTURE_END;   /* End of capturing group */
                break;
            case '\\': {                        /* Escape character */
                PTR_INC();                      /* Go to next character */
                switch (*p) {
                    case 'w':                   /* Match single character in list [\w] = [a-zA-Z0-9_] */
                    case 'W':                   /* Match single character not in a list [^\w] */
                    case 's':                   /* Match any whitespace [\r\n\t\v\f ] */
                    case 'S':                   /* Match any non-whitespace [^\r\n\t\v\f ] */
                    case 'd':                   /* Match any digit */
                    case 'D':                   /* Match any non-digit */
                        patterns[i].type = P_CHAR_CLASS;
                        patterns[i].str = p - 1;
                        patterns[i].len = 2;    /* We have 2 characters long pattern */
                        break;
                    default:
                        patterns[i].type = P_CHAR;  /* Treat it as normal character */
                        patterns[i].ch = *p;    /* Set character value */
                };
                break;
            }
            case '[': {
                PTR_INC();                      /* Go to next character */
                patterns[i].type = *p == '^' ? P_CHAR_CLASS_NOT : P_CHAR_CLASS; /* Set either normal or inverted character class */
                if (*p == '^') {                /* Go to next character and remember to negate */
                    PTR_INC();
                }
                patterns[i].str = p;            /* Set start address */
                patterns[i].len = 0;
                while (*p) {                    /* Set pointer and count length */
                    if (*p == ']' && *(p - 1) != '\\') {
                        break;
                    }
                    patterns[i].len++;
                    PTR_INC();
                }
                break;
            }
            case '{': {                         /* Length parameter, not necessary valid one? */
                /**
                 * Valid entries are:
                 *  1. {max} - Maximal number of repetitions
                 *  2. {min,max} - Minimal and maximal number of repetitions
                 *  3. {min,} - Minimal number of repetitions
                 */
                const char* tmp = p + 1;        /* Temporary var to check valid text first */
                uint8_t type = 0;
                uint32_t num1 = 0, num2 = 0;
                if (IS_DIGIT(*tmp)) {           /* At least one digit must be followed by { */
                    while (IS_DIGIT(*tmp)) {
                        num1 = num1 * 10 + CHAR_TO_NUM(*tmp++);
                    }
                    if (*tmp == ',') {          /* Check if comma exists */
                        tmp++;                  /* Go to next character */
                        type = 2;
                        if (IS_DIGIT(*tmp)) {   /* We also have set maximal value as second one */
                            type = 3;
                            while (IS_DIGIT(*tmp)) {
                                num2 = num2 * 10 + CHAR_TO_NUM(*tmp++);
                            }
                            if (num1 > num2) {  /* Check valid range */
                                type = 0;       /* Failed! */
                            }
                        }
                    } else {
                        type = 1;               /* Currently maximal number only is set */
                    }
                    if (*tmp++ != '}') {        /* We must finish with closed brackets */
                        type = 0;               /* Reset! */
                    }
                }
                /* Process forward to default */
                if (type && i) {
                    p_t* pattern;
                    if (i > 1 && (patterns[i - 1].type == P_CAPTURE_START || patterns[i - 1].type == P_CAPTURE_END)) {
                        pattern = &patterns[i - 2];
                    } else {
                        pattern = &patterns[i - 1];
                    }
                    while (p != tmp) {          /* Do it until they are not the same */
                        PTR_INC();              /* Increase input string */
                    }
                    switch (type) {             /* Check range type */
                        case 1: pattern->min = num1; pattern->max = num1; break;
                        case 2: pattern->min = num1; pattern->max = RANGE_MAX; break;
                        case 3: pattern->min = num1; pattern->max = num2; break;
                        default: return 0;
                    }
                    continue;
                }
            };
            default: {                          /* Non special character */
                if (len > 1 && !IS_SPECIAL_CHAR(p[1])) {    /* Is next one special character? */
                    patterns[i].type = P_CHAR_SEQUENCE;
                    patterns[i].str = p;        /* Set current pointer */
                    patterns[i].len = 1;
                    while ((len - 1) && *p) {   /* Process much as possible */
                        /* All special one except '[' */
                        if (p[1] == '\\') {    /* Is next one escape character? */
                            if (IS_SPECIAL_META_CHAR(p[2])) {   /* Is escape followed by meta character? */
                                break;
                            }
                        } else if (IS_SPECIAL_MOD_CHAR(p[2])) { /* In case second after current is special, stop with sequnce */
                            break;
                        } else if (p[0] != '\\' && IS_SPECIAL_CHAR(p[1])) { /* Check if next character is special one */
                            break;
                        }
                        patterns[i].len++;      /* Increase length of char sequence */
                        PTR_INC();              /* Go to next character */
                    }
                } else {
                    patterns[i].type = P_CHAR;
                    patterns[i].ch = *p;
                }
            }
        }
        i++;
ignore:
        PTR_INC();
    }
    patterns[i].type = P_EMPTY;                 /* Last pattern is always empty */
    r->p_len = i + 1;                           /* Set total length used */
    return 1;
}

/**
 * \brief           Checks if pattern starts and ends with correct characters such as /pattern/g
 * \param[in]       pattern: Pointer to pattern to test
 * \return          1 if ok, 0 otherwise
 */
static uint8_t
analyze_pattern(regex_t* r, const char** pat, size_t* length) {
    size_t len;
    int8_t brackets;                            /* Number of round, square and curly brackets */
    const char* pattern = *pat;
    len = strlen(pattern);                      /* Get pattern length */

    /**
     * Check if pattern is in format "/pattern/g"
     */
    if (pattern[0] != '/' || pattern[len - 2] != '/' || pattern[len - 1] != 'g') {
        return 0;
    }
    *pat = ++pattern;                           /* Set the pointer */
    *length = len - 3;                          /* Set length of pattern */

    /**
     * Check if there are same number of opening and closed brackets
     */
    for (brackets = 0; *pattern; pattern++) {
        switch (*pattern) {
            case '\\': {                        /* In case we have escape character */
                if (pattern[1] == '{' || pattern[1] == '(' || pattern[1] == '[' ||
                    pattern[1] == '}' || pattern[1] == ')' || pattern[1] == ']') {
                    pattern++;                  /* Simply skip next character */
                }
                break;
            }
            case '[':
            case '(':
            case '{':
                brackets++;
                break;
            case ']':
            case ')':
            case '}':
                brackets--;
                break;
        }
    }
    return !brackets;                           /* Brackets must be 0 */
}

/**
 * \brief           Try to match if character is in range between 2 values
 * \param[in]       str: Pointer to string (not-null-terminated) with range
 * \param[in]       len: Length of string available to check
 * \param[in]       c: Character to test against
 * \return          1 if match, 0 otherwise
 */
static uint8_t
match_class_range(regex_t* r, const char* str, size_t len, const char* in_str) {
    /**
     * To get a range match, we need:
     *  - Length of range string must be at least 3 characters
     *  - Middle one must be '-' to indicate range (ex. 0-9)
     *  - Input character must not be '-'
     */
    if (len >= 3 && str[1] == '-' && *in_str != '-') {  /* We need at least 3 characters here */
        return *in_str >= str[0] && *in_str <= str[2];
    }
    return 0;
}

/**
 * \brief           Match special characters for 'd', 'D', 'w', 'W', 's', 'S'
 * \param[in]       p: Pointer to current pattern
 * \param[in]       s_c: Special character to test for
 * \param[in]       c: Input character to test agains pattern
 * \return          1 if match, 0 otherwise
 */
static uint8_t
match_special_char(regex_t* r, const p_t* p, char s_c, const char* str) {
    uint8_t result = 0;
    switch (IS_C_UPPER(s_c) ? s_c : s_c - 0x20) {   /* Process only lowercase letters */
        case 'S': result = IS_S_CHAR(*str); break;  /* Check for whitespace */
        case 'W': result = IS_W_CHAR(*str); break;  /* Check for alphanumeric */
        case 'D': result = IS_D_CHAR(*str); break;  /* Check for digit */
        default: break;
    }
    if (IS_C_UPPER(s_c)) {                      /* Uppercase means NOT */
        result = !result;
    }
    return result;
}

/**
 * \brief           Match character class such as [0-9] or [0-9a-zA-Z] or similar
 * \param[in]       p: Pointer to exact pattern with character class included
 * \param[in]       c: Character to test in character class
 * \return          1 if match, 0 otherwise
 */
static uint8_t 
match_class_char(regex_t* r, const p_t* p, const char* str) {
    size_t i;
    for (i = 0; i < p->len; i++) {              /* Process input group string */
        /**
         * Try to match range of character between 2 values, such as [0-9]
         * In case p->str[i] == '-', check if left and right are range values
         */
        if (match_class_range(r, &p->str[i], p->len - i, str)) {
            return 1;
        }

        /**
         * Check if current char is backslash followed by special character
         */
        else if (p->str[i] == '\\') {
            i++;
            if (IS_SPECIAL_META_CHAR(p->str[i])) {  /* Check special characters */
                if (match_special_char(r, p, p->str[i], str)) {
                    return 1;
                }
            } else if (p->str[i] == *str) {     /* Maybe it was escaped direct character such as .+- */
                return 1;
            }
        }
        /**
         * In case character is the same as one of character group, we have a match:
         * - [abc] matches characters {'a', 'b', 'c'}
         * - If '-' character is included to match directly, it must be either first or last written:
         *    - [0-9-] or [-0-9] is valid and will match any number or '-' sign
         */
        else if (*str == p->str[i]) {
            if (*str == '-') {                  /* In case '-' is input character */
                return (p->str[0] == '-' || p->str[p->len - 1] == '-'); /* Check if it is first or last on char class */
            } else {                            /* Any other character */
                return 1;                       /* We have a match */
            }
        }
    }
    return 0;                                   /* Proceed with next pattern if possible */
}

/**
 * \brief           Match single character against given pattern type
 * \param[in]       p: Pointer to current pattern to match
 * \param[in]       c: Character to match
 * \return          1 if match, 0 otherwise
 */
static uint8_t
match_one_char(regex_t* r, const p_t* p, const char* str) {
    if (p->type == P_DOT) {                     /* Match any character */
        return 1;                               /* This one was successful */
    } else if (p->type == P_CHAR_CLASS) {       /* Match character class such [a-zA-Z0-9] */
        return match_class_char(r, p, str);
    } else if (p->type == P_CHAR_CLASS_NOT) {   /* Match character class such as [^a-zA-Z0-9] */
        return !match_class_char(r, p, str);
    } else {
        return p->ch == *str;
    }
    return 0;
}

/**
 * \brief           Matches char sequence
 * \param[in]       p: Pointer to current pattern holding char sequence
 * \param[in]       str: Input string to match sequence
 * \param[in]       con: 1 to continue to match next pattern, 0 to just return status of match
 * \return          1 if match, 0 otherwise
 */
static uint8_t
match_char_sequence(regex_t* r, const p_t* p, const char* str, uint8_t cont) {
    size_t i;
    const char* s = str;

    /**
     * go through entire char sequence string and process:
     *  - Check if end of matching string
     *  - Check if we have escape string, in this case move to next one before compare
     *  - Compare actual char by char and check if there is a match
     */
    for (i = 0; i < p->len; i++) {
        if (!*s) {                              /* If source string is empty */
            break;                              /* Stop execution immediatelly */
        }
        if (p->str[i] == '\\') {                /* Check for escape string */
            i++;                                /* Just move to next one */
        }
        if (*s != p->str[i]) {                  /* Compare actual string values */
            break;                              /* Finish as they failed */
        }
        s++;                                    /* Go to next source */
    }

    /**
     * If we have a match, proceed with next check
     */
    if (i == p->len) {                          /* All characters matched? */
        return cont ? match_pattern(r, p + 1, s, 1) : 1;/* Continue with matched source or just return positive result */
    }
    return 0;                                   /* No match here! */
}

/**
 * \brief           Match pattern range with {min,max} values, such as [0-9]{1,2} or similar
 * \note            It can match only single char or char sequence
 * \param[in]       p: Pointer to current pattern to test for
 * \param[in]       str: Pointer to string to test
 * \return          1 if match, 0 otherwise
 */
static uint8_t
match_pattern_range(regex_t* r, const p_t* p, const char* str) {
    int16_t cnt = 0;
    const char* s = str;
    
    /**
     * Process entire string and check for matches
     */
    while (cnt < p->max && *s) {                /* Process entire string or while we didn't reach maximum */
        if (p->type == P_CHAR_SEQUENCE) {       /* Check for char sequence */
            if (!match_char_sequence(r, p, s, 0)) { /* Try to match char sequence, but do not continue with other matches if there is a match */
                break;                          /* Stop execution when failed */
            }
            s += p->len;                        /* Increase character pointer for next entry by length of sequence */
        } else {                                /* Try to match single char only */
            if (!match_one_char(r, p, s)) {     /* Process with match */
                break;
            }
            s++;
        }

        cnt++;                                  /* Count number of matches */
        if (CAN_MATCH_MORE(p)) {                /* If last one is ont empty */
            if (match_pattern(r, p + 1, s, 0)) {/* Match next one? */
                if (cnt >= p->min) {
                    break;
                }
            }
        }
    }
    if (cnt >= p->min && cnt <= p->max) {       /* Now check how many entries we have */
        return CAN_MATCH_MORE(p) ? match_pattern(r, p + 1, s, 1) : 1;   /* We are in valid range */
    }
    return 0;                                   /* Invalid match */
}

/**
 * \brief           Match current pattern and check next one if required
 * \param[in]       p: Pointer to current pattern to match
 * \param[in]       str: Pointer to string to test pattern on
 * \param[in]       prev_result: Indicates previous result for OR operations
 * \return          1 if match, 0 otherwise
 */
static uint8_t
match_pattern(regex_t* r, const p_t* p, const char* str, uint8_t prev_result) {
    const char* s = str;
    uint8_t result = 0, ret = 0;
    do {
        if (p[0].type == P_OR) {                /* Is current pattern OR? */
            if (prev_result) {                  /* If result of previous operation was positive */
                prev_result = 0;                /* Reset result */
                while (p[0].type == P_OR) {     /* Ignore all others followed by OR */
                    p += 2;                     /* Ignore OR + next after OR */
                }
            } else {
                p++;                            /* Previous check was failed, try with next after OR */
            }
            continue;                           /* Ignore other execution and start over */
        }

        if (p->type == P_CAPTURE_START) {
            //printf("Capture start!\r\n");
            //r->matches[r->m_len].s = s;
            //r->matches[r->m_len].len = 0;
            p++;
            continue;
        } else if (p->type == P_CAPTURE_END) {
            //printf("Capture end!\r\n");
            //r->matches[r->m_len].len = s - r->matches[r->m_len].s;
            //r->m_len++;
            p++;
            continue;
        }

        /**
         * Check if there is no more patterns to match or
         * we have pattern with question mark meaning 0 or 1 match
         */
        if (p->type == P_EMPTY || p[1].type == P_QM) {
            result = 1;
            ret = 1;
        }
        /**
         * Check if we have to make sure about range of pattern
         * 
         * It matches STAR and PLUS patterns, set by pattern compilation
         */
        else if (p->min || p->max) {
            result = match_pattern_range(r, p, s);
            ret = 1;
        }
        /**
         * Match exact char sequence between pattern and source string
         */
        else if (p[0].type == P_CHAR_SEQUENCE) {
            result = match_char_sequence(r, p, s, 1);
            ret = 1;
        }
        /**
         * In case we have to end with specific match
         * check if we are really at the end by checking if next pattern is end
         * In this case simply check if source string is NULL
         */
        else if (p[0].type == P_END && p[1].type == P_EMPTY) {
            result = !*s;
            ret = 1;
        }

        /**
         * Handle OR entries after we have known result
         */
        prev_result = 0;
        if (ret) {                              /* Do we have to return status now? */
            if (result) {                       /* In case we have valid result */
                return result;                  /* Return it */
            } else if (p[1].type == P_OR) {     /* Check if next one is OR of current */
                p++;                            /* Go to next pattern to check */
                continue;                       /* Ignore other */
            }
            return result;                      /* Invalid result and no OR next = error */
        }
        if (*s && match_one_char(r, p, s)) {    /* Try to match single character */
            p++;                                /* Go to next pattern */
            s++;                                /* Go to next character */
            prev_result = 1;                    /* Set to valid result in case next one is OR */
        } else {                                /* We failed with next char */
            if (p[1].type == P_OR) {            /* In case next one is OR */
                p++;                            /* Go to OR pattern */
                continue;                       /* Ignore other execution */
            }
            break;                              /* Stop loop */
        }
    } while (1);                                /* Infinite loop */
    return 0;                                   /* No match at all found */
}

static void
print_pattern(regex_t* p) {
    size_t i = 0, len;

    for (; p->p[i].type != P_EMPTY; i++) {
        switch (p->p[i].type) {
            case P_CHAR_CLASS:
            case P_CHAR_CLASS_NOT:
                printf("Char class: \"");
                for (len = 0; len < p->p[i].len; len++) {
                    printf("%c", p->p[i].str[len]);
                }
                printf("\"; Min: %d, Max: %d\r\n", (int)p->p[i].min, (int)p->p[i].max);
                break;
            case P_CHAR_SEQUENCE:
                printf("Char sequence: \"");
                for (len = 0; len < p->p[i].len; len++) {
                    printf("%c", p->p[i].str[len]);
                }
                printf("\"; Min: %d, Max: %d\r\n", (int)p->p[i].min, (int)p->p[i].max);
                break;
            case P_CHAR:
                printf("Char: %c; Min: %d, Max: %d\r\n", p->p[i].ch, (int)p->p[i].min, (int)p->p[i].max);
                break;
            case P_OR:
                printf("OR\r\n");
                break;
            case P_CAPTURE_START:
                printf("CAPTURE_START\r\n");
                break;
            case P_CAPTURE_END:
                printf("CAPTURE_END\r\n");
                break;
            default:
                break;
        }
    }
}

/*
 * Public API functions
 */

/**
 * \brief           Prepare and compile pattern before it can be used for matching
 * \param[in]       r: Pointer to empty \ref regex_t structure for matching
 * \param[in]       pattern: Pointer to pattern string
 * \param[in]       p: Pointer to array to hold patterns data to
 * \param[in]       p_len: Size of array for patterns
 * \return          1 on success, 0 otherwise
 */
uint8_t
regex_prepare(regex_t* r, const char* pattern, regex_pattern_t* p, size_t p_len) {
    size_t len;

    r->p = p;                                   /* Save pointer to pattern array */
    r->p_totlen = p_len;                        /* Save total length of array available to use */
    r->p_len = 0;                               /* Reset number of currently used patterns */

    if (!analyze_pattern(r, &pattern, &len)) {  /* Analyze pattern and make sure it is in correct format */
        return 0;
    }

    if (!compile_pattern(r, pattern, len)) {    /* Try to compile pattern */
        return 0;
    }
    return 1;
}

/**
 * \brief           Check if string and pattern matches, public API function
 * \param[in]       pattern: Pattern to check in string
 * \param[in]       str: Pointer to input string to make match on
 * \return          1 on match, 0 otherwise
 */
uint8_t
regex_match(regex_t* r, const char* str, regex_match_t* matches, size_t m_len) {
    p_t* p;
    uint8_t anc;

    print_pattern(r);                           /* Print for debug purpose */
    
    p = r->p;                                   /* Set start pattern */
    anc = p->type == P_BEGIN ? (p++, 1) : 0;    /* Check if string must start with anchor */

    r->matches = matches;                       /* Set matching pointer */
    r->m_totlen = m_len;                        /* Set total length of available matching */
    r->m_len = 0;                               /* Reset number of used end matching arrays */

    do {
        if (match_pattern(r, p, str, 0)) {      /* Simply process entire string, even if it is NULL */
            return 1;                           /* Match was found */
        }
    } while (*str++ && !anc);                   /* Start from all the angles until string is valid */
    return 0;                                   /* Ooops, no match found! */
}