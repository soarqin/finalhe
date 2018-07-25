# Final h-encore
a tool to push h-encore exploit for PS VITA/PS TV automatically

# CREDITS
see CREDITS.md

# Build
You can choose either qmake or cmake to build
1. cmake: just run cmake to generate Makefile for compiling, for OSX/macOS it cannot produce app bundle, and you need to specify CMAKE_PREFIX_PATH if Qt is not installed in default location:
```
cmake -DCMAKE_PREFIX_PATH=<Path of Qt Root> <Path of Project Root>
```
2. qmake: just run qmake to generate Makefile for compiling, run ```make lcopy``` in ```src``` folder to compile translations and copy them to binary folder.
