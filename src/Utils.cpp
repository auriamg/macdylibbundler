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
#include "Settings.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <cstdio>
#include <stdio.h>
#include <sys/param.h>
#ifndef __clang__
#include <sys/types.h>
#endif
#include <unistd.h>
using namespace std;

bool isRpath(const string& path)
{
    return path.find("@rpath") == 0 || path.find("@loader_path") == 0;
}

string filePrefix(const string& in)
{
    return in.substr(0, in.rfind("/")+1);
}

string stripPrefix(const string& in)
{
    return in.substr(in.rfind("/")+1);
}

string getFrameworkRoot(const string& in)
{
    return in.substr(0, in.find(".framework")+10);
}

string getFrameworkPath(const string& in)
{
    return in.substr(in.rfind(".framework/")+11);
}

string stripLSlash(const string& in)
{
    if (in[0] == '.' && in[1] == '/') return in.substr(2, in.size());
    return in;
}

string& rtrim(string& s)
{
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char c) { return !std::isspace(c); }).base(), s.end());
    return s;
}

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

bool fileExists( const std::string& filename )
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
    if(!override)
    {
        if(fileExists( to ))
        {
            cerr << "\n\nError : File " << to.c_str() << " already exists. Remove it or enable overwriting." << endl;
            exit(1);
        }
    }

    string override_permission = string(override ? "-f " : "-n ");
        
    // copy file to local directory
    string command = string("cp -R ") + override_permission + string("\"") + from + string("\" \"") + to + string("\"");
    if( from != to && systemp( command ) != 0 )
    {
        cerr << "\n\nError : An error occured while trying to copy file " << from << " to " << to << endl;
        exit(1);
    }
    
    // give it write permission
    string command2 = string("chmod -R +w \"") + to + "\"";
    if( systemp( command2 ) != 0 )
    {
        cerr << "\n\nError : An error occured while trying to set write permissions on file " << to << endl;
        exit(1);
    }
}

bool deleteFile(const string& path, bool overwrite)
{
    string overwrite_permission = string(overwrite ? "-f " : "");
    string command = string("rm -r ") + overwrite_permission + "\"" + path + "\"";
    if( systemp( command ) != 0 )
    {
        cerr << "\n\nError: An error occured while trying to delete " << path << endl;
        return false;
    }
    return true;
}

bool deleteFile(const string& path)
{
    bool overwrite = Settings::canOverwriteFiles();
    return deleteFile(path, overwrite);
}

vector<string> lsDir(const string& path)
{
    string cmd = string("ls \"") + path + "\"";
    string output = system_get_output(cmd);
    vector<string> files;
    tokenize(output, "\n", &files);
    return files;
}

bool mkdir(const string& path)
{
    string command = string("mkdir -p \"") + path + "\"";
    if( systemp( command ) != 0 )
    {
        cerr << "\n/!\\ ERROR: An error occured while creating " << path << endl;
        return false;
    }
    return true;
}

std::string system_get_output(std::string cmd)
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
    if( !Settings::quietOutput() ) std::cout << "    " << cmd.c_str() << "\n";
    return system(cmd.c_str());
}

string bundleExecutableName(const string& app_bundle_path)
{
    string cmd = string("/usr/libexec/PlistBuddy -c 'Print :CFBundleExecutable' \"") + app_bundle_path + "Contents/Info.plist\"";
    string output = system_get_output(cmd);
    return rtrim(output);
}

void changeId(const string& binary_file, const string& new_id)
{
    string command = string("install_name_tool -id \"") + new_id + "\" \"" + binary_file + "\"";
    if( systemp( command ) != 0 )
    {
        cerr << "\n\nError: An error occured while trying to change identity of library " << binary_file << endl;
        exit(1);
    }
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

std::string getUserInputDirForFile(const std::string& filename, const std::string& dependent_file)
{
    const int searchPathAmount = Settings::searchPathAmount();
    for(int n=0; n<searchPathAmount; n++)
    {
        auto searchPath = Settings::searchPath(n);
        if( !searchPath.empty() && searchPath[ searchPath.size()-1 ] != '/' ) searchPath += "/";

        if( fileExists( searchPath+filename ) )
        {

            if( !Settings::quietOutput() )
            {
                std::cerr << (searchPath+filename) << " was found. /!\\ DYLIBBUNDLER MAY NOT CORRECTLY HANDLE THIS DEPENDENCY: Manually check the executable with 'otool -L'" << std::endl;
            }
            return searchPath;
        }
    }

    while (true)
    {
        if( Settings::quietOutput() )
        {
            std::cerr << "\n/!\\ WARNING: Dependency " << filename << " of " << dependent_file << " not found\n";
        }
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

void otool(const string& flags, const string& file, vector<string>& lines)
{
    string command = string("/usr/bin/otool ") + flags + " \"" + file + "\"";
    string output = system_get_output(command);

    if( output.find("can't open file") != string::npos
        || output.find("No such file") != string::npos
        || output.find("at least one file must be specified") != string::npos
        || output.empty() )
    {
        cerr << "\n\n/!\\ ERROR: Cannot find file " << file << " to read its load commands\n";
        exit(1);
    }
    tokenize(output, "\n", &lines);
}

void parseLoadCommands(const string& file, const map<string, string>& cmds_values, map<string, vector<string>>& cmds_results)
{
    vector<string> raw_lines;
    otool("-l", file, raw_lines);

    for (const auto& cmd_value : cmds_values)
    {
        vector<string> lines;
        string cmd = cmd_value.first;
        string value = cmd_value.second;
        string cmd_line = string("cmd ") + cmd;
        string value_line = string(value) + " ";
        bool searching = false;
        for (const auto& raw_line : raw_lines)
        {
            if(raw_line.find(cmd_line) != string::npos)
            {
                if(searching)
                {
                    cerr << "\n\n/!\\ ERROR: Failed to find " << value << " before next cmd\n";
                    exit(1);
                }
                searching = true;
            }
            else if(searching)
            {
                size_t start_pos = raw_line.find(value_line);
                if(start_pos == string::npos) continue;
                size_t start = start_pos + value.size() + 1; // exclude data label "|value| "
                size_t end = string::npos;
                if(value == "name" || value == "path")
                {
                    size_t end_pos = raw_line.find(" (");
                    if(end_pos == string::npos) continue;
                    end = end_pos - start;
                }
                lines.push_back(raw_line.substr(start, end));
                searching = false;
            }
        }
        cmds_results[cmd] = lines;
    }
}

string searchFilenameInRpaths(const string& rpath_file, const string& dependent_file)
{
    string fullpath;
    string suffix = rpath_file.substr(rpath_file.rfind('/')+1);
    char fullpath_buffer[PATH_MAX];

    const auto check_path = [&](string path)
    {
        char buffer[PATH_MAX];
        string file_prefix = filePrefix(dependent_file);
        if(path.find("@executable_path") != string::npos || path.find("@loader_path") != string::npos)
        {
            if(path.find("@executable_path") != string::npos)
            {
                if(Settings::appBundleProvided())
                {
                    path = regex_replace(path, regex("@executable_path/"), Settings::executableFolder());
                }
            }
            if(dependent_file != rpath_file)
            {
                if(path.find("@loader_path") != string::npos)
                {
                    path = regex_replace(path, regex("@loader_path/"), file_prefix);
                }
            }
            if(realpath(path.c_str(), buffer))
            {
                fullpath = buffer;
                Settings::rpathToFullPath(rpath_file, fullpath);
                return true;
            }
        }
        else if(path.find("@rpath") != string::npos)
        {
            if(Settings::appBundleProvided())
            {
                string pathE = regex_replace(path, regex("@rpath/"), Settings::executableFolder());
                if(realpath(pathE.c_str(), buffer))
                {
                    fullpath = buffer;
                    Settings::rpathToFullPath(rpath_file, fullpath);
                    return true;
                }
            }
            if(dependent_file != rpath_file)
            {
                string pathL = regex_replace(path, regex("@rpath/"), file_prefix);
                if(realpath(pathL.c_str(), buffer))
                {
                    fullpath = buffer;
                    Settings::rpathToFullPath(rpath_file, fullpath);
                    return true;
                }
            }
        }
        return false;
    };

    // fullpath previously stored
    if(Settings::rpathFound(rpath_file))
    {
        fullpath = Settings::getFullPath(rpath_file);
    }
    else if(!check_path(rpath_file))
    {
        auto rpaths_for_file = Settings::getRpathsForFile(dependent_file);
        for (auto rpath : rpaths_for_file)
        {
            if(rpath[rpath.size()-1] != '/') rpath += "/";
            string path = rpath + suffix;
            if (check_path(path)) break;
        }
    }

    if(fullpath.empty())
    {
        vector<string> search_paths = Settings::searchPaths();
        for (const auto& search_path : search_paths)
        {
            if(fileExists(search_path+suffix))
            {
                fullpath = search_path + suffix;
                break;
            }
        }
        if(fullpath.empty())
        {
            if(!Settings::quietOutput()) cerr << "\n/!\\ WARNING: Can't get path for '" << rpath_file << "'\n";
            fullpath = getUserInputDirForFile(suffix, dependent_file) + suffix;
            if(Settings::quietOutput() && fullpath.empty()) cerr << "\n/!\\ WARNING: Can't get path for '" << rpath_file << "'\n";
            if(realpath(fullpath.c_str(), fullpath_buffer)) fullpath = fullpath_buffer;
        }
    }

    return fullpath;
}

string searchFilenameInRpaths(const string& rpath_file)
{
    return searchFilenameInRpaths(rpath_file, rpath_file);
}

void createQtConf(string directory)
{
    string contents = "[Paths]\n"
                      "Plugins = PlugIns\n"
                      "Imports = Resources/qml\n"
                      "Qml2Imports = Resources/qml\n";
    if( directory[ directory.size()-1 ] != '/' ) directory += "/";
    ofstream out(directory + "qt.conf");
    out << contents;
    out.close();
}