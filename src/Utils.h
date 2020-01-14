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


#ifndef _utils_h_
#define _utils_h_

#include <string>
#include <vector>

class Library;

void tokenize(const std::string& str, const char* delimiters, std::vector<std::string>*);
bool fileExists( std::string filename );

void copyFile(std::string from, std::string to);

// executes a command in the native shell and returns output in string
std::string system_get_output(std::string cmd);

// like 'system', runs a command on the system shell, but also prints the command to stdout.
int systemp(std::string& cmd);
void changeInstallName(const std::string& binary_file, const std::string& old_name, const std::string& new_name);
std::string getUserInputDirForFile(const std::string& filename);

#endif
