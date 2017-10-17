#include "utils_ini.h"
#include <stdlib.h>
#include <string.h>

int main()
{
    const char *filename = "test.ini";

    ini_section_t *section = ini_parse(filename);
    if (section == NULL) {
        printf("Can't load '%s'", filename);
        return -1;
    }

    print_section(section);
    free_section(section);


    printf("test get_section1\n");
    ini_section_data_t* section_data = get_section(filename, "FileInput");
    if (section_data != NULL) {
        print_section_data(section_data);
        free_section_data(section_data);
    }

    printf("test get_section2\n");
    section_data = get_section(filename, "FileInput11");
    if (section_data != NULL) {
        print_section_data(section_data);
        free_section_data(section_data);
    }

    printf("test get_arg1\n");
    ini_arg_data_t* arg_data = get_arg(filename, "Global1", "WriteThreads");
    if (arg_data != NULL) {
        print_arg_data(arg_data);
        free_arg_data(arg_data);
    }

    printf("test get_arg2\n");
    arg_data = get_arg(filename, "Global", "WriteThreads11");
    if (arg_data != NULL) {
        print_arg_data(arg_data);
        free_arg_data(arg_data);
    }

    printf("test add_args\n");
    arg_data = malloc(sizeof(ini_arg_data_t));
    memset(arg_data, 0, sizeof(ini_arg_data_t));
    strcpy(arg_data->name, "Module");
    arg_data->values_number = 2;
    char **values = (char**)malloc(arg_data->values_number * sizeof(char*));
    values[0] = strdup("cpu");
    values[1] = strdup("memory");
    values[1] = strdup("process");
    arg_data->values = values;
    print_arg_data(arg_data);
    int ret = add_arg(filename, "System", arg_data);
    printf("add_args, ret=%d\n", ret);
    free_arg_data(arg_data);

    return 0;
}
// vim: set ts=4 sw=4 sts=4 et:
