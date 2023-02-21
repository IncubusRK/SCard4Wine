SCard4Wine
==========

Fork of [SCard4Wine project on SourceForge](http://sourceforge.net/projects/scard4wine/) that brings full Smart Card support to the WINE emulator.
  
This project contains only source files.
You will need to download complete Wine sources and copy winscard files into Wine sources and build the whole Wine.
Here are steps:

1. Download and extract the Wine source tarball. Or clone from git. For example:
```
git clone -b 8.1 --depth 1 --progress https://github.com/wine-mirror/wine ./wine
```	
2. Copy winscard source files.
```
cp -f scard4wine/src/*.{c,spec,in} wine/dlls/winscard/
cp -f scard4wine/src/unixlib.h wine/dlls/winscard/
cp -f scard4wine/src/winscard.h wine/include/
cp -f scard4wine/src/winsmcrd.h wine/include/
```
3. Build the whole Wine
```
./configure && make -j $(nproc) && make install -j $(nproc)
```	
