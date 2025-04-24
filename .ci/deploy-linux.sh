#!/bin/sh -ex

cd build || exit 1

CPU_ARCH="${1:-x86_64}"

if [ "$DEPLOY_APPIMAGE" = "true" ]; then
    DESTDIR=AppDir ninja install

    sudo curl -fsSLo /usr/bin/linuxdeploy "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-$CPU_ARCH.AppImage"
    sudo chmod a+x /usr/bin/linuxdeploy
    sudo curl -fsSLo /usr/bin/linuxdeploy-plugin-qt "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-$CPU_ARCH.AppImage"
    sudo chmod a+x /usr/bin/linuxdeploy-plugin-qt
    curl -fsSLo linuxdeploy-plugin-checkrt.sh https://github.com/darealshinji/linuxdeploy-plugin-checkrt/releases/download/continuous/linuxdeploy-plugin-checkrt.sh
    chmod +x ./linuxdeploy-plugin-checkrt.sh

    export EXTRA_PLATFORM_PLUGINS="libqwayland-egl.so;libqwayland-generic.so"
    export EXTRA_QT_PLUGINS="svg;wayland-decoration-client;wayland-graphics-integration-client;wayland-shell-integration;waylandcompositor"

    APPIMAGE_EXTRACT_AND_RUN=1 linuxdeploy --appdir AppDir --plugin qt --plugin checkrt

    # Remove libwayland-client because it has platform-dependent exports and breaks other OSes
    rm -f ./AppDir/usr/lib/libwayland-client.so*

    # Remove libvulkan because it causes issues with gamescope
    rm -f ./AppDir/usr/lib/libvulkan.so*

    # Remove git directory containing local commit history file
    rm -rf ./AppDir/usr/share/rpcs3/git

    linuxdeploy --appimage-extract
    ./squashfs-root/plugins/linuxdeploy-plugin-appimage/usr/bin/appimagetool AppDir -g

    mv ./*.AppImage ../RPCS3-Qt-UI.AppImage
fi
