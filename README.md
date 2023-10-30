# kbinsert: Insert a string into the Linux kernel input buffer

## Usage: In current console terminal only:

```
$ kbinsert "Hello world" # Will 'type' "Hello world"
$ kbinsert Hello world   # Multiple arguments are added with spaces inbetween
```

### Usage: X11 version (inserts anywhere you are!)

```
$ sleep 5; kbinsertx -g "Hello world"
```

## Notes:
    1. kbinsert is compiled with plain terminal support
    1. kbinsertx is compiled with X11 support (and needs -g to use it)

## Compiling
    1. Clone
    2. type `make`

