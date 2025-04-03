#!/bin/sh -ex

# First let's see print some info about our caches
"$(cygpath -u "$CCACHE_BIN_DIR")"/ccache.exe --show-stats -v

# BUILD_blablabla is Azure specific, so we wrap it for portability
ARTIFACT_DIR="$BUILD_ARTIFACTSTAGINGDIRECTORY"

# Remove unecessary files
rm -f ./rpcs3/bin/rpcs3.exp ./rpcs3/bin/rpcs3.lib ./rpcs3/bin/rpcs3.pdb ./rpcs3/bin/vc_redist.x64.exe
rm -rf ./rpcs3/bin/git

# Prepare compatibility and SDL database for packaging
mkdir ./rpcs3/bin/config
mkdir ./rpcs3/bin/config/input_configs
curl -fsSL 'https://raw.githubusercontent.com/gabomdq/SDL_GameControllerDB/master/gamecontrollerdb.txt' 1> ./rpcs3/bin/config/input_configs/gamecontrollerdb.txt
curl -fsSL 'https://rpcs3.net/compatibility?api=v1&export' | iconv -t UTF-8 1> ./rpcs3/bin/GuiConfigs/compat_database.dat

# Download SSL certificate (not needed with CURLSSLOPT_NATIVE_CA)
#curl -fsSL 'https://curl.haxx.se/ca/cacert.pem' 1> ./bin/cacert.pem

# Package artifacts
7z a -m0=LZMA2 -mx9 "$BUILD" ./rpcs3/bin/*

# Generate sha256 hashes
# Write to file for GitHub releases
sha256sum "$BUILD" | awk '{ print $1 }' | tee "$BUILD.sha256"
echo "$(cat "$BUILD.sha256");$(stat -c %s "$BUILD")B" > GitHubReleaseMessage.txt

# Move files to publishing directory
cp -- "$BUILD" "$ARTIFACT_DIR"
cp -- "$BUILD.sha256" "$ARTIFACT_DIR"
