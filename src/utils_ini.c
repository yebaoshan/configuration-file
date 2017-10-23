/**
 * inih -- simple .INI file parser
 */

#include "utils_ini.h"

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>

#include <stdarg.h>
#include <unistd.h>
#include <sys/types.h>

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

#define APPED_ITEM(head, item)  \
    do {                        \
        item->next = head;  \
        head = item;        \
    } while(0)

#define LOG_DBG 0
#define LOG_ERR 1
#define DEBUG(...) print_log(LOG_DBG, __VA_ARGS__)
#define ERROR(...) print_log(LOG_ERR, __VA_ARGS__)

typedef int (*HANDLER)(void *user, const char *section,
                       const char *name, const char *value, long pos);

typedef struct get_section_user_s
{
    ini_section_data_t *section_data;
    char name[INI_MAX_SECTION];
} get_section_user_t;

typedef struct get_arg_user_s
{
    ini_arg_data_t *arg_data;
    char section_name[INI_MAX_SECTION];
    char arg_name[INI_MAX_NAME];
} get_arg_user_t;

typedef struct add_args_user_s
{
    long section_pos;
    long arg_bpos;
    long arg_epos;
    char arg_name[INI_MAX_NAME];
    char section_name[INI_MAX_SECTION];
} add_arg_user_t;

void print_log(int level, const char *format, ...)
{
  char msg[1024];
  va_list ap;

  va_start(ap, format);
  vsnprintf(msg, sizeof(msg), format, ap);
  msg[sizeof(msg) - 1] = '\0';
  va_end(ap);

  printf("%s\n", msg);
} /* void plugin_log */

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

static int ini_parse_handler(void *user, const char *section,
                             const char *name, const char *value,
                             long __attribute__((unused)) pos )
{
    DEBUG("section:%s; name:%s; value:%s", section, name, value);

    ini_section_t **section_head = (ini_section_t**)(user);
    if ((*section_head) == NULL || strcmp(section, (*section_head)->data.name) != 0)
    {
        ini_section_t *section_item = (ini_section_t*)calloc(1, sizeof(ini_section_t));
        sstrncpy(section_item->data.name, section, INI_MAX_SECTION);
        APPED_ITEM((*section_head), section_item);
    }

    if (name == NULL)
        return 0;

    ini_arg_t **ini_arg = &(*section_head)->data.args;
    if (*ini_arg == NULL
        || strcmp(name, (*ini_arg)->data.name) != 0)
    {
        ini_arg_t *item = (ini_arg_t*)calloc(1, sizeof(ini_arg_t));
        sstrncpy(item->data.name, name, INI_MAX_NAME);
        APPED_ITEM((*ini_arg), item);
    }

    if (0 != strarray_add(&(*ini_arg)->data.values,
                          &(*ini_arg)->data.values_number,
                          value))
    {
        ERROR("Failed to add value to args. section:%s, name:%s, value:%s\n",
              section, name, value);
        return -1;
    }

    return 0;
}

static int get_section_handler(void *user, const char *section,
                               const char *name, const char *value,
                               long __attribute__((unused)) pos)
{
    DEBUG("section:%s; name:%s; value:%s", section, name, value);

    get_section_user_t *section_user = (get_section_user_t*)(user);
    if (strcmp(section, section_user->name) != 0) {
        if (section_user->section_data != NULL)
            return 1;
        else
            return 0;
    }

    if (section_user->section_data == NULL) {
        section_user->section_data = (ini_section_data_t*)calloc(1, sizeof(ini_section_data_t));
        sstrncpy(section_user->section_data->name, section, INI_MAX_SECTION);
    }

    if (name == NULL)
        return 0;

    ini_arg_t **ini_arg = &section_user->section_data->args;
    if (*ini_arg == NULL || strcmp(name, (*ini_arg)->data.name) != 0)
    {
        ini_arg_t *arg_item = (ini_arg_t*)calloc(1, sizeof(ini_arg_t));
        sstrncpy(arg_item->data.name, name, INI_MAX_NAME);
        APPED_ITEM((*ini_arg), arg_item);
    }

    if (0 != strarray_add(&(*ini_arg)->data.values,
                          &(*ini_arg)->data.values_number,
                          value))
    {
        ERROR("Failed to add value to args. section:%s, name:%s, value:%s\n",
              section, name, value);
        return -1;
    }

    return 0;
}

static int get_arg_handler(void *user, const char *section,
                           const char *name, const char *value,
                           long __attribute__((unused)) pos)
{
    DEBUG("section:%s; name:%s; value:%s", section, name, value);

    if (name == NULL)
        return 0;

    get_arg_user_t *arg_user = (get_arg_user_t*)(user);
    if (strcmp(section, arg_user->section_name) != 0
        || strcmp(name, arg_user->arg_name) != 0) {
        if (arg_user->arg_data != NULL)
            return 1;
        else
            return 0;
    }

    if (arg_user->arg_data == NULL) {
        arg_user->arg_data = (ini_arg_data_t*)calloc(1, sizeof(ini_arg_data_t));
        sstrncpy(arg_user->arg_data->name, name, sizeof(arg_user->arg_data->name));
    }

    if (0 != strarray_add(&arg_user->arg_data->values,
                          &arg_user->arg_data->values_number,
                          value))
    {
        ERROR("Failed to add value to args. section:%s, name:%s, value:%s\n",
              section, name, value);
        return -1;
    }

    return 0;
}

static int add_arg_handler(void *user, const char *section,
                           const char *name, const char *value, long pos)
{
    DEBUG("section:%s; name:%s; value:%s", section, name, value);

    add_arg_user_t *arg_user = (add_arg_user_t*)(user);
    if (strcmp(section, arg_user->section_name) != 0)
    {
        if (arg_user->section_pos == -1)
            return 0;

        // Found next section, to end
        if (arg_user->arg_bpos != -1) {
            arg_user->arg_epos = pos;
        } else {
            arg_user->arg_bpos = pos;
            arg_user->arg_epos = pos;
        }
        return 1;
    }

    if (arg_user->section_pos == -1)
        arg_user->section_pos = pos;

    if (name == NULL)
        return 0;

    if (strcmp(name, arg_user->arg_name) != 0)
    {
        if (arg_user->arg_bpos != -1) {  // Found next arg, to end
            arg_user->arg_epos = pos;
            return 1;
        } else {
            return 0;
        }
    }

    if (arg_user->arg_bpos == -1)
        arg_user->arg_bpos = pos;

    return 0;
}

static int parse_stream(void *stream, HANDLER handler, void *user)
{
    char line[INI_MAX_LINE];
    char section[INI_MAX_SECTION] = "";
    char prev_name[INI_MAX_NAME] = "";

    char *start;
    char *end;
    char *name;
    char *value;
    int lineno = 0;
    int ret = 0;
    long pos = ftell(stream);

    /* Scan through stream line by line */
    while (fgets(line, INI_MAX_LINE, stream) != NULL) {
        lineno++;

        start = line;
        start = lskip(rstrip(start));

        if (*start == ';' || *start == '#') {
            /* Per Python configparser, allow both ; and # comments at the
               start of a line */
        } else if (*prev_name && *start && start > line) {
            /* Non-blank line with leading whitespace, treat as continuation
               of previous name's value (as per Python configparser). */
            if ((ret = handler(user, section, prev_name, start, pos)) != 0)
                break;
        }
        else if (*start == '[') {
            /* A "[section]" line */
            end = find_chars_or_comment(start + 1, "]");
            if (*end != ']') // No ']' found on section line
                break;

            *end = '\0';
            sstrncpy(section, start + 1, sizeof(section));
            *prev_name = '\0';
            if ((ret = handler(user, section, NULL, NULL, pos)) != 0)
                break;
        } else if (*start) {
            /* Not a comment, must be a name[=:]value pair */
            end = find_chars_or_comment(start, "=:");
            if (*end != '=' && *end != ':') // No '=' or ':' found on name[=:]value line
                break;

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
            if ((ret = handler(user, section, name, value, pos)) != 0)
                break;
        }

        pos = ftell(stream);
    }

    if (ret < 0) {
        ERROR("Failed to parse ini. line=%d\n", lineno);
    }

    return ret;
}

void free_arg_data(ini_arg_data_t *arg)
{
    if (arg == NULL)
        return;

    strarray_free(arg->values, arg->values_number);
    free(arg);
}

void free_section_data(ini_section_data_t *section_data)
{
    if (section_data == NULL)
        return;

    ini_arg_t *arg = section_data->args;
    while (arg != NULL)
    {
        strarray_free(arg->data.values, arg->data.values_number);

        ini_arg_t *tmp = arg->next;
        free(arg);
        arg = tmp;
    }
}

void free_section(ini_section_t *section)
{
    while (section != NULL)
    {
        free_section_data(&section->data);

        ini_section_t *tmp = section->next;
        free(section);
        section = tmp;
    }
}

void print_arg_data(const ini_arg_data_t *arg_data)
{
    if (arg_data == NULL)
        return;

    printf("    %s = ", arg_data->name);
    for (int i = 0; i < arg_data->values_number; i++)
    {
        printf("%s; ", *(arg_data->values + i));
    }

    printf("\n");
}

void print_section_data(const ini_section_data_t *section_data)
{
    if (section_data == NULL)
        return;

    printf("[%s]\n", section_data->name);
    ini_arg_t *arg = section_data->args;
    while (arg != NULL)
    {
        print_arg_data(&arg->data);
        arg = arg->next;
    }
}

void print_section(const ini_section_t *section)
{
    while (section != NULL)
    {
        print_section_data(&section->data);
        section = section->next;
    }
}

ini_section_t* ini_parse(const char* filename)
{
    FILE* file;
    file = fopen(filename, "r");
    if (!file) {
        ERROR("Failed to open file:%s. errno:%d", filename, errno);
        return NULL;
    }

    ini_section_t* section_head = NULL;
    if (parse_stream(file, ini_parse_handler, &section_head) != 0
        && section_head != NULL) {
        free_section(section_head);
        section_head = NULL;
    }

    fclose(file);

    return section_head;
}

ini_section_data_t* get_section(const char *filename, const char *section_name)
{
    FILE *file;
    file = fopen(filename, "r");
    if (!file) {
        ERROR("Failed to open file:%s. errno:%d", filename, errno);
        return NULL;
    }

    get_section_user_t section_user;
    memset(&section_user, 0, sizeof(get_section_user_t));
    sstrncpy(section_user.name, section_name, sizeof(section_user.name));
    if (parse_stream(file, get_section_handler, &section_user) < 0
        && section_user.section_data != NULL) {
            free_section_data(section_user.section_data);
            section_user.section_data = NULL;
    }

    fclose(file);

    return section_user.section_data;
}

ini_arg_data_t* get_arg(const char *filename, const char *section_name,
                        const char *arg_name)
{
    FILE *file;
    file = fopen(filename, "r");
    if (!file) {
        ERROR("Failed to open file:%s. errno:%d", filename, errno);
        return NULL;
    }

    get_arg_user_t arg_user;
    memset(&arg_user, 0, sizeof(get_arg_user_t));
    sstrncpy(arg_user.section_name, section_name, sizeof(arg_user.section_name));
    sstrncpy(arg_user.arg_name, arg_name, sizeof(arg_user.arg_name));
    if (parse_stream(file, get_arg_handler, &arg_user) < 0
        && arg_user.arg_data != NULL) {
            free_arg_data(arg_user.arg_data);
            arg_user.arg_data = NULL;
    }

    fclose(file);

    return arg_user.arg_data;
}

static int write_arg(FILE *file, add_arg_user_t *user,
                     ini_arg_data_t *arg_data)
{
    int fd = fileno(file);
    if (fd == -1) {
        ERROR("Failed to get fd, errno:%d", errno);
        return -1;
    }

    char line[INI_MAX_LINE] = {0};
    if (user->section_pos == -1) {
        fseek(file, 0, SEEK_END);
        //fprintf(file, "\n[%s]\n", user->section_name);
        snprintf(line, sizeof(line), "\n[%s]\n", user->section_name);
        fwrite(line, sizeof(char), strlen(line), file);

        user->arg_bpos = user->arg_epos = ftell(file);
    } else if (user->arg_bpos == -1 && user->arg_epos == -1) {
        user->arg_bpos = user->arg_epos = ftell(file);
    } else if (user->arg_bpos != -1 && user->arg_epos == -1) {
        user->arg_epos = ftell(file);
    }

    fseek(file, 0, SEEK_END);
    char *buf = NULL;
    long len = ftell(file) - user->arg_epos;
    if (len) {
        buf = (char*)malloc((size_t)len);
        if (!buf) {
            ERROR("Failed to malloc buf, len(%ld)", len);
            return -1;
        }

        fseek(file, user->arg_epos, SEEK_SET);
        if (len != fread(buf, sizeof(char), (size_t)len, file)) {
            ERROR("Failed to read last content, len:%ld", len);
            return -1;
        }
    }

    char name[INI_MAX_NAME];
    char **values;
    size_t values_number;
    fseek(file, user->arg_bpos, SEEK_SET);
    //fprintf(file, "%s = ", arg_data->name);
    memset(line, 0 , sizeof(line));
    snprintf(line, sizeof(line), "%s = ", arg_data->name);
    fwrite(line, sizeof(char), strlen(line), file);

    for (int i = 0; i < arg_data->values_number; ++i) {
        //fprintf(file, "    %s\n", *(arg_data->values + i));
        memset(line, 0 , sizeof(line));
        snprintf(line, sizeof(line), "    %s\n", *(arg_data->values + i));
        fwrite(line, sizeof(char), strlen(line), file);
    }

    if (len != 0) {
        fwrite(buf, sizeof(char), (size_t)len, file);
        //fprintf(file, "%s", buf);
        fflush(file);
        fsync(fd);
    }

    if (buf)
        free(buf);

    if (-1 == ftruncate(fd, ftell(file))) {
        ERROR("Failed to ftruncate.");
        return -1;
    }

    return 0;
}

int add_arg(const char *filename, const char *section_name, ini_arg_data_t *arg_data)
{
    FILE *file;
    file = fopen(filename, "rb+");
    if (!file) {
        if (access(filename, F_OK) != 0)
            file = fopen(filename, "wb+");

        if (!file) {
            ERROR("Failed to open file:%s. errno:%d", filename, errno);
            return -1;
        }
    }

    add_arg_user_t user;
    user.section_pos = -1;
    user.arg_bpos = -1;
    user.arg_epos = -1;
    sstrncpy(user.section_name, section_name, sizeof(user.section_name));
    sstrncpy(user.arg_name, arg_data->name, sizeof(user.arg_name));
    if (parse_stream(file, add_arg_handler, &user) < 0) {
        fclose(file);
        ERROR("Failed to parse stream to get position, %s.", filename);
        return -1;
    }

    if (write_arg(file, &user, arg_data) != 0) {
        ERROR("Failed to write arg. file:%s", filename);
        fclose(file);
        return -1;
    }

    fclose(file);
    return 0;
}
