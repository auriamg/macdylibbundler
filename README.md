[![Build Status](https://travis-ci.org/auriamg/macdylibbundler.svg?branch=master)](https://travis-ci.org/auriamg/macdylibbundler)

mac dylib bundler
================


About
-----

Mac OS X introduced an innovative and very useful way to package applications : app bundles.
While their design has all that is needed to ease distribution of resources and frameworks, it
seems like dynamic libraries (.dylib) are very complicated to distribute. Sure, applications developed
specifically for OS X won't make use of them, however applications ported from Linux or other Unices may have
dependencies that will only compile as dylibs. By default, there exists no mechanism to bundle them but some command-line utilities provided by Apple - however it turns out that for a single program it is often necessary to issue dozens of commands! This often leads each porter to develop their own "home solution" wich are often hacky, poorly portable and/or nonoptimal.

**dylibbundler** is a small command-line programs that aims to make bundling .dylibs as easy as possible.
It automatically determines which dylibs are needed by your program, copies these libraries inside the app bundle, and fixes both them and the executable to be ready for distribution... all this with a single command on the teminal! It will also work if your program uses plug-ins that have dependencies too.

It usually involves 2 actions :
* Creating a directory (by default called *libs*) that can be placed inside the *Contents* folder of the app bundle.
* Fixing the executable file so that it is aware of the new location of its dependencies.




Installation
------------
In the terminal, cd to the main directory of dylibbundler and type "make". You can install with "sudo make install".


Feedback / Contact
------------------
You can contact me here on github, for instance by creating a ticket or pull request


Using dylibbundler on the terminal
----------------------------------
Here is a list of flags you can pass to dylibbundler on the terminal.

`-h`, `--help`
<blockquote>
displays a summary of options
</blockquote>

`-x`, `--fix-file` (executable or plug-in filepath)
<blockquote>
Fixes given executable or plug-in file (a .dylib can work too. anything on which `otool -L` works is accepted by `-x`). Dylibbundler will walk through the dependencies of the specified file to build a dependency list. It will also fix the said files' dependencies so that it expects to find the libraries relative to itself (e.g. in the app bundle) instead of at an absolute path (e.g. /usr/local/lib). To pass multiple files to fix, simply specify multiple `-x` flags.
</blockquote>

`-b`, `--bundle-deps`
<blockquote>
Copies libaries to a local directory, fixes their internal name so that they are aware of their new location,
fixes dependencies where bundled libraries depend on each other. If this option is not passed, no libraries will be prepared for distribution.
</blockquote>

`-i`, `--ignore` (path)
> Dylibs in (path) will be ignored. By default, dylibbundler will ignore libraries installed in `/usr/lib` since they are assumed to be present by default on all OS X installations.*(It is usually recommend not to install additional stuff in `/usr/`, always use ` /usr/local/` or another prefix to avoid confusion between system libs and libs you added yourself)*

`-d`, `--dest-dir` (directory)
> Sets the name of the directory in wich distribution-ready dylibs will be placed, relative to the current working directory. (Default is `./libs`) For an app bundle, it is often conveniant to set it to something like `./MyApp.app/Contents/libs`.

`-p`, `--install-path` (libraries install path)
> Sets the "inner" installation path of libraries, usually inside the bundle and relative to executable. (Default is `@executable_path/../libs/`, which points to a directory named `libs` inside the `Contents` directory of the bundle.)

`-s`, `--search-path` (search path)
> Check for libraries in the specified path

*The difference between `-d` and `-p` is that `-d` is the location dylibbundler will put files at, while `-p` is the location where the libraries will be expected to be found when you launch the app. Both are often related.*

`-of`, `--overwrite-files`
> When copying libraries to the output directory, allow overwriting files when one with the same name already exists.

`-od`, `--overwrite-dir`
> If the output directory already exists, completely erase its current content before adding anything to it. (This option implies --create-dir)

`-cd`, `--create-dir`
> If the output directory does not exist, create it.

A command may look like
`% dylibbundler -od -b -x ./HelloWorld.app/Contents/MacOS/helloworld -d ./HelloWorld.app/Contents/libs/`


If you want to create a universal binary by merging toghether two builds from PPC and Intel machines, you can ease it up by putting the ppc and intel libs in different directories, then to create the universal binary you only have to lipo the executable.
<code>
<pre>
% dylibbundler -od -b -x ./HelloWorld.app/Contents/MacOS/helloworld
    -d ./HelloWorld.app/Contents/libs-ppc/ -p @executable_path/../libs-ppc/

% dylibbundler -od -b -x ./HelloWorld.app/Contents/MacOS/helloworld
    -d ./HelloWorld.app/Contents/libs-intel/ -p @executable_path/../libs-intel/
  </pre>
</code>
