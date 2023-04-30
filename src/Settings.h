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

#ifndef _settings_
#define _settings_

#include <string>

namespace Settings
{

bool isSystemLibrary(const std::string& prefix);
bool isPrefixBundled(const std::string& prefix);
bool isPrefixIgnored(const std::string& prefix);
void ignore_prefix(std::string prefix);
    
bool canOverwriteFiles();
void canOverwriteFiles(bool permission);

bool canOverwriteDir();
void canOverwriteDir(bool permission);

bool canCreateDir();
void canCreateDir(bool permission);

bool canCodesign();
void canCodesign(bool permission);

bool bundleLibs();
void bundleLibs(bool on);

bool searchSystemLib();
void searchSystemLib(bool on);

std::string destFolder();
void destFolder(const std::string& path);

void addFileToFix(const std::string& path);
int fileToFixAmount();
std::string fileToFix(const int n);

std::string inside_lib_path();
void inside_lib_path(const std::string& p);

void addSearchPath(const std::string& path);
int searchPathAmount();
std::string searchPath(const int n);

}
#endif
