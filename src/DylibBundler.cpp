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
#ifdef __linux
#include <linux/limits.h>
#endif
#include "Utils.h"
#include "Settings.h"
#include "Dependency.h"


std::vector<Dependency> deps;
std::map<std::string, std::vector<Dependency> > deps_per_file;
std::map<std::string, bool> deps_collected;
std::set<std::string> frameworks;
std::set<std::string> rpaths;
std::map<std::string, std::vector<std::string> > rpaths_per_file;
std::map<std::string, std::string> rpath_to_fullpath;
bool qt_plugins_called = false;

void changeLibPathsOnFile(std::string file_to_fix)
{
    if (deps_collected.find(file_to_fix) == deps_collected.end())
    {
        collectDependencies(file_to_fix);
    }
    if (!Settings::quietOutput()) std::cout << "\n";
    std::cout << "* Fixing dependencies on " << file_to_fix.c_str() << std::endl;
    
    const int dep_amount = deps_per_file[file_to_fix].size();
    for(int n=0; n<dep_amount; n++)
    {
        deps_per_file[file_to_fix][n].fixFileThatDependsOnMe(file_to_fix);
    }
}

bool isRpath(const std::string& path)
{
    return path.find("@rpath") == 0 || path.find("@loader_path") == 0;
}

void collectRpaths(const std::string& filename)
{
    if (!fileExists(filename))
    {
        std::cerr << "\n/!\\ WARNING : can't collect rpaths for nonexistent file '" << filename << "'\n";
        return;
    }

    std::string cmd = "otool -l " + filename;
    std::string output = system_get_output(cmd);

    std::vector<std::string> lc_lines;
    tokenize(output, "\n", &lc_lines);

    size_t pos = 0;
    bool read_rpath = false;
    while(pos < lc_lines.size())
    {
        std::string line = lc_lines[pos];
        pos++;

        if (read_rpath)
        {
            size_t start_pos = line.find("path ");
            size_t end_pos = line.find(" (");
            if (start_pos == std::string::npos || end_pos == std::string::npos)
            {
                std::cerr << "\n/!\\ WARNING: Unexpected LC_RPATH format\n";
                continue;
            }
            start_pos += 5;
            std::string rpath = line.substr(start_pos, end_pos - start_pos);
            rpaths.insert(rpath);
            rpaths_per_file[filename].push_back(rpath);
            read_rpath = false;
            continue;
        }

        if (line.find("LC_RPATH") != std::string::npos)
        {
            read_rpath = true;
            pos++;
        }
    }
}

void collectRpathsForFilename(const std::string& filename)
{
    if (rpaths_per_file.find(filename) == rpaths_per_file.end())
    {
        collectRpaths(filename);
    }
}

std::string searchFilenameInRpaths(const std::string& rpath_file, const std::string& dependent_file)
{
    char fullpath_buffer[PATH_MAX];
    std::string fullpath;
    std::string suffix = rpath_file.substr(rpath_file.rfind("/")+1);

    const auto check_path = [&](std::string path)
    {
        char buffer[PATH_MAX];
        std::string file_prefix = filePrefix(dependent_file);
        if (path.find("@executable_path") != std::string::npos || path.find("@loader_path") != std::string::npos)
        {
            if (path.find("@executable_path") != std::string::npos)
            {
                if (Settings::appBundleProvided())
                {
                    path = std::regex_replace(path, std::regex("@executable_path/"), Settings::executableFolder());
                }
            }
            if (dependent_file != rpath_file)
            {
                if (path.find("@loader_path") != std::string::npos)
                {
                    path = std::regex_replace(path, std::regex("@loader_path/"), file_prefix);
                }
            }
            if (realpath(path.c_str(), buffer))
            {
                fullpath = buffer;
                rpath_to_fullpath[rpath_file] = fullpath;
                return true;
            }
        }
        else if (path.find("@rpath") != std::string::npos)
        {
            if (Settings::appBundleProvided())
            {
                std::string pathE = std::regex_replace(path, std::regex("@rpath/"), Settings::executableFolder());
                if (realpath(pathE.c_str(), buffer))
                {
                    fullpath = buffer;
                    rpath_to_fullpath[rpath_file] = fullpath;
                    return true;
                }
            }
            if (dependent_file != rpath_file)
            {
                std::string pathL = std::regex_replace(path, std::regex("@rpath/"), file_prefix);
                if (realpath(pathL.c_str(), buffer))
                {
                    fullpath = buffer;
                    rpath_to_fullpath[rpath_file] = fullpath;
                    return true;
                }
            }
        }
        return false;
    };

    // fullpath previously stored
    if (rpath_to_fullpath.find(rpath_file) != rpath_to_fullpath.end())
    {
        fullpath = rpath_to_fullpath[rpath_file];
    }
    else if (!check_path(rpath_file))
    {
        for(auto it = rpaths_per_file[dependent_file].begin(); it != rpaths_per_file[dependent_file].end(); ++it)
        {
            std::string rpath = *it;
            if (rpath[rpath.size()-1] != '/') rpath += "/";

            std::string path = rpath + suffix;
            if (check_path(path)) break;
        }
    }

    if (fullpath.empty())
    {
        size_t search_path_count = Settings::searchPathAmount();
        for(size_t i=0; i<search_path_count; ++i)
        {
            std::string search_path = Settings::searchPath(i);
            if (fileExists(search_path+suffix))
            {
                fullpath = search_path + suffix;
                break;
            }
        }
        if (fullpath.empty())
        {
            if (!Settings::quietOutput())
            {
                std::cerr << "\n/!\\ WARNING : can't get path for '" << rpath_file << "'\n";
            }
            fullpath = getUserInputDirForFile(suffix, dependent_file) + suffix;
            if (Settings::quietOutput() && fullpath.empty())
            {
                std::cerr << "\n/!\\ WARNING: Can't get path for '" << rpath_file << "'\n";
            }
            if (realpath(fullpath.c_str(), fullpath_buffer))
            {
                fullpath = fullpath_buffer;
            }
        }
    }

    return fullpath;
}

std::string searchFilenameInRpaths(const std::string& rpath_file)
{
    return searchFilenameInRpaths(rpath_file, rpath_file);
}

void fixRpathsOnFile(const std::string& original_file, const std::string& file_to_fix)
{
    std::vector<std::string> rpaths_to_fix;
    std::map<std::string, std::vector<std::string> >::iterator found = rpaths_per_file.find(original_file);
    if (found != rpaths_per_file.end())
    {
        rpaths_to_fix = found->second;
    }

    for (size_t i=0; i < rpaths_to_fix.size(); ++i)
    {
        std::string command = std::string("install_name_tool -rpath \"") +
                rpaths_to_fix[i] + "\" \"" + Settings::inside_lib_path() + "\" \"" + file_to_fix + "\"";
        if ( systemp(command) != 0)
        {
            std::cerr << "\n\nError : An error occured while trying to fix dependencies of " << file_to_fix << std::endl;
            exit(1);
        }
    }
}

void addDependency(std::string path, std::string filename)
{
    Dependency dep(path, filename);
    
    // we need to check if this library was already added to avoid duplicates
    bool in_deps = false;
    const int dep_amount = deps.size();
    for(int n=0; n<dep_amount; n++)
    {
        if(dep.mergeIfSameAs(deps[n])) in_deps = true;
    }
    
    // check if this library was already added to |deps_per_file[filename]| to avoid duplicates
    bool in_deps_per_file = false;
    const int deps_in_file_amount = deps_per_file[filename].size();
    for(int n=0; n<deps_in_file_amount; n++)
    {
        if(dep.mergeIfSameAs(deps_per_file[filename][n])) in_deps_per_file = true;
    }

    if(!Settings::isPrefixBundled(dep.getPrefix())) return;

    if(!in_deps && dep.isFramework()) frameworks.insert(dep.getOriginalPath());
    
    if(!in_deps) deps.push_back(dep);
    if(!in_deps_per_file) deps_per_file[filename].push_back(dep);
}

/*
 *  Fill vector 'lines' with dependencies of given 'filename'
 */
void collectDependencies(std::string filename, std::vector<std::string>& lines)
{
    // execute "otool -l" on the given file and collect the command's output
    std::string cmd = "otool -l \"" + filename + "\"";
    std::string output = system_get_output(cmd);

    if (output.find("can't open file")!=std::string::npos or output.find("No such file")!=std::string::npos or output.size()<1)
    {
        std::cerr << "Cannot find file " << filename << " to read its dependencies" << std::endl;
        exit(1);
    }
    
    // split output
    std::vector<std::string> raw_lines;
    tokenize(output, "\n", &raw_lines);

    bool searching = false;
    for(const auto& line : raw_lines) {
        if (line.find("cmd LC_LOAD_DYLIB") != std::string::npos)
        {
            if (searching)
            {
                std::cerr << "\n\n/!\\ ERROR: Failed to find name before next cmd" << std::endl;
                exit(1);
            }
            searching = true;
        }
        else if (searching)
        {
            size_t found = line.find("name ");
            if (found != std::string::npos)
            {
                lines.push_back('\t' + line.substr(found+5, std::string::npos));
                searching = false;
            }
        }
    }
}

void collectDependencies(std::string filename)
{
    std::vector<std::string> lines;
    collectDependencies(filename, lines);

    const int line_amount = lines.size();
    for(int n=0; n<line_amount; n++)
    {
        if(lines[n][0] != '\t') continue; // only lines beginning with a tab interest us
        if(!Settings::isPrefixBundled(lines[n])) continue; // skip system/ignored prefixes
        // trim useless info, keep only library name
        std::string dep_path = lines[n].substr(1, lines[n].rfind(" (") - 1);
        addDependency(dep_path, filename);
    }
    deps_collected[filename] = true;
}

void collectSubDependencies()
{
    size_t dep_counter = deps.size();
    int dep_amount = deps.size();
    
    // recursively collect each dependencie's dependencies
    while(true)
    {
        dep_amount = deps.size();
        for(int n=0; n<dep_amount; n++)
        {
            std::vector<std::string> lines;
            std::string original_path = deps[n].getOriginalPath();
            if (isRpath(original_path))
            {
                original_path = searchFilenameInRpaths(original_path);
            }
            collectRpathsForFilename(original_path);
            collectDependencies(original_path, lines);
            
            const int line_amount = lines.size();
            for(int n=0; n<line_amount; n++)
            {
                if (lines[n][0] != '\t') continue; // only lines beginning with a tab interest us
                // trim useless info, keep only library name
                std::string dep_path = lines[n].substr(1, lines[n].rfind(" (") - 1);
                if (!Settings::isPrefixBundled(dep_path)) continue; // skip system/ignored prefixes

                addDependency(dep_path, original_path);
            }//next
        }//next
        
        if (deps.size() == dep_amount) break; // no more dependencies were added on this iteration, stop searching
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
        std::string command = std::string("rm -r \"") + dest_folder + "\"";
        if( systemp( command ) != 0)
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
            std::string command = std::string("mkdir -p \"") + dest_folder + "\"";
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
    const int dep_amount = deps.size();
    // print info to user
    for(int n=0; n<dep_amount; n++)
    {
        deps[n].print();
    }
    std::cout << std::endl;
    
    // copy files if requested by user
    if(Settings::bundleLibs())
    {
        createDestDir();
        
        for(int n=0; n<dep_amount; n++)
        {
            deps[n].copyYourself();
            changeLibPathsOnFile(deps[n].getInstallPath());
            fixRpathsOnFile(deps[n].getOriginalPath(), deps[n].getInstallPath());
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

    for (std::set<std::string>::iterator it = frameworks.begin(); it != frameworks.end(); ++it)
    {
        std::string framework = *it;
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

    const auto fixupPlugin = [original_file](std::string plugin)
    {
        std::string dest = Settings::pluginsFolder();
        std::string framework_root = getFrameworkRoot(original_file);
        std::string prefix = filePrefix(framework_root);
        std::string qt_prefix = filePrefix(prefix.substr(0, prefix.size()-1));
        std::string qt_plugins_prefix = qt_prefix + "plugins/";
        if (fileExists(qt_plugins_prefix + plugin))
        {
            mkdir(dest + plugin);
            copyFile(qt_plugins_prefix + plugin, dest);
            std::vector<std::string> files = lsDir(dest + plugin+"/");
            for (const auto& file : files)
            {
                Settings::addFileToFix(dest + plugin+"/"+file);
                collectDependencies(dest + plugin+"/"+file);
                changeId(dest + plugin+"/"+file, "@rpath/" + plugin+"/"+file);
            }
        }
    };

    std::string framework_root = getFrameworkRoot(original_file);
    std::string prefix = filePrefix(framework_root);
    std::string qt_prefix = filePrefix(prefix.substr(0, prefix.size()-1));
    std::string qt_plugins_prefix = qt_prefix + "plugins/";

    std::string dest = Settings::pluginsFolder();
    mkdir(dest + "platforms");
    copyFile(qt_plugins_prefix + "platforms/libqcocoa.dylib", dest + "platforms");
    Settings::addFileToFix(dest + "platforms/libqcocoa.dylib");
    collectDependencies(dest + "platforms/libqcocoa.dylib");

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
