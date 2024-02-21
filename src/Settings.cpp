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

#include "Settings.h"
#include "Utils.h"

#include <vector>
#include <iostream>
#include <fstream>

namespace Settings
{

bool overwrite_files = false;
bool overwrite_dir = false;
bool create_dir = false;
bool codesign = true;
std::string codesign_identity = "-";

bool canOverwriteFiles(){ return overwrite_files; }
bool canOverwriteDir(){ return overwrite_dir; }
bool canCreateDir(){ return create_dir; }
bool canCodesign(){ return codesign; }

void canOverwriteFiles(bool permission){ overwrite_files = permission; }
void canOverwriteDir(bool permission){ overwrite_dir = permission; }
void canCreateDir(bool permission){ create_dir = permission; }
void canCodesign(bool permission){ codesign = permission; }

std::string signingIdentity(){ return codesign_identity; }
void signingIdentity(const std::string& str) { codesign_identity = str; }

bool bundleLibs_bool = false;
bool bundleLibs(){ return bundleLibs_bool; }
void bundleLibs(bool on){ bundleLibs_bool = on; }


std::string dest_folder_str = "./libs/";
std::string destFolder(){ return dest_folder_str; }
void destFolder(const std::string& path)
{
    dest_folder_str = path;
    // fix path if needed so it ends with '/'
    if( dest_folder_str[ dest_folder_str.size()-1 ] != '/' ) dest_folder_str += "/";
}

std::vector<std::string> files;
void addFileToFix(const std::string& path){ files.push_back(path); }
int fileToFixAmount(){ return files.size(); }
std::string fileToFix(const int n){ return files[n]; }

namespace {

bool isMachOFile(const std::string& path) {
    auto file = std::ifstream{path, std::ios::binary};
    if (!file) {
        return false;
    }

    // Read the first 4 bytes
    char buffer[4];
    if (!file.read(buffer, 4)) {
        return false;
    }

    // Check if the magic number of the file looks like Mach-O file...
    if (std::memcmp(buffer, "\xcf\xfa\xed\xfe", 4) == 0 || std::memcmp(buffer, "\xce\xfa\xed\xfe", 4) == 0) {
        // Now make sure by calling the `file` command...
        static const auto prefix = std::string{"Mach-O"};
        auto type = system_get_output("file -b \"" + path + "\"");
        return type.compare(0, prefix.length(), prefix) == 0;
    }

    return false;
}

}

void addFolderToFix(const std::string& path)
{
    // we use find as a silly way to list all files inside the folder without
    // doing the recursion in C++, which without C++17 std::filesystem is a bit
    // cumbersome...
    auto all_files = std::vector<std::string>{};
    tokenize(system_get_output("find \"" + path + "\""), "\n", &all_files);

    auto prefix = std::string{"Mach-O"};
    for (auto&& file : all_files) {
        if (isMachOFile(file)) {
            std::cout << "  * Found file " << file << std::endl;
            files.push_back(std::move(file));
        }
    }

}

std::string inside_path_str = "@executable_path/../libs/";
std::string inside_lib_path(){ return inside_path_str; }
void inside_lib_path(const std::string& p)
{
    inside_path_str = p;
    // fix path if needed so it ends with '/'
    if( inside_path_str[ inside_path_str.size()-1 ] != '/' ) inside_path_str += "/";
}

std::vector<std::string> prefixes_to_ignore;
void ignore_prefix(std::string prefix)
{
    if( prefix[ prefix.size()-1 ] != '/' ) prefix += "/";
    prefixes_to_ignore.push_back(prefix);
}

bool isSystemLibrary(const std::string& prefix)
{
    if(prefix.find("/usr/lib/") == 0) return true;
    if(prefix.find("/System/Library/") == 0) return true;

    return false;
}

bool isPrefixIgnored(const std::string& prefix)
{
    const int prefix_amount = prefixes_to_ignore.size();
    for(int n=0; n<prefix_amount; n++)
    {
        if(prefix.compare(prefixes_to_ignore[n]) == 0) return true;
    }

    return false;
}

bool isPrefixBundled(const std::string& prefix)
{
    if(prefix.find(".framework") != std::string::npos) return false;
    if(prefix.find("@executable_path") != std::string::npos) return false;
    if(isSystemLibrary(prefix)) return false;
    if(isPrefixIgnored(prefix)) return false;

    return true;
}

std::vector<std::string> searchPaths;
void addSearchPath(const std::string& path){ searchPaths.push_back(path); }
int searchPathAmount(){ return searchPaths.size(); }
std::string searchPath(const int n){ return searchPaths[n]; }

}
