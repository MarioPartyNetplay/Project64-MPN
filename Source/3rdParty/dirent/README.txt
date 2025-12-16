dirent Library Setup Instructions
==================================

This directory contains dirent as a git submodule pointing to:
  https://github.com/tronkko/dirent.git

To initialize the submodule:
  git submodule update --init --recursive Source/3rdParty/dirent

After initialization, the dirent.h header file will be available at:
  Source/3rdParty/dirent/include/dirent.h

The include paths in NetplayInputPlugin.vcxproj are already configured to find
dirent.h from this location. Use #include <dirent.h> in your code.
