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

namespace Settings
{

bool overwrite_files = false;
bool overwrite_dir = false;
bool create_dir = false;
bool bundle_libs = false;
bool bundle_frameworks = false;
bool quiet_output = false;

std::string dest_folder_str = "./libs/";
std::string dest_folder_str_app = "./Frameworks/";
std::string dest_folder = dest_folder_str;
std::string dest_path = dest_folder;

std::string inside_path_str = "@executable_path/../libs/";
std::string inside_path_str_app = "@executable_path/../Frameworks/";
std::string inside_path = inside_path_str;

bool canOverwriteFiles(){ return overwrite_files; }
bool canOverwriteDir(){ return overwrite_dir; }
bool canCreateDir(){ return create_dir; }

void canOverwriteFiles(bool permission){ overwrite_files = permission; }
void canOverwriteDir(bool permission){ overwrite_dir = permission; }
void canCreateDir(bool permission){ create_dir = permission; }

bool bundleLibs(){ return bundle_libs; }
void bundleLibs(bool on){ bundle_libs = on; }

bool bundleFrameworks(){ return bundle_frameworks; }
void bundleFrameworks(bool on){ bundle_frameworks = on; }

bool quietOutput(){ return quiet_output; }
void quietOutput(bool status){ quiet_output = status; }

std::string app_bundle;
bool appBundleProvided(){ return !app_bundle.empty(); }
std::string appBundle(){ return app_bundle; }
void appBundle(std::string path)
{
    app_bundle = path;
    char buffer[PATH_MAX];
    if(realpath(app_bundle.c_str(), buffer))
        app_bundle = buffer;
    // fix path if needed so it ends with '/'
    if( app_bundle[ app_bundle.size()-1 ] != '/' )
        app_bundle += "/";

    std::string bundle_executable_path = app_bundle + "Contents/MacOS/" + bundleExecutableName(app_bundle);
    if(realpath(bundle_executable_path.c_str(), buffer))
        bundle_executable_path = buffer;
    addFileToFix(bundle_executable_path);

    if(inside_path == inside_path_str)
        inside_path = inside_path_str_app;
    if(dest_folder == dest_folder_str)
        dest_folder = dest_folder_str_app;

    dest_path = app_bundle + "Contents/" + stripLSlash(dest_folder);
    if(realpath(dest_path.c_str(), buffer))
        dest_path = buffer;
    if( dest_path[ dest_path.size()-1 ] != '/' )
        dest_path += "/";
}

std::string destFolder(){ return dest_path; }
void destFolder(std::string path)
{
    dest_path = path;
    if(appBundleProvided()) dest_path = app_bundle + "Contents/" + stripLSlash(path);
    char buffer[PATH_MAX];
    if(realpath(dest_path.c_str(), buffer)) dest_path = buffer;
    if( dest_path[ dest_path.size()-1 ] != '/' ) dest_path += "/";
}

std::string executableFolder() { return app_bundle + "Contents/MacOS/"; }
std::string frameworksFolder() { return app_bundle + "Contents/Frameworks/"; }
std::string pluginsFolder() { return app_bundle + "Contents/PlugIns/"; }
std::string resourcesFolder() { return app_bundle + "Contents/Resources/"; }

std::vector<std::string> files;
void addFileToFix(std::string path)
{
    char buffer[PATH_MAX];
    if(realpath(path.c_str(), buffer)) path = buffer;
    files.push_back(path);
}
int fileToFixAmount(){ return files.size(); }
std::string fileToFix(const int n){ return files[n]; }

std::string inside_lib_path(){ return inside_path; }
void inside_lib_path(std::string p)
{
    inside_path = p;
    // fix path if needed so it ends with '/'
    if( inside_path[ inside_path.size()-1 ] != '/' ) inside_path += "/";
}

std::vector<std::string> prefixes_to_ignore;
void ignore_prefix(std::string prefix)
{
    if( prefix[ prefix.size()-1 ] != '/' ) prefix += "/";
    prefixes_to_ignore.push_back(prefix);
}

bool isSystemLibrary(std::string prefix)
{
    if(prefix.find("/usr/lib/") == 0) return true;
    if(prefix.find("/System/Library/") == 0) return true;

    return false;
}

bool isPrefixIgnored(std::string prefix)
{
    const int prefix_amount = prefixes_to_ignore.size();
    for(int n=0; n<prefix_amount; n++)
    {
        if(prefix.compare(prefixes_to_ignore[n]) == 0) return true;
    }

    return false;
}

bool isPrefixBundled(std::string prefix)
{
    if(!bundle_frameworks && prefix.find(".framework") != std::string::npos) return false;
    if(prefix.find("@executable_path") != std::string::npos) return false;
    if(isSystemLibrary(prefix)) return false;
    if(isPrefixIgnored(prefix)) return false;
    
    return true;
}

std::vector<std::string> searchPaths;
void addSearchPath(std::string path){ searchPaths.push_back(path); }
int searchPathAmount(){ return searchPaths.size(); }
std::string searchPath(const int n){ return searchPaths[n]; }

std::vector<std::string> userSearchPaths;
void addUserSearchPath(std::string path){ userSearchPaths.push_back(path); }
size_t userSearchPathAmount(){ return userSearchPaths.size(); }
std::string userSearchPath(const int n){ return userSearchPaths[n]; }

// if some libs are missing prefixes, this will be set to true
// more stuff will then be necessary to do
bool missing_prefixes = false;
bool missingPrefixes(){ return missing_prefixes; }
void missingPrefixes(bool status){ missing_prefixes = status; }

}
