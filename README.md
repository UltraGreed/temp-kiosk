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
