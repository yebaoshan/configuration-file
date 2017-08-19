/**
 * inih -- simple .INI file parser
 *
 */

#ifndef INI_H
#define INI_H

#include <stdio.h>

/* Maximum length of section name. */
#ifndef INI_MAX_SECTION
#define INI_MAX_SECTION 50
#endif

/* Maximum length of args name. */
#ifndef INI_MAX_NAME
#define INI_MAX_NAME 50
#endif

struct ini_arg_s;
typedef struct ini_arg_s ini_arg_t;
struct ini_arg_s
{
    char name[INI_MAX_NAME];
    char **values;
    size_t values_number;
    ini_arg_t *next;
};

struct ini_section_s;
typedef struct ini_section_s ini_section_t;
struct ini_section_s
{
    char name[INI_MAX_SECTION];
    ini_arg_t *args;
    ini_section_t *next;
};

ini_section_t* ini_parse(const char* filename);
void print_section(ini_section_t *section);
void free_section(ini_section_t *section);

#endif /* INI_H */
