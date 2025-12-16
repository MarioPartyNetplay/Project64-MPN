Crypto++ Library Setup Instructions
===================================

This directory contains Crypto++ as a git submodule pointing to:
  https://github.com/weidai11/cryptopp.git

The submodule is located at: Source/3rdParty/cryptopp/cryptopp/

To initialize the submodule:
  git submodule update --init --recursive Source/3rdParty/cryptopp/cryptopp

After cloning/updating the submodule, run update_project.bat to automatically
update cryptopp.vcxproj with all source files from the nested submodule.

The cryptopp.vcxproj file in this directory is a wrapper that builds the Crypto++
library from the submodule source files located in the cryptopp/ subdirectory.
