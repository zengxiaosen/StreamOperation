//
// Created by yiqixie on 7/4/17.
//
#include <iostream>
#include <string>
#include <fstream>
using namespace std;
int main(){
    string filePath = "D:\\arwen.txt";
    fstream writeFile;
    writeFile.open(filePath,IOS::out);
    writeFile << "hello arwen" << endl;
    fstream.close();

    string filePath1 = "E:\\arwen.txt";
    fstream readFile;
    readFile.open(filePath, ios::in);
    char ch;
    while(readFile.get(ch))
        cout << ch;
    readFile.close();


}
