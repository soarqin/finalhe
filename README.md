# Final h-encore

Push the h-encore exploit for PS VITA andPS TV through a Windows, macOS or Linux GUI.

## Credits

see [CREDITS.md](CREDITS.md)

## Common usage

1. Download a pre-built executable binary below.
1. If you want to update PS Vita to firmware 3.60, 3.65 or 3.68 through USB connection, download related Update Packages [here](https://darthsternie.net/index.php/ps-vita-firmwares/) and extract the `PSP2UPDAT.PUP` to the same folder as this tool
1. If you want to install VitaShell or enso by transferring in Content Manager, download prebuilt zip from `releases` tab and put in the same folder of this tool
1. Connect your PS Vita to your computer via USB
1. Run the executable and follow the on-screen directions

### Prebuilt binaries

Download a pre-built executable binary below and follow instructions. Supported firmwares: 3.60, 3.61, 3.65-3.72

- For Windows get the [latest release](https://github.com/soarqin/finalhe/releases/latest)
  - if you have not installed USB driver for PS Vita: install `QcmaDriver_winusb.exe` (also on the releases page) 
- For macOS, the last pre-built [release is v1.5](https://github.com/soarqin/finalhe/releases/tag/v1.5)
- For linux openSUSE
  1. add a home [repository](https://software.opensuse.org/package/finalhe) to your local software repositories:
      - `sudo addrepo -f http://download.opensuse.org/repositories/home:/seilerphilipp/openSUSE_Leap_15.0/`
      - note: if using Leap 42.3, replace the version in the url with `42.3`
  2. install package
      - `sudo zypper install finalhe`
  3. Run "FinalHE"
      - in your terminal type `FinalHE`

## Build from source

### Prerequisites 

1. macOS: install [brew](https://brew.sh), install libusb & qt through brew (`brew install libusb qt`)
2. Linux:
   - Debian/Ubuntu: install build-essential, libxml2-dev, libusb-dev, zlib-dev or zlib1g-dev, qtbase5-dev, qttools5, cmake(if use cmake to build)
   - Fedora/CentOS: group install "Development Tools", install libxml2-devel, libusb-devel, zlib-devel, qt5-qtbase-devel, qt5-qtbase, cmake3(if use cmake to build)
   - openSUSE: install cmake >= 11.0, gcc-c++, zlib-devel, libxml2-devel, libQt5Widgets-devel, libQt5Network-devel, libqt5-linguist-devel, libusb-compat-devel
   - Arch: install base-devel, libxml2, libusb, zlib, qt5, cmake (if using cmake to build)

### Build from source

You can choose either `qmake` or `cmake` to build

- cmake: run `cmake` to generate Makefile for compiling
  - macOS: it cannot produce app bundle, and you need to specify `CMAKE_PREFIX_PATH` if Qt is not installed in default location: `cmake -DCMAKE_PREFIX_PATH=<Path of Qt Root> <Path of Project Root>`
- qmake: run `qmake` to generate Makefile for compiling, run `make lcopy` in `src` folder to compile translations and copy them to binary folder

## Contribute translations

- For coders using Qt:
   1. If using qmake, add your language to this line in src/src.pro: `TRANSLATIONS += ...`, and re-generate Makefile from qmake, then run `make lupdate` in `src` folder, you will get new generated .ts files in src/languages
   1. If  using cmake, add your language to this line in src/CMakeLists.txt: `set(TRANSLATION_FILES ...`, and re-generate Makefile/project from cmake, compile it, you will get new generated .ts files in src/languages
   1. Open .ts files with Qt Linquist tool and translate strings into native language
   1. If using qmake, run `make lcopy` in src folder and you will get compiled .qm files in languages folder
   1. If using cmake, compile the project and you will get compiled .qm files in language folder
- For non-coders:
   1. Copy translations/en_US.ts to a new file with filename in [IETF language tag](https://datahub.io/core/language-codes/r/3.html) form (but replace "-" with "_")
   1. Open the .ts file, it is just in xml format, go through all elements of `<translation type="unfinished">` to translate, and remove the property `type="unfinished"` from translated items
