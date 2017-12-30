#ifndef _PATH_HPP_
#define _PATH_HPP_    

#include <iostream>
#include <cstring>

    const int8_t NAME_LENGTH = 12;
    const int8_t NOT_FILLED = -1;

    const char DELIMETER = ' ';
    const char PATH_SEPARATOR = '/';
    
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
      public:
        
        Path();
        Path(const Path & path);
        ~Path();
        
        void goInto(const char * name);
        void goBack();
        void printPath();
        
    };


#endif