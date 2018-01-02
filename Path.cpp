#include <cstring>
#include <sstream>

#include "Path.hpp"
#include "PseudoNTFS.hpp"

Path::Path(PseudoNTFS * ntfs) : pathHead(new struct pathNode()) {

    this->ntfs = ntfs;

    strcpy(pathHead->name, "");
    pathHead->mftItemIndex = 0;
    pathHead->next = NULL;
    pathHead->previous = NULL;
    pathTail = pathHead;
}

Path::Path(const Path & path) : pathHead(new struct pathNode()) {

    *pathHead = *path.pathHead;
    pathTail = pathHead;

    struct pathNode * orig = path.pathHead->next;
    struct pathNode * copy = pathHead;
    struct pathNode * temp;

    while (orig != NULL) {
        temp = new struct pathNode();
        *temp = *orig;
        temp->previous = copy;
        copy->next = temp;
        orig = orig->next;
        copy = copy->next;
        pathTail = temp;
    }

    ntfs = path.ntfs;
}

Path & Path::operator=(const Path & path) {

    if (this == &path) {
        return *this;
    }
    else {
        clear();

        *pathHead = *path.pathHead;
        pathTail = pathHead;

        struct pathNode * orig = path.pathHead->next;
        struct pathNode * copy = pathHead;
        struct pathNode * temp;

        while (orig != NULL) {
            temp = new struct pathNode();
            *temp = *orig;
            temp->previous = copy;
            copy->next = temp;
            orig = orig->next;
            copy = copy->next;
            pathTail = temp;
        }

        ntfs = path.ntfs;
    }
}

Path::~Path() {

    struct pathNode * temp = pathHead; 

    while (temp->next != NULL) {
        temp = temp->next;
        delete temp->previous;
    }
    
    delete temp;
    temp = NULL;
}

void Path::clear() {

    struct pathNode * temp = pathHead->next; 

    if (temp == NULL) {
        return;
    }

    while (temp->next != NULL) {
        temp = temp->next;
        delete temp->previous;
    }
    
    delete temp;
    temp = NULL;

    pathTail = pathHead;

}

bool Path::change(char * path, bool isDirectory) {

    if (strcmp(path, "") == 0) {
        return true;
    }

    if (path[0] == PATH_SEPARATOR) {
        clear();
    }
    
        
    char * token;
    char * nextToken;

    token = strtok(path, PATH_DELIM);

    for(;;) {
            
        nextToken = strtok(NULL, PATH_DELIM);

        if (strcmp(token, BACK) == 0) {
            if (!goBack()) {
                return false;
            }
        }
        else {
            if (nextToken == NULL) {
                if (!goInto(token, isDirectory)) {
                    return false;
                }
            }
            else {
                if (!goInto(token, true)) {
                    return false;
                }
            }
        }

        if (nextToken == NULL) {
            break;
        }
        else {
            strcpy(token, nextToken);
        }
    }

    return true;
} 

bool Path::goInto(const char * name, const bool isDirectory) { 
    
    int32_t mftItemIndex = ntfs->contains(pathTail->mftItemIndex, name, isDirectory);
    if (mftItemIndex == NOT_FOUND) {
        return false;
    }
    else {
        struct pathNode * temp = new struct pathNode();
        temp->mftItemIndex = mftItemIndex;
        strcpy(temp->name, name);
        temp->previous = pathTail;
        temp->next = NULL;
        pathTail->next = temp;
        pathTail = temp;
        return true;
    }
}

bool Path::goBack() {
    
    if (pathTail->previous == NULL) {
        //TODO CANNOT GO BACKWARD
        return false;
    }
    else {
        pathTail = pathTail->previous;
        delete pathTail->next;
        pathTail->next = NULL;
        return true;
    } 
}

void Path::printPath() {
    
    using namespace std;
    
    struct pathNode * temp = pathHead;
    
    cout << PATH_SEPARATOR;
    while (temp->next != NULL) {
        temp = temp->next;
        cout << temp->name << PATH_SEPARATOR;
    }
    
}

int32_t Path::getCurrentMftIndex() {
    return pathTail->mftItemIndex;
} 

void Path::getNameFromPath(const char * path, char * fileName) {

    char * temp = new char[strlen(path)];
    strcpy(temp, path);

    char * token;

    token = strtok(temp, PATH_DELIM);
    while (token != NULL) {
        strcpy(fileName, token);
        token = strtok(NULL, PATH_DELIM);
    }
}

void Path::getNameFromPath(const char * path, char * fileName, std::string * parentDirectoryPath) {

    char * temp = new char[strlen(path)];
    strcpy(temp, path);

    std::ostringstream oss;

    char * token;
    char * nextToken;

    token = strtok(temp, PATH_DELIM);
    while (token != NULL) {
        nextToken = strtok(NULL, PATH_DELIM);

        if (nextToken == NULL) {
            strcpy(fileName, token);
            break;
        }
        else {
            oss << token << PATH_SEPARATOR;
            strcpy(token, nextToken);
        }
    }
    
    *parentDirectoryPath = oss.str();

}