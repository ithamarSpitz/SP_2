#include <iostream>
#include <cstdlib>
#include <string>
#include <vector>

using namespace std;

int main(int argc, char* argv[]) {
    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " -e \"<command>\"" << endl;
        return 1;
    }
    string input;
    if (1){// -o
        
    }
    string command = argv[2];
    string fullCommand = "bash -c \"" + command + "\"";
    int result = system(fullCommand.c_str());
    return 0;
}
