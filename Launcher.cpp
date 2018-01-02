#include <sstream>
#include <fstream>

#include "PseudoNTFS.hpp"
#include "Path.hpp"

const int32_t DISK_SIZE = 10000;
const int32_t CLUSTER_SIZE = 100; 

Path * currentPath;
PseudoNTFS * pntfs;

using namespace std;

void executeCommand(string command);
void executeCd(string * param);
void executeIncp(string * fparam, string * sParam);
void executeCat(string * param);
void executeInfo(string * param);
void executeLs(string * param);
void executeMkdir(string * param);
void executeRmdir(string * param);  

int main(int argc, char * argv[]) {

    pntfs = new PseudoNTFS(DISK_SIZE, CLUSTER_SIZE);

    currentPath = new Path(pntfs);
  
    string command;
    for(;;) {

        cout << ">> ";
        getline(cin, command);
    
        if (command == "EXIT" || command == "exit") {
            break;
        }
        else if (command == "PRINT-DISK" || command == "print-disk") {
            pntfs->printDisk();
        }

        cout << ">> ";
        executeCommand(command);
        cout << endl;
    
    }


    //pntfs.saveFileToPseudoNtfs("temp_file", "test_file.txt");
   
}

void executeCommand(string command) {
  
  istringstream iss(command);
  string token, fParam, sParam;
  
  getline(iss, token, DELIMETER);
  
  if (token == "pwd") {
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
        pntfs->saveFileToPseudoNtfs(fileName, fParam->c_str(), tempPath.getCurrentMftIndex());
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
        pntfs->loadFileFromPseudoNtfs(tempPath.getCurrentMftIndex(), &content);
        cout << content;
    }
    else {
        cout << "PATH NOT FOUND";
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
        mftItem = pntfs->getMftItem(fTempPath.getCurrentMftIndex());
        pntfs->printMftItemInfo(mftItem);
    }
    else if (sTempPath.change(path, true)) {
        mftItem = pntfs->getMftItem(sTempPath.getCurrentMftIndex());
        pntfs->printMftItemInfo(mftItem);
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
        pntfs->makeDirectory(tempPath.getCurrentMftIndex(), name);
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
        pntfs->removeDirectory(mftItemIndex, parentDirectoryMftItemIndex);
    }
    else {
        cout << "PATH NOT FOUND";
    }

    delete [] path;

}