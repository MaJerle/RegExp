/**
 * \file            regex.h
 * \brief           Regular expression header file
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
#ifndef __REGEX_LIB_H
#define __REGEX_LIB_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "stdlib.h"
#include "string.h"
#include "stdint.h"
#include "stdio.h"

/**
 * \brief           List of possible regex pattern types
 */
typedef enum {
    P_UNKNOWN,                                  /*!< Type is not known */
    P_BEGIN,                                    /*!< ^ indicating string must start with match */
    P_QM,                                       /*!< Match 0 or 1 times */
    P_DOT,                                      /*!< Match any character */
    P_OR,                                       /*!< Branch (OR) operator */
    P_CHAR,                                     /*!< Match exact character */
    P_CHAR_SEQUENCE,                            /*!< Sequence of literal characters */
    P_CHAR_CLASS,                               /*!< Character class to match */
    P_CHAR_CLASS_NOT,                           /*!< Character class not to match */
    P_END,                                      /*!< Match end of string $ */
    P_EMPTY,                                    /*!< Indicate end of pattern */
    P_CAPTURE_START,                            /*!< Start of capturing group */
    P_CAPTURE_END,                              /*!< End of capturing group */
} regex_pattern_type_t;

/**
 * \brief           Information about single pattern after compilation
 */
typedef struct {
    union {
        const char* str;                        /*!< Pointer to string in source pattern */
        char ch;                                /*!< Character used for repetition */
    };
    uint8_t len;                                /*!< Length of string in source pattern, valid only if string is used */
    regex_pattern_type_t type;                  /*!< Pattern type */
    int16_t min, max;                           /*!< Minimal or maximal readings */
} regex_pattern_t;

/**
 * \brief           Structure holding single capturing group
 */
typedef struct {
    const char* s;                              /*!< Pointer to start of string */
    size_t len;                                 /*!< Length of string after match */
} regex_match_t;

/**
 * \brief           Main structure used between matches engine
 */
typedef struct {
    regex_pattern_t* p;                         /*!< Pointer to array of patterns */
    size_t p_len;                               /*!< Length of patterns used after compilation */
    size_t p_totlen;                            /*!< Total length of patterns array */

    regex_match_t* matches;                     /*!< Pointer to array of matches */
    size_t m_len;                               /*!< Number of matches used so far */
    size_t m_totlen;                            /*!< Total length of matches array */
} regex_t;

/**
 * \defgroup        RegExp Regular expression
 * \brief           Regular expression package
 * \{
 */
uint8_t     regex_prepare(regex_t* r, const char* pattern, regex_pattern_t* p, size_t p_len);
uint8_t     regex_match(regex_t* r, const char* str, regex_match_t* matches, size_t m_len);

/**
 * \}
 */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __REGEX_LIB_H */
