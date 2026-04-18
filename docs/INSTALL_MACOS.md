# Install On macOS

## Requirements

- macOS with Xcode Command Line Tools
- Python 3
- Homebrew
- `clang` and `clang++`

## Build ns-3

```bash
cd ns-allinone-3.29/ns-3.29
./waf configure --disable-werror
./waf build
```

`--disable-werror` is recommended on modern macOS toolchains because ns-3.29 and related code paths emit legacy warnings that would otherwise stop the build.

## Build NetAnim

Install Qt 5:

```bash
brew install qt@5
```

Then build NetAnim:

```bash
cd ns-allinone-3.29/netanim-3.108
/opt/homebrew/opt/qt@5/bin/qmake NetAnim.pro
make -j4
```

## Notes

- The repository includes small compatibility adjustments for recent Python and macOS toolchains.
- Optional ns-3 features such as PyViz, SQLite stats, and OpenFlow are not required for the ASTRO-FANET scenario.
