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

#include <stdlib.h>
#include <sstream>
#include <vector>

std::string stripPrefix(std::string in)
{
    return in.substr(in.rfind("/")+1);
}

std::string& rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
    return s;
}

//the pathes to search for dylibs, store it globally to parse the environment variables only once
std::vector<std::string> pathes;

//initialize the dylib search pathes
void initSearchPathes(){
    //Check the same pathes the system would search for dylibs
    std::string searchPathes;
    char *dyldLibPath = std::getenv("DYLD_LIBRARY_PATH");
    if( dyldLibPath!=0 )
        searchPathes = dyldLibPath;
    dyldLibPath = std::getenv("DYLD_FALLBACK_FRAMEWORK_PATH");
    if (dyldLibPath != 0)
    {
        if (!searchPathes.empty() && searchPathes[ searchPathes.size()-1 ] != ':') searchPathes += ":";
        searchPathes += dyldLibPath;
    }
    dyldLibPath = std::getenv("DYLD_FALLBACK_LIBRARY_PATH");
    if (dyldLibPath!=0 )
    {
        if (!searchPathes.empty() && searchPathes[ searchPathes.size()-1 ] != ':') searchPathes += ":";
        searchPathes += dyldLibPath;
    }
    if (!searchPathes.empty())
    {
        std::stringstream ss(searchPathes);
        std::string item;
        while(std::getline(ss, item, ':'))
        {
            if (item[ item.size()-1 ] != '/') item += "/";
            pathes.push_back(item);
        }
    }
}

// if some libs are missing prefixes, this will be set to true
// more stuff will then be necessary to do
bool missing_prefixes = false;

Dependency::Dependency(std::string path,
		       const std::string & dependent)
{
    char original_file_buffer[PATH_MAX];
    std::string original_file;

    original_file = path;

    size_t pos = original_file.find("@loader_path/");
    if (pos  != std::string::npos) {
	    size_t starting_dir_end = dependent.rfind("/");
	    if (starting_dir_end != std::string::npos) {
		    original_file.replace(pos,pos+13,
					  dependent.substr(0,starting_dir_end+1));
	    } else {
		    original_file.replace(pos,pos+13,"./");
	    }
    }

    //TODO: deal with @... paths
    if (not realpath(rtrim(original_file).c_str(), original_file_buffer))
    {
        std::cerr << "\n/!\\ WARNING : Cannot resolve path '" << path.c_str() << "'" << std::endl;
    }
    else
    {
        original_file = original_file_buffer;
    }

    // we iterate over dependencies.
    // in dependencies we must store the requested file name
    //

    filename = stripPrefix(original_file);
    prefix = original_file.substr(0, original_file.rfind("/")+1);
    addSymlink(path,dependent);



    //check if the lib is in a known location
    if( !prefix.empty() && prefix[ prefix.size()-1 ] != '/' ) prefix += "/";
    if( prefix.empty() || !fileExists( prefix+filename ) )
    {
        //the pathes contains at least /usr/lib so if it is empty we have not initilazed it
        if( pathes.empty() ) initSearchPathes();

        //check if file is contained in one of the pathes
        for( size_t i=0; i<pathes.size(); ++i)
        {
            if (fileExists( pathes[i]+filename ))
            {
                std::cout << "FOUND " << filename << " in " << pathes[i] << std::endl;
                prefix = pathes[i];
                missing_prefixes = true; //the prefix was missing
                break;
            }
        }
    }

    //If the location is still unknown, ask the user for search path
    if( !Settings::isPrefixIgnored(prefix)
        && ( prefix.empty() || !fileExists( prefix+filename ) ) )
    {
        std::cerr << "\n/!\\ WARNING : Library " << filename << " has an incomplete name (location unknown)" << std::endl;
        missing_prefixes = true;

        while (true)
        {
		// allow to fail in scripts
	    std::cout << "Please specify now where this library can be found (or write 'quit' to abort): ";
	    fflush(stdout);

            char buffer[128];
            std::cin >> buffer;
            prefix = buffer;
            std::cout << std::endl;

            if(std::cin.eof() || std::cin.fail() || prefix.compare("quit")==0) exit(1);

            if( !prefix.empty() && prefix[ prefix.size()-1 ] != '/' ) prefix += "/";

            if( !fileExists( prefix+filename ) )
            {
                std::cerr << (prefix+filename) << " does not exist. Try again" << std::endl;
                continue;
            }
            else
            {
                pathes.push_back( prefix );
                std::cerr << (prefix+filename) << " was found. /!\\MANUALLY CHECK THE EXECUTABLE WITH 'otool -L', DYLIBBUNDLDER MAY NOT HANDLE CORRECTLY THIS UNSTANDARD/ILL-FORMED DEPENDENCY" << std::endl;
                break;
            }
        }
    }

    //new_name  = filename.substr(0, filename.find(".")) + ".dylib";
    new_name = filename;
}

void Dependency::print() const
{
    std::cout << std::endl;
    std::cout << " * " << filename << " from " << prefix << std::endl;

    for (filenamelist::const_iterator i = dependents.begin();
	 i != dependents.end();
	 ++i)
	    std::cout << "\tsymlink " << i->first << " <-- " << i->second << std::endl;
}

std::string Dependency::getInstallPath() const
{
    return Settings::destFolder() + new_name;
}
std::string Dependency::getInnerPath() const
{
    return Settings::inside_lib_path() + new_name;
}

void Dependency::copyYourself() const
{
    copyFile(getOriginalPath(), getInstallPath());

    // Fix the lib's inner name
    std::string command = std::string("install_name_tool -id ") + getInnerPath() + " " + getInstallPath();
    if( systemp( command ) != 0 )
    {
        std::cerr << "\n\nError : An error occured while trying to change identity of library " << getInstallPath() << std::endl;
        exit(1);
    }
}

void Dependency::fixFileThatDependsOnMe(const std::string & file,
					const std::string & symlink)
{
    // for main lib file
    std::string command = std::string("install_name_tool -change ") +
	    symlink + " " + getInnerPath() + " " + file;

    if( systemp( command ) != 0 )
    {
        std::cerr << "\n\nError : An error occured while trying to fix depencies of " << file << std::endl;
        exit(1);
    }

    return;

    // I don't know what is inteded by missing_prefixes
    // it should be easy enough to call a shell script with correct
    // search path for missing files.
#if 0
    // for symlinks
    for(filenamelist::iterator i = dependents.begin();
	i != dependents.end(); ++i) {
        std::string command = std::string("install_name_tool -change ") +
        *i + " " + getInnerPath() + " " + file_to_fix;

        if( systemp( command ) != 0 )
        {
            std::cerr << "\n\nError : An error occured while trying to fix depencies of " << file_to_fix << std::endl;
            exit(1);
        }
    }


    // FIXME - hackish
    if(missing_prefixes)
    {
        // for main lib file
        std::string command = std::string("install_name_tool -change ") +
        filename + " " + getInnerPath() + " " + file_to_fix;

        if( systemp( command ) != 0 )
        {
            std::cerr << "\n\nError : An error occured while trying to fix depencies of " << file_to_fix << std::endl;
            exit(1);
        }

        // for symlinks
        const int symamount = symlinks.size();
        for(int n=0; n<symamount; n++)
        {
            std::string command = std::string("install_name_tool -change ") +
            symlinks[n] + " " + getInnerPath() + " " + file_to_fix;

            if( systemp( command ) != 0 )
            {
                std::cerr << "\n\nError : An error occured while trying to fix depencies of " << file_to_fix << std::endl;
                exit(1);
            }
        }//next
    }// end if(missing_prefixes)
#endif
}
