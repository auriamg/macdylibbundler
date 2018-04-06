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

#include "DylibBundler.h"
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <queue>
#include "Utils.h"
#include "Settings.h"
#include "Dependency.h"


typedef std::set<Dependency> DependencyMap;
DependencyMap deps;
std::queue <std::string> openDeps;


void addOpenDeps(const std::string & path)
{
	openDeps.push(path);
	deps.insert(Dependency(path,""));
}

#if 0
void changeLibPathsOnFile(std::string file_to_fix)
{
    std::cout << "\n* Fixing dependencies on " << file_to_fix.c_str() << std::endl;
    deps[file_to_fix].fixFilesThatDependOnMe();
}
#endif


Dependency & addDependency(const std::string & path,
			   const std::string & dependent)
{
    Dependency dep(path,dependent);

    DependencyMap::iterator pos = deps.find(dep);
    if (pos == deps.end()) {
	    pos = deps.insert(dep).first;
	    if(Settings::isPrefixBundled(dep.getPrefix()))
		    openDeps.push(dep.getOriginalPath());

    } else
	    const_cast<Dependency &>(*pos).merge(dep);
    return const_cast<Dependency &>(*pos);
}

/*
 *  Fill vector 'lines' with dependencies of given 'filename'
 */
void collectDependencies(std::string filename, std::vector<std::string>& lines)
{
    // execute "otool -L" on the given file and collect the command's output
    std::string cmd = "otool -L " + filename;
    std::string output = system_get_output(cmd);

    if(output.find("can't open file")!=std::string::npos or output.find("No such file")!=std::string::npos or output.size()<1)
    {
        std::cerr << "Cannot find file " << filename << " to read its dependencies" << std::endl;
        exit(1);
    }

    // split output
    tokenize(output, "\n", &lines);
}

// copying the string is ok here, since the original might not be stable.
void collectDependencies(std::string filename)
{
    DependencyMap::iterator d = deps.find(Dependency(filename,""));
    if (d == deps.end()) {
	    std::cerr << "Internal error: Dependency " << filename << " is not initialized" << std::endl;
	    exit(1);
    }
    Dependency & dep = const_cast<Dependency &>(*d);
    filename = dep.getOriginalPath();

    std::vector<std::string> lines;
    collectDependencies(filename, lines);

    std::cout << "."; fflush(stdout);

    std::string newlibrary;
    const int line_amount = lines.size();
    for(int n=0; n<line_amount; n++)
    {
	newlibrary = lines[n];
        std::cout << "."; fflush(stdout);
	if (newlibrary.empty()) continue; // ignore empty lines
        if (newlibrary[0] != '\t') continue; // only lines beginning with a tab interest us
        if (newlibrary.find(".framework") != std::string::npos ) continue;
	// Ignore frameworks, we can not handle them

	// trim useless info, keep only library name
	newlibrary = newlibrary.substr(1, lines[n].rfind(" (") - 1);
	std::cout << filename
		  << " -> "
		  << newlibrary << std::endl;
	addDependency(newlibrary,filename);
    }
}
void collectSubDependencies()
{
    // print status to user
    int dep_amount = deps.size();

    // recursively collect each dependencie's dependencies
    while(!openDeps.empty())
    {
	    collectDependencies(openDeps.front());
	    openDeps.pop();
    }
#if 0
#if 0
        dep_amount = deps.size();
        for(int m=0; m<dep_amount; m++)
        {
            std::cout << "."; fflush(stdout);
            std::vector<std::string> lines;
            collectDependencies(deps[m].getOriginalPath(), lines);

            const int line_amount = lines.size();
            for(int n=0; n<line_amount; n++)
            {
                if(lines[n][0] != '\t') continue; // only lines beginning with a tab interest us
                if( lines[n].find(".framework") != std::string::npos ) continue; //Ignore frameworks, we can not handle them

		std::cout <<  (deps[m].getOriginalPath())
			  << " -> "
			  << lines[n].substr(1, lines[n].rfind(" (") - 1)<< std::endl;
                addDependency( // trim useless info, keep only library name
                               lines[n].substr(1, lines[n].rfind(" (") - 1)
                               );
            }//next
#else
	    collectDependencies(deps[m].getOriginalPath());
#endif
        }//next

        if(deps.size() == dep_amount) break; // no more dependencies were added on this iteration, stop searching
    }
#endif
}

void createDestDir()
{
    std::string dest_folder = Settings::destFolder();
    std::cout << "* Checking output directory " << dest_folder.c_str() << std::endl;

    // ----------- check dest folder stuff ----------
    bool dest_exists = fileExists(dest_folder);

    if(dest_exists and Settings::canOverwriteDir())
    {
        std::cout << "* Erasing old output directory " << dest_folder.c_str() << std::endl;
        std::string command = std::string("rm -r ") + dest_folder;
        if( systemp( command ) != 0)
        {
            std::cerr << "\n\nError : An error occured while attempting to override dest folder." << std::endl;
            exit(1);
        }
        dest_exists = false;
    }

    if(!dest_exists)
    {

        if(Settings::canCreateDir())
        {
            std::cout << "* Creating output directory " << dest_folder.c_str() << std::endl;
            std::string command = std::string("mkdir -p ") + dest_folder;
            if( systemp( command ) != 0)
            {
                std::cerr << "\n\nError : An error occured while creating dest folder." << std::endl;
                exit(1);
            }
        }
        else
        {
            std::cerr << "\n\nError : Dest folder does not exist. Create it or pass the appropriate flag for automatic dest dir creation." << std::endl;
            exit(1);
        }
    }

}

void doneWithDeps_go()
{
    std::cout << std::endl;
    for (DependencyMap::iterator i = deps.begin();
	 i != deps.end(); ++i) {
        i->print();
    }
    std::cout << std::endl;

    // copy files if requested by user
    if(Settings::bundleLibs())
    {
        createDestDir();

	for (DependencyMap::iterator i = deps.begin();
	     i != deps.end(); ++i) {
		i->copyYourself();
	}
    }
    for (DependencyMap::iterator i = deps.begin();
	 i != deps.end(); ++i) {
	    i->copyYourself();
    }
}
