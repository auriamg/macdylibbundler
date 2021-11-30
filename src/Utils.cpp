/*
The MIT License (MIT)

Copyright (c) 2014 Marianne Gagnon

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
 */


#include "Utils.h"
#include "Dependency.h"
#include "Settings.h"
#include <cstdlib>
#include <unistd.h>
#include <iostream>
#include <cstdio>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
using namespace std;

/*
void setInstallPath(string loc)
{
    path_to_libs_folder = loc;
}*/

void tokenize(const string& str, const char* delim, vector<string>* vectorarg)
{
    vector<string>& tokens = *vectorarg;
    
    string delimiters(delim);
    
    // skip delimiters at beginning.
    string::size_type lastPos = str.find_first_not_of( delimiters , 0);
    
    // find first "non-delimiter".
    string::size_type pos = str.find_first_of(delimiters, lastPos);
    
    while (string::npos != pos || string::npos != lastPos)
    {
        // found a token, add it to the vector.
        tokens.push_back(str.substr(lastPos, pos - lastPos));
        
        // skip delimiters.  Note the "not_of"
        lastPos = str.find_first_not_of(delimiters, pos);
        
        // find next "non-delimiter"
        pos = str.find_first_of(delimiters, lastPos);
    }
    
}



bool fileExists(const std::string& filename)
{
    if (access( filename.c_str(), F_OK ) != -1)
    {
        return true; // file exists
    }
    else
    {
        //std::cout << "access(filename) returned -1 on filename [" << filename << "] I will try trimming." << std::endl;
        std::string delims = " \f\n\r\t\v";
        std::string rtrimmed = filename.substr(0, filename.find_last_not_of(delims) + 1);
        std::string ftrimmed = rtrimmed.substr(rtrimmed.find_first_not_of(delims));
        if (access( ftrimmed.c_str(), F_OK ) != -1)
        {
            return true;
        }
        else
        {
            //std::cout << "Still failed. Cannot find the specified file." << std::endl;
            return false;// file doesn't exist
        }
    }
}

void copyFile(const string& from, const string& to)
{
    bool override = Settings::canOverwriteFiles();
    if( from != to && !override )
    {
        if(fileExists( to ))
        {
            cerr << "\n\nError : File " << to.c_str() << " already exists. Remove it or enable overwriting." << endl;
            exit(1);
        }
    }

    string override_permission = string(override ? "-f " : "-n ");
        
    // copy file to local directory
    string command = string("cp ") + override_permission + string("\"") + from + string("\" \"") + to + string("\"");
    if( from != to && systemp( command ) != 0 )
    {
        cerr << "\n\nError : An error occured while trying to copy file " << from << " to " << to << endl;
        exit(1);
    }
    
    // give it write permission
    string command2 = string("chmod +w \"") + to + "\"";
    if( systemp( command2 ) != 0 )
    {
        cerr << "\n\nError : An error occured while trying to set write permissions on file " << to << endl;
        exit(1);
    }
}

std::string system_get_output(const std::string& cmd)
{
    FILE * command_output;
    char output[128];
    int amount_read = 1;
    
    std::string full_output;
    
    try
    {
        command_output = popen(cmd.c_str(), "r");
        if(command_output == NULL) throw;
        
        while(amount_read > 0)
        {
            amount_read = fread(output, 1, 127, command_output);
            if(amount_read <= 0) break;
            else
            {
                output[amount_read] = '\0';
                full_output += output;
            }
        }
    }
    catch(...)
    {
        std::cerr << "An error occured while executing command " << cmd.c_str() << std::endl;
        pclose(command_output);
        return "";
    }
    
    int return_value = pclose(command_output);
    if(return_value != 0) return "";
    
    return full_output;
}

int systemp(const std::string& cmd)
{
    std::cout << "    " << cmd.c_str() << std::endl;
    return system(cmd.c_str());
}

void changeInstallName(const std::string& binary_file, const std::string& old_name, const std::string& new_name)
{
    std::string command = std::string("install_name_tool -change \"") + old_name + "\" \"" + new_name + "\" \"" + binary_file + "\"";
    if( systemp( command ) != 0 )
    {
        std::cerr << "\n\nError: An error occured while trying to fix dependencies of " << binary_file << std::endl;
        exit(1);
    }
}

std::string getUserInputDirForFile(const std::string& filename)
{
    const int searchPathAmount = Settings::searchPathAmount();
    for(int n=0; n<searchPathAmount; n++)
    {
        auto searchPath = Settings::searchPath(n);
        if( !searchPath.empty() && searchPath[ searchPath.size()-1 ] != '/' ) searchPath += "/";

        if( fileExists( searchPath+filename ) )
        {
            std::cerr << (searchPath+filename) << " was found. /!\\ DYLIBBUNDLER MAY NOT CORRECTLY HANDLE THIS DEPENDENCY: Manually check the executable with 'otool -L'" << std::endl;
            return searchPath;
        }
    }

    while (true)
    {
        std::cout << "Please specify the directory where this library is located (or enter 'quit' to abort): ";  fflush(stdout);

        std::string prefix;
        std::cin >> prefix;
        std::cout << std::endl;

        if(prefix.compare("quit")==0) exit(1);

        if( !prefix.empty() && prefix[ prefix.size()-1 ] != '/' ) prefix += "/";

        if( !fileExists( prefix+filename ) )
        {
            std::cerr << (prefix+filename) << " does not exist. Try again" << std::endl;
            continue;
        }
        else
        {
            std::cerr << (prefix+filename) << " was found. /!\\ DYLIBBUNDLER MAY NOT CORRECTLY HANDLE THIS DEPENDENCY: Manually check the executable with 'otool -L'" << std::endl;
            Settings::addSearchPath(prefix);
            return prefix;
        }
    }
}

void adhocCodeSign(const std::string& file)
{
    // Add ad-hoc signature for ARM (Apple Silicon) binaries
    std::string signCommand = std::string("codesign --force --deep --preserve-metadata=entitlements,requirements,flags,runtime --sign - \"") + file + "\"";
    if( systemp( signCommand ) != 0 )
    {
        // If the codesigning fails, it may be a bug in Apple's codesign utility.
        // A known workaround is to copy the file to another inode, then move it back
        // erasing the previous file. Then sign again.
        std::cerr << "  * Error : An error occurred while applying ad-hoc signature to " << file << ". Attempting workaround" << std::endl;

        std::string machine = system_get_output("machine");
        bool isArm = machine.find("arm") != std::string::npos;
        std::string tempDirTemplate = std::string(std::getenv("TMPDIR") + std::string("dylibbundler.XXXXXXXX"));
        std::string filename = file.substr(file.rfind("/")+1);
        char* tmpDirCstr = mkdtemp((char *)(tempDirTemplate.c_str()));
        if( tmpDirCstr == NULL )
        {
            std::cerr << "  * Error : Unable to create temp directory for signing workaround" << std::endl;
            if( isArm )
            {
                exit(1);
            }
        }
        std::string tmpDir = std::string(tmpDirCstr);
        std::string tmpFile = tmpDir+"/"+filename;
        const auto runCommand = [isArm](const std::string& command, const std::string& errMsg)
        {
            if( systemp( command ) != 0 )
            {
                std::cerr << errMsg << std::endl;
                if( isArm )
                {
                    exit(1);
                }
            }
        };
        std::string command = std::string("cp -p \"") + file + "\" \"" + tmpFile + "\"";
        runCommand(command, "  * Error : An error occurred copying " + file + " to " + tmpDir);
        command = std::string("mv -f \"") + tmpFile + "\" \"" + file + "\"";
        runCommand(command, "  * Error : An error occurred moving " + tmpFile + " to " + file);
        command = std::string("rm -rf \"") + tmpDir + "\"";
        systemp( command );
        runCommand(signCommand, "  * Error : An error occurred while applying ad-hoc signature to " + file);
    }
}
