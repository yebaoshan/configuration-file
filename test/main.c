#include "utils_ini.h"

int main()
{
    const char* filename = "test.ini";
    ini_section_t *section = ini_parse(filename);
    if (section == NULL) {
        printf("Can't load '%s'", filename);
        return -1;
    }


    print_section(section);
    free_section(section);

    return 0;
}
// vim: set ts=4 sw=4 sts=4 et:
