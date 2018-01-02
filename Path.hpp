#ifndef _PATH_HPP_
#define _PATH_HPP_    

#include <iostream>
#include <cstring>

#include "Utils.hpp"


    const int8_t NAME_LENGTH = 12;
    const int8_t NOT_FILLED = -1;

    const char DELIMETER = ' ';
    const char PATH_SEPARATOR = '/';
    
    const char PATH_DELIM[] = "/";
    const char BACK[] = "..";
    
    class PseudoNTFS;

    // Basic node of path
    struct pathNode {
        
        // Name of file/folder
        char name[NAME_LENGTH] = {'\0'};
        // Index to mft table
        int32_t mftItemIndex = NOT_FILLED; 
        // Next path node
        pathNode * next = NULL;
        // Previous path node
        pathNode * previous = NULL;

        pathNode & operator=(const pathNode & node) {
            strcpy(name, node.name);
            mftItemIndex = node.mftItemIndex;
            next = NULL;
            previous = NULL;
        }
    };

    class Path {
      
      private:
        struct pathNode * const pathHead;
        struct pathNode * pathTail;
        PseudoNTFS * ntfs;

        void clear();

      public:
        
        Path(PseudoNTFS * ntfs);
        Path(const Path & path);
        ~Path();
        
        Path & operator=(const Path & path);

        bool change(char * path, bool isDirectory);
        bool goInto(const char * name, const bool isDirectory);
        bool goBack();
        int32_t getCurrentMftIndex();
        void printPath();

        static void getNameFromPath(const char * path, char * fileName);
        static void getNameFromPath(const char * path, char * fileName, std::string * parentDirectoryPath);    
    };


#endif