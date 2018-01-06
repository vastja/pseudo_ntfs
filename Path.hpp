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

        /* change path to file/directory
         * CHANGE current path if given path exists
         * +param - path - change path to this path
         * +param - isDirectory - true - path points to directory, else false
         * +retun - true - file exists, else false
        */
        bool change(char * path, bool isDirectory);
        /* go from current path into file/directory
         * CHANGE current path if file/directory exist
         * +param - name - file/directory name
         * +param - isDirectory - true - directory, else false
         * +return - file/directory exists
        */ 
        bool goInto(const char * name, const bool isDirectory);
        /* go back in current path one step
        */
        bool goBack();
        /* get mft index of current file/directory
         * +return - mft index of current path of file/directory
        */
        int32_t getCurrentMftIndex();

        /* print path */
        void printPath();

        /* get name of file/folder from path
         * +param - path - path
         * +param - fileName - searched file/directory name  
        */
        static void getNameFromPath(const char * path, char * fileName);
         /* get name of file/folder from path and parent directory path
         * +param - path - path
         * +param - fileName - searched file/directory name 
         * +param - parentDirectoryPath - path fo file/directory parent directory
        */
        static void getNameFromPath(const char * path, char * fileName, std::string * parentDirectoryPath);    
    };


#endif