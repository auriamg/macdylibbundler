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
#include <regex>
#include <set>
#include <map>
#include <sys/param.h>
#ifdef __linux
#include <linux/limits.h>
#endif
#ifndef __clang__
#include <sys/types.h>
#endif
#include "Utils.h"
#include "Settings.h"
#include "Dependency.h"
using namespace std;

std::vector<Dependency> deps;
std::map<std::string, std::vector<Dependency> > deps_per_file;
std::map<std::string, bool> deps_collected;
std::set<std::string> frameworks;
std::set<std::string> rpaths;
std::map<std::string, bool> rpaths_collected;
std::map<std::string, std::vector<std::string> > rpaths_per_file;
bool qt_plugins_called = false;

void changeLibPathsOnFile(std::string file_to_fix)
{
    if (deps_collected.find(file_to_fix) == deps_collected.end()
        || rpaths_collected.find(file_to_fix) == rpaths_collected.end())
    {
        collectDependenciesRpaths(file_to_fix);
    }
    if (!Settings::quietOutput()) std::cout << "\n";
    std::cout << "* Fixing dependencies on " << file_to_fix.c_str() << std::endl;
    
    const int dep_amount = deps_per_file[file_to_fix].size();
    for(int n=0; n<dep_amount; n++)
    {
        deps_per_file[file_to_fix][n].fixFileThatDependsOnMe(file_to_fix);
    }
}

void fixRpathsOnFile(const std::string& original_file, const std::string& file_to_fix)
{
    if (!Settings::fileHasRpath(original_file)) return;
    
    std::vector<std::string> rpaths_to_fix = Settings::getRpathsForFile(original_file);
    for (const auto& rpath_to_fix : rpaths_to_fix)
    {
        std::string command =
            std::string("install_name_tool -rpath ") + rpath_to_fix + " "
            + Settings::inside_lib_path() + " " + file_to_fix;
        if (systemp(command) != 0)
        {
            std::cerr << "\n\n/!\\ ERROR: An error occured while trying to fix rpath "
                      << rpath_to_fix << " of " << file_to_fix << std::endl;
            exit(1);
        }
    }
}

void addDependency(const std::string& path, const std::string& filename)
{
    Dependency dep(path, filename);
    
    // we need to check if this library was already added to avoid duplicates
    bool in_deps = false;
    for (auto& d : deps)
    {
        if (dep.mergeIfSameAs(d)) in_deps = true;
    }
    
    // check if this library was already added to |deps_per_file[filename]| to avoid duplicates
    bool in_deps_per_file = false;
    for (auto& d : deps_per_file[filename])
    {
        if (dep.mergeIfSameAs(d)) in_deps_per_file = true;
    }

    // check if this library is in /usr/lib, /System/Library, or in ignored list
    if(!Settings::isPrefixBundled(dep.getPrefix())) return;

    if(!in_deps && dep.isFramework()) frameworks.insert(dep.getOriginalPath());
    
    if(!in_deps) deps.push_back(dep);
    if(!in_deps_per_file) deps_per_file[filename].push_back(dep);
}

void collectDependenciesRpaths(const std::string& dependent_file)
{
    if (deps_collected.find(dependent_file) != deps_collected.end()
        && rpaths_per_file.find(dependent_file) != rpaths_per_file.end())
    {
        return;
    }

    std::map<std::string, std::string> cmds_values;
    std::string dylib = "LC_LOAD_DYLIB";
    std::string rpath = "LC_RPATH";
    cmds_values[dylib] = "name";
    cmds_values[rpath] = "path";
    std::map<std::string, std::vector<std::string>> cmds_results;

    parseLoadCommands(dependent_file, cmds_values, cmds_results);

    if (rpaths_collected.find(dependent_file) == rpaths_collected.end())
    {
        auto rpath_results = cmds_results[rpath];
        for (const auto& rpath_result : rpath_results)
        {
            rpaths.insert(rpath_result);
            Settings::addRpathForFile(dependent_file, rpath_result);
        }
        rpaths_collected[dependent_file] = true;
    }

    if (deps_collected.find(dependent_file) == deps_collected.end())
    {
        auto dylib_results = cmds_results[dylib];
        for (const auto& dylib_result : dylib_results)
        {
            // skip system/ignored prefixes
            if (Settings::isPrefixBundled(dylib_result))
                addDependency(dylib_result, dependent_file);
        }
        deps_collected[dependent_file] = true;
    }
}

void collectSubDependencies()
{
    size_t dep_counter = deps.size();
    size_t dep_amount = deps.size();
    
    // recursively collect each dependencie's dependencies
    while(true)
    {
        dep_amount = deps.size();
        for(int n=0; n<dep_amount; n++)
        {
            std::string original_path = deps[n].getOriginalPath();
            if (isRpath(original_path))
            {
                original_path = searchFilenameInRpaths(original_path);
            }
            collectDependenciesRpaths(original_path);
        }

        if(deps.size() == dep_amount) break; // no more dependencies were added on this iteration, stop searching
    }

    if (Settings::bundleLibs() && Settings::bundleFrameworks())
    {
        if ( !qt_plugins_called || (deps.size() != dep_counter) ) copyQtPlugins();
    }
}

void createDestDir()
{
    std::string dest_folder = Settings::destFolder();
    std::cout << "Checking output directory " << dest_folder.c_str() << std::endl;
    
    // ----------- check dest folder stuff ----------
    bool dest_exists = fileExists(dest_folder);
    
    if(dest_exists and Settings::canOverwriteDir())
    {
        std::cout << "Erasing old output directory " << dest_folder.c_str() << std::endl;
        if( !deleteFile(dest_folder) )
        {
            std::cerr << "\n\nError : An error occured while attempting to overwrite dest folder." << std::endl;
            exit(1);
        }
        dest_exists = false;
    }
    
    if(!dest_exists)
    {
        
        if(Settings::canCreateDir())
        {
            std::cout << "Creating output directory " << dest_folder.c_str() << "\n\n";
            if( !mkdir(dest_folder) )
            {
                std::cerr << "\n\nError : An error occured while creating dest folder." << std::endl;
                exit(1);
            }
        }
        else
        {
            std::cerr << "\n\nError : Dest folder does not exist. Create it or pass the '-cd' or '-od' flag" << std::endl;
            exit(1);
        }
    }
    
}

void doneWithDeps_go()
{
    for (const auto& dep : deps)
    {
        dep.print();
    }
    std::cout << "\n";
    
    // copy & fix up dependencies
    if(Settings::bundleLibs())
    {
        createDestDir();
        
        for (const auto& dep : deps)
        {
            dep.copyYourself();
            changeLibPathsOnFile(dep.getInstallPath());
            fixRpathsOnFile(dep.getOriginalPath(), dep.getInstallPath());
        }
    }
    
    const int fileToFixAmount = Settings::fileToFixAmount();
    for(int n=0; n<fileToFixAmount; n++)
    {
        changeLibPathsOnFile(Settings::fileToFix(n));
        fixRpathsOnFile(Settings::fileToFix(n), Settings::fileToFix(n));
    }
}

void copyQtPlugins()
{
    bool qtCoreFound = false;
    bool qtGuiFound = false;
    bool qtNetworkFound = false;
    bool qtSqlFound = false;
    bool qtSvgFound = false;
    bool qtMultimediaFound = false;
    bool qt3dRenderFound = false;
    bool qt3dQuickRenderFound = false;
    bool qtPositioningFound = false;
    bool qtLocationFound = false;
    bool qtTextToSpeechFound = false;
    bool qtWebViewFound = false;
    std::string original_file;

    for (const auto& framework : frameworks)
    {
        if (framework.find("QtCore") != std::string::npos)
        {
            qtCoreFound = true;
            original_file = framework;
        }
        if (framework.find("QtGui") != std::string::npos)
            qtGuiFound = true;
        if (framework.find("QtNetwork") != std::string::npos)
            qtNetworkFound = true;
        if (framework.find("QtSql") != std::string::npos)
            qtSqlFound = true;
        if (framework.find("QtSvg") != std::string::npos)
            qtSvgFound = true;
        if (framework.find("QtMultimedia") != std::string::npos)
            qtMultimediaFound = true;
        if (framework.find("Qt3DRender") != std::string::npos)
            qt3dRenderFound = true;
        if (framework.find("Qt3DQuickRender") != std::string::npos)
            qt3dQuickRenderFound = true;
        if (framework.find("QtPositioning") != std::string::npos)
            qtPositioningFound = true;
        if (framework.find("QtLocation") != std::string::npos)
            qtLocationFound = true;
        if (framework.find("TextToSpeech") != std::string::npos)
            qtTextToSpeechFound = true;
        if (framework.find("WebView") != std::string::npos)
            qtWebViewFound = true;
    }

    if (!qtCoreFound) return;
    if (!qt_plugins_called) createQtConf(Settings::resourcesFolder());
    qt_plugins_called = true;

    const auto fixupPlugin = [original_file](const std::string& plugin)
    {
        std::string dest = Settings::pluginsFolder();
        std::string framework_root = getFrameworkRoot(original_file);
        std::string prefix = filePrefix(framework_root);
        std::string qt_prefix = filePrefix(prefix.substr(0, prefix.size()-1));
        std::string qt_plugins_prefix = qt_prefix + "plugins/";
        if (fileExists(qt_plugins_prefix + plugin) && !fileExists(dest + plugin))
        {
            mkdir(dest + plugin);
            copyFile(qt_plugins_prefix + plugin, dest);
            std::vector<std::string> files = lsDir(dest + plugin+"/");
            for (const auto& file : files)
            {
                Settings::addFileToFix(dest + plugin+"/"+file);
                collectDependenciesRpaths(dest + plugin+"/"+file);
                changeId(dest + plugin+"/"+file, "@rpath/" + plugin+"/"+file);
            }
        }
    };

    std::string framework_root = getFrameworkRoot(original_file);
    std::string prefix = filePrefix(framework_root);
    std::string qt_prefix = filePrefix(prefix.substr(0, prefix.size()-1));
    std::string qt_plugins_prefix = qt_prefix + "plugins/";
    std::string dest = Settings::pluginsFolder();

    if (!fileExists(dest + "platforms"))
    {
        mkdir(dest + "platforms");
        copyFile(qt_plugins_prefix + "platforms/libqcocoa.dylib", dest + "platforms");
        Settings::addFileToFix(dest + "platforms/libqcocoa.dylib");
        collectDependenciesRpaths(dest + "platforms/libqcocoa.dylib");
        changeId(dest + "platforms/libqcocoa.dylib", "@rpath/platforms/libqcocoa.dylib");
    }

    fixupPlugin("printsupport");
    fixupPlugin("styles");
    fixupPlugin("imageformats");
    fixupPlugin("iconengines");
    if (!qtSvgFound) systemp(std::string("rm -f \"") + dest + "imageformats/libqsvg.dylib\"");
    if (qtGuiFound)
    {
        fixupPlugin("platforminputcontexts");
        fixupPlugin("virtualkeyboard");
    }
    if (qtNetworkFound) fixupPlugin("bearer");
    if (qtSqlFound) fixupPlugin("sqldrivers");
    if (qtMultimediaFound)
    {
        fixupPlugin("mediaservice");
        fixupPlugin("audio");
    }
    if (qt3dRenderFound)
    {
        fixupPlugin("sceneparsers");
        fixupPlugin("geometryloaders");
    }
    if (qt3dQuickRenderFound) fixupPlugin("renderplugins");
    if (qtPositioningFound) fixupPlugin("position");
    if (qtLocationFound) fixupPlugin("geoservices");
    if (qtTextToSpeechFound) fixupPlugin("texttospeech");
    if (qtWebViewFound) fixupPlugin("webview");

    collectSubDependencies();
}
