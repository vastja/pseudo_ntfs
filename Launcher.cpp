#include <sstream>
#include <fstream>

#include "PseudoNTFS.hpp"
#include "Path.hpp"

const int32_t DISK_SIZE = 100000;
const int32_t CLUSTER_SIZE = 100; 

Path * currentPath;
PseudoNTFS * pntfs;

using namespace std;

/* functions over ntfs */
void executeCommand(string command);
void executeCd(string * param);
void executeIncp(string * fparam, string * sParam);
void executeCat(string * param);
void executeInfo(string * param);
void executeLs(string * param);
void executeMkdir(string * param);
void executeRmdir(string * param);
void executeOutcp(string * fparam, string * sParam);  
void executeRm(string * param);
void executeMv(string * fParam, string * sParam);
void executeCp(string * fParam, string * sParam) ;

int main(int argc, char * argv[]) {

    if (argc != 2) {
        cout << "USAGE: PseudoNTFS.out <signature>";
        exit(0);
    }

    pntfs = new PseudoNTFS(DISK_SIZE, CLUSTER_SIZE, argv[1]);

    currentPath = new Path(pntfs);
  
    string command;
    for(;;) {

        cout << ">> ";
        getline(cin, command);
    
        if (command == "EXIT" || command == "exit") {
            break;
        }    

        cout << ">> ";
        executeCommand(command);
        cout << endl;
    
    }  
}

void executeCommand(string command) {
  
    istringstream iss(command);
    string token, fParam, sParam;
  
    getline(iss, token, DELIMETER);
  
    if (command == "PDISK" || command == "pdisk") {
            pntfs->printDisk();
    }
    else if (command == "chdisk") {
        if (pntfs->checkDiskConsistency()) {
            cout << ">> DISK IS OK";
        }
        else {
            cout << "DISK IS CORRUPTED";
        }
    }
    else if (command == "ddisk") {
        pntfs->defragmentDisk();
    }
    else if (token ==  "load") {
        getline(iss, fParam, DELIMETER);

        ifstream ifs(fParam);

        if (ifs) {
            while (getline(ifs, command)) {
                executeCommand(command);
            }
            ifs.close();
        }
        else {
            cout << "FILE NOT FOUND";
        }
    }
    else if (token == "pwd") {
        currentPath->printPath();
    }
    else if (token == "cd") {
        getline(iss, fParam, DELIMETER);
        executeCd(&fParam);
    }
    else if (token == "incp") {
        getline(iss, fParam, DELIMETER);
        getline(iss, sParam, DELIMETER);
        executeIncp(&fParam, &sParam);
    }
    else if (token == "cat") {
        getline(iss, fParam, DELIMETER);
        executeCat(&fParam);
    }
    else if (token == "info") {
        getline(iss, fParam, DELIMETER);
        executeInfo(&fParam);
    }
    else if (token == "ls") {
        getline(iss, fParam, DELIMETER);
        executeLs(&fParam);
    }
    else if (token == "mkdir") {
        getline(iss, fParam, DELIMETER);
        executeMkdir(&fParam); 
    }
    else if (token == "rmdir") {
        getline(iss, fParam, DELIMETER);
        executeRmdir(&fParam); 
    }
    else if (token == "outcp") {
        getline(iss, fParam, DELIMETER);
        getline(iss, sParam, DELIMETER);
        executeOutcp(&fParam, &sParam);
    }
    else if (token == "rm") {
        getline(iss, fParam, DELIMETER);
        executeRm(&fParam); 
    }
    else if (token == "mv") {
        getline(iss, fParam, DELIMETER);
        getline(iss, sParam, DELIMETER);
        executeMv(&fParam, &sParam);
    }
    else if (token == "cp") {
        getline(iss, fParam, DELIMETER);
        getline(iss, sParam, DELIMETER);
        executeCp(&fParam, &sParam);
    }
  
}

void executeCd(string * param) {

    char * path = new char[param->length() + 1];
    strcpy(path, param->c_str());
    
    Path tempPath = *currentPath;
    if (tempPath.change(path, true)) {
        *currentPath = tempPath;
        cout << "OK";
    }
    else {
        cout << "PATH NOT FOUND";
    }

    delete [] path;
}
 
void executeIncp(string * fParam, string * sParam) {

    char * path = new char[sParam->length() + 1];
    strcpy(path, sParam->c_str());
    
    Path tempPath = *currentPath;
    if (tempPath.change(path, true)) {
        char fileName[12];
        Path::getNameFromPath(fParam->c_str(), fileName);
        if (pntfs->saveFileToPseudoNtfs(fileName, fParam->c_str(), tempPath.getCurrentMftIndex())) {
            cout << "OK";
        }
    }
    else {
        cout << "PATH NOT FOUND";
    }

    delete [] path;
}

void executeCat(string * param) {
    
    char * path = new char[param->length() + 1];
    strcpy(path, param->c_str());
    
    Path tempPath = *currentPath;
    if (tempPath.change(path, false)) {
        string content;
        if (pntfs->loadFileFromPseudoNtfs(tempPath.getCurrentMftIndex(), &content)) {
            cout << content;
        }
    }
    else {
        cout << "FILE NOT FOUND";
    }

    delete [] path;
}

void executeInfo(string * param) {
    
    char * path = new char[param->length() + 1];
    strcpy(path, param->c_str());
    
    struct mft_item * mftItem;

    Path fTempPath = *currentPath;
    Path sTempPath = *currentPath;
    if (fTempPath.change(path, false)) {
        pntfs->printMftItem(fTempPath.getCurrentMftIndex());
    }
    else if (sTempPath.change(path, true)) {
        pntfs->printMftItem(sTempPath.getCurrentMftIndex());
    }
    else {
        cout << "PATH NOT FOUND";
    }

    delete [] path;
}

void executeLs(string * param) {

    char * path = new char[param->length() + 1];
    strcpy(path, param->c_str());
    
    Path tempPath = *currentPath;
    if (tempPath.change(path, false)) {
        
        list<mft_item> content;
        pntfs->getDirectoryContent(tempPath.getCurrentMftIndex(), &content);

        for (mft_item mftItem : content) {

            if (mftItem.isDirectory) {
                cout << "+" << mftItem.item_name << " ";
            }
            else {
                cout << "-" << mftItem.item_name << " ";
            }
        }

    }
    else {
        cout << "PATH NOT FOUND";
    }

    delete [] path;

}

void executeMkdir(string * param) {

    char * path = new char[param->length() + 1];
    strcpy(path, param->c_str());
    
    Path tempPath = *currentPath;

    string parentDirectoryPath;
    char name[12];
    Path::getNameFromPath(path, name, &parentDirectoryPath);

    char * parentPath = new char[parentDirectoryPath.length() + 1];
    strcpy(parentPath, parentDirectoryPath.c_str());

    if (tempPath.change(parentPath, true)) {
        if (pntfs->makeDirectory(tempPath.getCurrentMftIndex(), name)) {
            cout << "OK";
        }
    }
    else {
        cout << "PATH NOT FOUND";
    }

    delete [] path;

}

void executeRmdir(string * param) {

    char * path = new char[param->length() + 1];
    strcpy(path, param->c_str());
    
    Path tempPath = *currentPath;
    if (tempPath.change(path, true)) {
        int32_t mftItemIndex = tempPath.getCurrentMftIndex();
        tempPath.goBack();
        int32_t parentDirectoryMftItemIndex = tempPath.getCurrentMftIndex();
        if (pntfs->removeDirectory(mftItemIndex, parentDirectoryMftItemIndex)) {
            cout << "OK";
        }
    }
    else {
        cout << "PATH NOT FOUND";
    }

    delete [] path;

}

void executeOutcp(string * fParam, string * sParam) {
    
    char * path = new char[fParam->length() + 1];
    strcpy(path, fParam->c_str());
    
    Path tempPath = *currentPath;
    if (tempPath.change(path, false)) {
        string content;
        if (pntfs->loadFileFromPseudoNtfs(tempPath.getCurrentMftIndex(), &content)) {
            ofstream file(*sParam);
            if (file) {
                file << content;
                file.close();
                cout << "OK"; 
            }
            else {
            cout << "PATH NOT FOUND"; 
            }
        }
    }
    else {
        cout << "FILE NOT FOUND";
    }

    delete [] path;
}

void executeRm(string * param) {

    char * path = new char[param->length() + 1];
    strcpy(path, param->c_str());
    
    int32_t mftItemIndex;
    Path tempPath = *currentPath;
    if (tempPath.change(path, false)) {
        mftItemIndex = tempPath.getCurrentMftIndex();
        tempPath.goBack();
        if (pntfs->removeFile(mftItemIndex, tempPath.getCurrentMftIndex())) {
            cout << "OK";
        }
    }
    else {
        cout << "FILE NOT FOUND";
    }

    delete [] path;

}

void executeMv(string * fParam, string * sParam) {
    char * fPath = new char[fParam->length() + 1];
    strcpy(fPath, fParam->c_str());
    
    char * sPath = new char[sParam->length() + 1];
    strcpy(sPath, sParam->c_str());
    
    int32_t fileMftIndex, directoryMftIndex;
    Path tempPath = *currentPath;
    if (tempPath.change(fPath, false)) {
        fileMftIndex = tempPath.getCurrentMftIndex();
        Path tempPath = *currentPath;
        if (tempPath.change(sPath, true)) {
            directoryMftIndex = tempPath.getCurrentMftIndex();
            tempPath.goBack();
            if (pntfs->move(fileMftIndex, tempPath.getCurrentMftIndex(), directoryMftIndex)) {
                cout << "OK";
            }
        }
        else {
            cout << "PATH NOT FOUND";
        }
    }
    else {
        cout << "FILE NOT FOUND";
    }

    delete [] fPath;
    delete [] sPath;
}

void executeCp(string * fParam, string * sParam) {
    
    char * fPath = new char[fParam->length() + 1];
    strcpy(fPath, fParam->c_str());
    
    char * sPath = new char[sParam->length() + 1];
    strcpy(sPath, sParam->c_str());
    
    int32_t fileMftItemIndex, toMftitemIndex;
    Path tempPath = *currentPath;
    if (tempPath.change(fPath, false)) {
        fileMftItemIndex = tempPath.getCurrentMftIndex();
        Path tempPath = *currentPath;
        if (tempPath.change(sPath, true)) {
            if (pntfs->copy(fileMftItemIndex, tempPath.getCurrentMftIndex())) {
                cout << "OK";
            }
        }
        else {
           cout << "PATH NOT FOUND"; 
        }
    }
    else {
        cout << "FILE NOT FOUND";
    }

    delete [] fPath;
    delete [] sPath;
}