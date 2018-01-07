#include "Utils.hpp"

#include <fstream>
#include <sstream>

/*
Load content of file to string - with EOF 
*/
bool readFile(const char * filePath, std::string * str) {

    std::ifstream file(filePath);

    if (!file) {
        return false;
    }

    std::ostringstream oss;
    char c;
    while ((c = file.get()) != EOF {
        oss << c;
    }
    file.close();

    *str = oss.str();

    return true;
}