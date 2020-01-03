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

#include <algorithm>
#include <functional>
#include <cctype>
#include <locale>
#include "Dependency.h"
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <sys/param.h>
#include "Utils.h"
#include "Settings.h"
#include "DylibBundler.h"

#include <stdlib.h>
#include <sstream>
#include <vector>


//initialize the dylib search paths
void initSearchPaths()
{
    //Check the same paths the system would search for dylibs
    std::string searchPaths;
    char *dyldLibPath = std::getenv("DYLD_LIBRARY_PATH");
    if( dyldLibPath!=0 )
        searchPaths = dyldLibPath;
    dyldLibPath = std::getenv("DYLD_FALLBACK_FRAMEWORK_PATH");
    if (dyldLibPath != 0)
    {
        if (!searchPaths.empty() && searchPaths[ searchPaths.size()-1 ] != ':') searchPaths += ":";
        searchPaths += dyldLibPath;
    }
    dyldLibPath = std::getenv("DYLD_FALLBACK_LIBRARY_PATH");
    if (dyldLibPath!=0 )
    {
        if (!searchPaths.empty() && searchPaths[ searchPaths.size()-1 ] != ':') searchPaths += ":";
        searchPaths += dyldLibPath;
    }
    if (!searchPaths.empty())
    {
        std::stringstream ss(searchPaths);
        std::string item;
        while(std::getline(ss, item, ':'))
        {
            if (item[ item.size()-1 ] != '/') item += "/";
            Settings::addSearchPath(item);
        }
    }
}


Dependency::Dependency(std::string path, std::string dependent_file)
    : is_framework(false)
{
    char original_file_buffer[PATH_MAX];
    std::string original_file;
    std::string warning_msg;

    rtrim_in_place(path);

    if (isRpath(path))
    {
        original_file = searchFilenameInRpaths(path, dependent_file);
    }
    else if (not realpath(path.c_str(), original_file_buffer))
    {
        warning_msg = "\n/!\\ WARNING : Cannot resolve path '" + path + "'\n";
        original_file = path;
    }
    else
    {
        original_file = original_file_buffer;
    }

    // check if given path is a symlink
    if (original_file != path) addSymlink(path);

    prefix = filePrefix(original_file);
    filename = stripPrefix(original_file);

    // check if this dependency is in /usr/lib, /System/Library, or in ignored list
    if (!Settings::isPrefixBundled(prefix)) return;

    if (original_file.find(".framework") != std::string::npos)
    {
        is_framework = true;
        std::string framework_root = getFrameworkRoot(original_file);
        std::string framework_path = getFrameworkPath(original_file);
        std::string framework_name = stripPrefix(framework_root);
        prefix = filePrefix(framework_root);
        filename = framework_name + "/" + framework_path;
    }

    //check if the lib is in a known location
    if ( !prefix.empty() && prefix[ prefix.size()-1 ] != '/' ) prefix += "/";
    if ( prefix.empty() || !fileExists( prefix+filename ) )
    {
        //the paths contains at least /usr/lib so if it is empty we have not initialized it
        size_t search_path_count = Settings::searchPathAmount();
        if ( search_path_count == 0 ) initSearchPaths();
        
        //check if file is contained in one of the paths
        search_path_count = Settings::searchPathAmount();
        for(size_t i=0; i<search_path_count; ++i)
        {
            std::string search_path = Settings::searchPath(i);
            if (fileExists( search_path+filename ))
            {
                warning_msg += "FOUND " + filename + " in " + search_path + "\n";
                prefix = search_path;
                Settings::missingPrefixes(true); //the prefix was missing
                break;
            }
        }
    }

    if ( !Settings::quietOutput() ) std::cout << warning_msg;
    
    //If the location is still unknown, ask the user for search path
    if( !Settings::isPrefixIgnored(prefix)
        && ( prefix.empty() || !fileExists( prefix+filename ) ) )
    {
        if ( !Settings::quietOutput() )
        {
            std::cerr << "\n/!\\ WARNING: Dependency " << filename << " of " << dependent_file << " not found\n";
        }
        Settings::missingPrefixes(true);
        Settings::addSearchPath( getUserInputDirForFile(filename, dependent_file) );
    }

    new_name = filename;
}

void Dependency::print()
{
    std::cout << "\n* " << filename.c_str() << " from " << prefix.c_str() << std::endl;
    
    const int symamount = symlinks.size();
    for(int n=0; n<symamount; n++)
        std::cout << "    symlink --> " << symlinks[n].c_str() << std::endl;
}

std::string Dependency::getInstallPath()
{
    return Settings::destFolder() + new_name;
}

std::string Dependency::getInnerPath()
{
    return Settings::inside_lib_path() + new_name;
}

void Dependency::addSymlink(std::string s)
{
    // calling std::find on this vector is not near as slow as an extra invocation of install_name_tool
    if(std::find(symlinks.begin(), symlinks.end(), s) == symlinks.end()) symlinks.push_back(s);
}

// Compares the given Dependency with this one. If both refer to the same file,
// it returns true and merges both entries into one.
bool Dependency::mergeIfSameAs(Dependency& dep2)
{
    if(dep2.getOriginalFileName().compare(filename) == 0)
    {
        const int samount = getSymlinkAmount();
        for(int n=0; n<samount; n++) {
            dep2.addSymlink(getSymlink(n));
        }
        return true;
    }
    return false;
}

void Dependency::copyYourself()
{
    std::string original_path = getOriginalPath();
    std::string dest_path = getInstallPath();

    if (is_framework)
    {
        original_path = getFrameworkRoot(original_path);
        dest_path = Settings::destFolder() + stripPrefix(original_path);
    }

    copyFile(original_path, dest_path);
    
    if (is_framework)
    {
        std::string headers_path = dest_path + std::string("/Headers");
        char buffer[PATH_MAX];
        if (realpath(rtrim(headers_path).c_str(), buffer)) headers_path = buffer;
        // delete headers directory
        deleteFile(headers_path, true);
    }

    // Fix the lib's inner name
    changeId(getInstallPath(), "@rpath/"+new_name);
}

void Dependency::fixFileThatDependsOnMe(std::string file_to_fix)
{
    // for main lib file
    changeInstallName(file_to_fix, getOriginalPath(), getInnerPath());
    // for symlinks
    const int symamount = symlinks.size();
    for(int n=0; n<symamount; n++)
    {
        changeInstallName(file_to_fix, symlinks[n], getInnerPath());
    }
    
    // FIXME - hackish
    if (Settings::missingPrefixes())
    {
        // for main lib file
        changeInstallName(file_to_fix, filename, getInnerPath());
        // for symlinks
        const int symamount = symlinks.size();
        for(int n=0; n<symamount; n++)
        {
            changeInstallName(file_to_fix, symlinks[n], getInnerPath());
        }
    }
}
