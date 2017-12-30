#include "Utils.hpp"

#include <fstream>
#include <sstream>

/*
Load content of file to string - with EOF 
*/
void readFile(const char * filePath, std::string * str) {

    std::ifstream file(filePath);

    std::ostringstream oss;
    char c;
    while (c = file.get()) {
        oss << c;

        if (c == EOF) {
            break;
        }
    }
    file.close();

    *str = oss.str();
}