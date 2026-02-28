#!/bin/zsh
alias peko=/Users/preston/Work/peko/compiler/peko_cli/target/debug/peko_cli

clang $(peko clangflags --nostd --os=android --arch=arm) -DANDROID -Irawdraw -Iandroid -Icnfa android_webview.c                 -o $(pwd)/../objects/android/android_webview.o; \
clang $(peko clangflags --nostd --os=android --arch=arm) -DANDROID -Irawdraw -Iandroid -Icnfa android/android_native_app_glue.c -o $(pwd)/../objects/android/native_glue.o; \
clang $(peko clangflags --nostd --os=android --arch=arm) -DANDROID -Irawdraw -Iandroid -Icnfa android/android_usb_devices.c     -o $(pwd)/../objects/android/android_usb_devices.o; \