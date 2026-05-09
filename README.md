# h2g - Hubbard to GoatTracker Converter

Converts SID files containing Hubbard player music to GoatTracker `.sng` format.

## Usage

```
h2g <input.sid> [output.sng]
```

If no output path is given, the input filename is used with a `.sng` extension.

## Build

Requires a C99 compiler. No external dependencies.

```
cc -std=c99 -o h2g h2g.c
```

## Credits

- Original VB6 version by Stilianos (Stello) Doussis (August 2005)
- ANSI C conversion by Stefan A. Haubenthal
- Licensed under GPL-2.0
