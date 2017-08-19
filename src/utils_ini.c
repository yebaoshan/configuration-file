/**
 * inih -- simple .INI file parser
 */

//#include "common.h"
#include "utils_ini.h"
//#include "utils_print.h"

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>

#ifndef INI_INLINE_COMMENT_PREFIXES
#define INI_INLINE_COMMENT_PREFIXES ";"
#endif

/* Maximum line length for any line in INI file. */
#ifndef INI_MAX_LINE
#define INI_MAX_LINE 512
#endif

#define sfree(ptr)                                                             \
  do {                                                                         \
    free(ptr);                                                                 \
    (ptr) = NULL;                                                              \
  } while (0)

int strarray_add(char ***ret_array, size_t *ret_array_len,
                 char const *str) /* {{{ */
{
  char **array;
  size_t array_len = *ret_array_len;

  if (str == NULL)
    return (EINVAL);

  array = (char**)realloc(*ret_array, (array_len + 1) * sizeof(*array));
  if (array == NULL)
    return (ENOMEM);
  *ret_array = array;

  array[array_len] = strdup(str);
  if (array[array_len] == NULL)
    return (ENOMEM);

  array_len++;
  *ret_array_len = array_len;
  return (0);
} /* }}} int strarray_add */

void strarray_free(char **array, size_t array_len) /* {{{ */
{
  for (size_t i = 0; i < array_len; i++)
    sfree(array[i]);
  sfree(array);
} /* }}} void strarray_free */

static char* sstrncpy(char *dest, const char *src, size_t n)
{
    strncpy(dest, src, n);
    dest[n - 1] = '\0';
    return dest;
}

/* Strip whitespace chars off end of given string, in place. Return s. */
static char* rstrip(char* s)
{
    char* p = s + strlen(s);
    while (p > s && isspace((unsigned char)(*--p)))
        *p = '\0';
    return s;
}

/* Return pointer to first non-whitespace char in given string. */
static char* lskip(const char* s)
{
    while (*s && isspace((unsigned char)(*s)))
        s++;
    return (char*)s;
}

/* Return pointer to first char (of chars) or inline comment in given string,
   or pointer to null at end of string if neither found. Inline comment must
   be prefixed by a whitespace character to register as a comment. */
static char* find_chars_or_comment(const char* s, const char* chars)
{
    int was_space = 0;
    while (*s && (!chars || !strchr(chars, *s)) &&
           !(was_space && strchr(INI_INLINE_COMMENT_PREFIXES, *s))) {
        was_space = isspace((unsigned char)(*s));
        s++;
    }

    return (char*)s;
}


static int handler(ini_section_t** section_head, const char* section,
                   const char* name, const char* value)
{
#define APPED_ITEM(head, item)  \
    do {                        \
        if (head == NULL)       \
        {                       \
            head = item;        \
            head->next = NULL;  \
        }                       \
        else                    \
        {                       \
            item->next = head;  \
            head = item;        \
        }                      \
    } while(0)

    //ini_section_t **section_head = (ini_section_t**)(user);
    if ((*section_head) == NULL || strcmp(section, (*section_head)->name) != 0)
    {
        ini_section_t *section_item = (ini_section_t*)malloc(sizeof(ini_section_t));
        sstrncpy(section_item->name, section, INI_MAX_SECTION);
        APPED_ITEM((*section_head), section_item);
    }

    if ((*section_head)->args == NULL
        || strcmp(name, (*section_head)->args->name) != 0)
    {
        ini_arg_t *arg_item = (ini_arg_t*)malloc(sizeof(ini_arg_t));
        sstrncpy(arg_item->name, name, INI_MAX_NAME);
        APPED_ITEM((*section_head)->args, arg_item);
    }

    if (0 != strarray_add(&(*section_head)->args->values,
                          &(*section_head)->args->values_number,
                          value))
    {
        printf("Failed to add value to args. section:%s, name:%s, value:%s\n",
               section, name, value);
        return -1;
    }

    return 0;
}

static ini_section_t* ini_parse_stream(void* stream)
{
    ini_section_t* section_head = NULL;

    char line[INI_MAX_LINE];
    char section[INI_MAX_SECTION] = "";
    char prev_name[INI_MAX_NAME] = "";

    char* start;
    char* end;
    char* name;
    char* value;
    int lineno = 0;
    int error = 0;

    /* Scan through stream line by line */
    while (fgets(line, INI_MAX_LINE, (FILE*)stream) != NULL) {
        lineno++;

        start = line;
        start = lskip(rstrip(start));

        if (*start == ';' || *start == '#') {
            /* Per Python configparser, allow both ; and # comments at the
               start of a line */
        }
        else if (*start == '[') {
            /* A "[section]" line */
            end = find_chars_or_comment(start + 1, "]");
            if (*end == ']') {
                *end = '\0';
                //sstrncpy(section, start + 1, sizeof(section));
                sstrncpy(section, lskip(rstrip(++start)) , sizeof(section));
                *prev_name = '\0';
            }
            else if (!error) {
                /* No ']' found on section line */
                error = lineno;
            }
        }
        else if (*prev_name && *start && start > line) {
            /* Non-blank line with leading whitespace, treat as continuation
               of previous name's value (as per Python configparser). */
            if (handler(&section_head, section, prev_name, start) != 0 && !error)
                error = lineno;
        }
        else if (*start) {
            /* Not a comment, must be a name[=:]value pair */
            end = find_chars_or_comment(start, "=:");
            if (*end == '=' || *end == ':') {
                *end = '\0';
                name = rstrip(start);
                value = end + 1;
                end = find_chars_or_comment(value, NULL);
                if (*end)
                    *end = '\0';
                value = lskip(value);
                rstrip(value);

                /* Valid name[=:]value pair found, call handler */
                sstrncpy(prev_name, name, sizeof(prev_name));
                if (handler(&section_head, section, name, value) != 0 && !error)
                    error = lineno;
            }
            else if (!error) {
                /* No '=' or ':' found on name[=:]value line */
                error = lineno;
            }
        }

        if (error) {
            printf("Failed to parse ini. line=%d\n", lineno);
            if (section_head != NULL) {
                free_section(section_head);
                section_head = NULL;
            }
            break;
        }
    }

    return section_head;
}

void free_section(ini_section_t *section)
{
    while (section != NULL)
    {
        ini_arg_t *args = section->args;
        while (args != NULL)
        {
            strarray_free(args->values, args->values_number);

            ini_arg_t *tmp = args->next;
            free(args);
            args = tmp;
        }

        ini_section_t *tmp = section->next;
        free(section);
        section = tmp;
    }
}

void print_section(ini_section_t *section)
{
    while (section != NULL)
    {
        printf("[%s]\n", section->name);
        ini_arg_t *args = section->args;
        while (args != NULL)
        {
            printf("    %s = ", args->name);
            for (int i = 0; i < args->values_number; i++)
            {
                printf("%s; ", *(args->values + i));
            }
            printf("\n");
            args = args->next;
        }
        section = section->next;
    }
}

ini_section_t* ini_parse(const char* filename)
{
    FILE* file;

    file = fopen(filename, "r");
    if (!file) {
        printf("Failed to open file:%s. errno:%d", filename, errno);
        return NULL;
    }

    ini_section_t* section_head = ini_parse_stream(file);
    fclose(file);

    return section_head;
}
