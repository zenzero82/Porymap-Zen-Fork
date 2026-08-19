# Building Porymap Studio

Porymap Studio remains a native Qt desktop application. The qmake project
requires:

- a C++17 compiler;
- Qt 5.14.2 or newer;
- Qt Core, GUI, and Widgets;
- optional Qt Declarative/QML, Charts, and Network modules for their
  corresponding plug-in, chart, and update-check features;
- qmake and a platform build tool such as GNU Make or MinGW Make.

The executable and application bundle retain the internal name `porymap` for
upstream compatibility.

## Replit

The repository includes a Replit/Nix-aware build helper because Qt modules and
OpenGL development files are provided through separate Nix outputs:

```bash
./scripts/replit-build.sh
./scripts/replit-run.sh
```

Do not replace the helper with a bare `qmake` call in the Replit workflow.

## Windows

Install the Qt development tools and a MinGW toolchain using the
[Qt online installer](https://www.qt.io/download-qt-installer). Select a
supported desktop Qt version and the Qt Charts component.

From a Qt-enabled terminal:

```powershell
git clone https://github.com/zenzero82/Porymap-Zen-Fork.git
cd Porymap-Zen-Fork
qmake -config release porymap.pro
mingw32-make -j4
.\release\porymap.exe
```

Qt Creator may also open `porymap.pro` directly. The repository's GitHub Actions
workflow performs the same dynamic Windows build and uploads the executable as a
test artifact; it does not publish a release.

## macOS

Install Xcode command-line tools and Qt with [Homebrew](https://brew.sh/):

```bash
xcode-select --install
brew update
brew install qt

git clone https://github.com/zenzero82/Porymap-Zen-Fork.git
cd Porymap-Zen-Fork
qmake -config release porymap.pro
make -j4
./porymap.app/Contents/MacOS/porymap
```

## Ubuntu

Package names vary by distribution release. For Qt 6 on Ubuntu:

```bash
sudo apt-get install build-essential qt6-base-dev qt6-declarative-dev libqt6charts6-dev

git clone https://github.com/zenzero82/Porymap-Zen-Fork.git
cd Porymap-Zen-Fork
qmake6 -config release porymap.pro
make -j4
./porymap
```

If `qmake6` is exposed as `qmake`, use that command instead.

## Arch Linux

```bash
sudo pacman -S base-devel qt6-base qt6-declarative qt6-charts

git clone https://github.com/zenzero82/Porymap-Zen-Fork.git
cd Porymap-Zen-Fork
qmake6 -config release porymap.pro
make -j4
./porymap
```

## Baseline verification

A successful foundation check consists of:

1. qmake completing with Qt Core, GUI, and Widgets available (warnings about
   optional modules mean only their corresponding features are disabled);
2. the native compiler and linker completing successfully;
3. the application starting and displaying **Porymap Studio**;
4. existing project formats remaining unchanged.

Meaningful editor testing requires a compatible Pokémon decompilation project.
