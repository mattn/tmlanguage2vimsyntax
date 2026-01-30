# TextMate to Vim Syntax Converter

Converts TextMate syntax definitions (.tmLanguage.json) to Vim syntax files (.vim).

## Usage

```bash
./tmlanguage2vimsyntax input.tmLanguage.json output.vim
```

## Example

```bash
./tmlanguage2vimsyntax Go.tmLanguage.json Go.vim
```

## Build

```bash
mkdir build && cd build
cmake ..
make
```

## License

MIT License

## Author

Yasuhiro Matsumoto (a.k.a. mattn)