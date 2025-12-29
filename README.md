# Build instructions
1. Install C compiler of your choice (GCC, Clang, MinGW, etc)
2. Setup build directory:
```sh
mkdir build
meson setup build
```
3. Build with meson:
```sh
meson compile -C build
```
4. Execute:
```sh
./build/main
```

# Cross-compilation
Linux-to-windows compilation can be done with provided cross-compilation file.

Setup build directory with:
```sh
meson setup --cross-file cross-files/x86_64-w64-mingw32.txt build-mingw
```
Then build as usual:
```sh
meson compile -C build-mingw
```

For windows-to-Linux compilation just compile project under WSL.
