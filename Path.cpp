#include "Path.hpp"

Path::Path() : pathHead(new struct pathNode()) {
    pathTail = pathHead;
}

Path::Path(const Path & path) : pathHead(new struct pathNode()) {

    *pathHead = *path.pathHead;

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
    
void Path::goInto(const char * name) { 
    //TODO TEST IF IT EXISTS, FOLDER
    struct pathNode * temp = new struct pathNode();
    strcpy(temp->name, name);
    temp->previous = pathTail;
    temp->next = NULL;
    pathTail->next = temp;
    pathTail = temp;
}

void Path::goBack() {
    
    if (pathTail->previous == NULL) {
        //TODO CANNOT GO BACKWARD
        return;
    }
    
    pathTail = pathTail->previous;
    delete pathTail->next;
    pathTail->next = NULL;
    
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