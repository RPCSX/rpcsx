#!/bin/sh -ex

# First let's see print some info about our caches
"$(cygpath -u "$CCACHE_BIN_DIR")"/ccache.exe --show-stats -v

# Remove unecessary files
rm -f ./rpcs3/bin/rpcs3.exp ./rpcs3/bin/rpcs3.lib ./rpcs3/bin/rpcs3.pdb ./rpcs3/bin/vc_redist.x64.exe
rm -rf ./rpcs3/bin/git

# Prepare compatibility and SDL database for packaging
mkdir -p ./rpcs3/bin/config/input_configs
curl -fsSL 'https://raw.githubusercontent.com/gabomdq/SDL_GameControllerDB/master/gamecontrollerdb.txt' 1> ./rpcs3/bin/config/input_configs/gamecontrollerdb.txt
cp -rf ./build-msvc/bin/ ./rpcs3/bin/

# Package artifacts
7z a -m0=LZMA2 -mx9 "$BUILD" ./rpcs3/bin/*

# Move files to publishing directory
cp -- "$BUILD" ./RPCS3-Qt-UI.7z
