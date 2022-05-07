#!/bin/bash

if ! command -v brew &> /dev/null
then
    read -p "Brew is not installed. Would you like to install it (y/n)?" answer
    case ${answer:0:1} in
        y|Y )
            /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
        ;;
        * )
            echo "Brew must be installed to continue. Exiting."
            exit 0
        ;;
    esac
fi

if brew list cmake &>/dev/null; then
    echo "cmake is installed"
else
    echo "Installing cmake..."
    brew install cmake
fi

if brew list libusb &>/dev/null; then
    echo "libusb is installed"
else
    echo "Installing libusb..."
    brew install libusb
fi

if brew list pkg-config &>/dev/null; then
    echo "pkg-config is installed"
else
    echo "Installing pkg-config..."
    brew install pkg-config
fi

if brew list qt5 &>/dev/null; then
    echo "qt5 is installed"
else
    echo "Installing qt5..."
    brew install qt5
fi

echo "Checking for qt5 in path..."
if echo $PATH | grep -F "qt@5" &>/dev/null; then
    echo "qt5 is in the PATH"
else
    echo "Adding qt to path"
    echo 'export PATH="/opt/homebrew/opt/qt@5/bin:$PATH"' >> ~/.profile
    source ~/.profile
fi

echo "All dependencies have been installed."
echo "Building FinalHE..."
cmake .
make

APPDIR="$(pwd)/src/FinalHE.app"
if [ -d "$APPDIR" ]; then
    rm -rf "$APPDIR"
fi

# create macOS standalone application
mkdir "$APPDIR"
mkdir "$APPDIR/Contents"
cp "$(pwd)/src/FinalHE" "$APPDIR/Contents/FinalHE"

mkdir "$APPDIR/Contents/Resources"
touch "$APPDIR/Contents/Info.plist"
echo '<?xml version="1.0" encoding="UTF-8"?>
     <plist version="1.0">
     <dict>
        <key>CFBundleExecutable</key>
	    <string>FinalHE</string>
        <key>CFBundleIconFile</key>
        <string>finalhe</string>
        <key>CFBundleShortVersionString</key>
        <string>1.92</string>
     </dict>
</plist>
' > "$APPDIR/Contents/Info.plist"
cp "$(pwd)/src/finalhe.icns" "$APPDIR/Contents/Resources/finalhe.icns"

echo "Done."

read -p "Would you like to run FinalHE (y/n)? " answer
case ${answer:0:1} in
    y|Y )
         open $(pwd)/src/FinalHE.app
    ;;
    * )
        exit 0
    ;;
esac

exit 0
