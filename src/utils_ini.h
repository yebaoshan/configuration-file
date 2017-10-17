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

typedef struct ini_arg_data_s
{
    char name[INI_MAX_NAME];
    char **values;
    size_t values_number;
} ini_arg_data_t;

struct ini_arg_s;
typedef struct ini_arg_s ini_arg_t;
struct ini_arg_s
{
    ini_arg_data_t data;
    ini_arg_t *next;
};

typedef struct ini_section_data_s
{
    char name[INI_MAX_SECTION];
    ini_arg_t *args;
} ini_section_data_t;

struct ini_section_s;
typedef struct ini_section_s ini_section_t;
struct ini_section_s
{
    ini_section_data_t data;
    ini_section_t *next;
};

void free_arg_data(ini_arg_data_t *arg);
void free_section_data(ini_section_data_t *section_data);
void free_section(ini_section_t *section);
void print_arg_data(const ini_arg_data_t *arg_data);
void print_section_data(const ini_section_data_t *section_data);
void print_section(const ini_section_t *section);

ini_section_t* ini_parse(const char *filename);
ini_section_data_t* get_section(const char *filename, const char *section_name);
ini_arg_data_t* get_arg(const char *filename,
                         const char *section_name,
                         const char *arg_name);

int add_arg(const char *filename, const char *section_name, ini_arg_data_t *data);




#endif /* INI_H */
