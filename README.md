# Final h-encore
a tool to push h-encore exploit for PS VITA/PS TV automatically

# CREDITS
see [CREDITS.md](CREDITS.md)

# Build
You can choose either qmake or cmake to build
1. cmake: just run cmake to generate Makefile for compiling, for OSX/macOS it cannot produce app bundle, and you need to specify CMAKE_PREFIX_PATH if Qt is not installed in default location:
```
cmake -DCMAKE_PREFIX_PATH=<Path of Qt Root> <Path of Project Root>
```
2. qmake: just run qmake to generate Makefile for compiling, run ```make lcopy``` in ```src``` folder to compile translations and copy them to binary folder.

# Contribute to translations
1. If you are using qmake, add your language to this line in src/src.pro: ```TRANSLATIONS += ...```, and re-generate Makefile from qmake, the run ```make lupdate``` in src folder, you will get new generated .ts files in src/languages
   If you are using cmake, add your language to this line in src/CMakeLists.txt: ```set(TRANSLATION_FILES ...```, and re-generate Makefile/project from cmake, compile it, you will get new generated .ts files in src/languages
2. Open .ts files with Qt Linquist tool and translate strings into native language
3. If you are using qmake, run ```make lcopy``` in src folder and you will get compiled .qm files in languages folder
4. If you are using cmake, compile the project and you will get compiled .qm files in language folder
