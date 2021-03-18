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


#ifndef _depend_h_
#define _depend_h_

#include <string>
#include <vector>

class Dependency
{
    // origin
    std::string filename;
    std::string prefix;
    std::vector<std::string> symlinks;
    
    // installation
    std::string new_name;
public:
    Dependency(std::string path, const std::string& dependent_file);

    void print();

    std::string getOriginalFileName() const{ return filename; }
    std::string getOriginalPath() const{ return prefix+filename; }
    std::string getInstallPath();
    std::string getInnerPath();
        
    void addSymlink(const std::string& s);
    int getSymlinkAmount() const{ return symlinks.size(); }

    std::string getSymlink(const int i) const{ return symlinks[i]; }
    std::string getPrefix() const{ return prefix; }

    void copyYourself();
    void fixFileThatDependsOnMe(const std::string& file);
    
    // Compares the given dependency with this one. If both refer to the same file,
    // it returns true and merges both entries into one.
    bool mergeIfSameAs(Dependency& dep2);
};


#endif