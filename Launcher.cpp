#include "PseudoNTFS.hpp"

const int32_t DISK_SIZE = 10000;
const int32_t CLUSTER_SIZE = 100; 

int main(int argc, char * argv[]) {

    PseudoNTFS pntfs = PseudoNTFS(DISK_SIZE, CLUSTER_SIZE);

    pntfs.saveFileToPseudoNtfs("temp_file", "test_file.txt");
    pntfs.printDisk();
    
}

// void executeCommand(string command) {
  
//   istringstream iss(command);
//   string token;
  
//   getline(iss, token, DELIMETER);
  
//   if (token == "EXIT" || token == "exit") {
//       // TODO BETTER
//     delete currentPath; 
//     exit(0);
//   }
//   else if (token == "pwd") {
//     currentPath->printPath();
//   }
//   else if (token == "cd") {
//       // DEFINETLY TODO ONLY TEST ...
//     getline(iss, token, DELIMETER);

//     char * name = new char[token.length() + 1];
//     strcpy(name, token.c_str());
//     currentPath->goInto(name);
//   } 
  
// }
 
// int main() {
  
//   currentPath = new Path();
  
//   string command;
//   for(;;) {
    
    
//     cout << ">> ";
//     getline(cin, command);
    
//     cout << ">> ";
//     executeCommand(command);
//     cout << endl;
    
//   }
  
// } 