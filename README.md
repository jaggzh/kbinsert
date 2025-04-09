# kbinsert: Insert a string into the keyboard buffer (the 'Linux kernel input buffer')

This inserts a string as if you typed it. This works in the current term and will not accidentally type into another terminal or window. (But if you mess something up don't blame me.)

(Multiple command-line args are joined with single spaces.)

## Why do this at all? WHY?

It lets you automate things. My simplest convenient use is that I have
aliases that go somewhere and type a command **but don't hit enter**. Example:
```bash
alias prj='cd /path/some-project && ls -lgGrt && kbins vi some-prj.c'
```
```bash
~$ prj<enter>
total 16
-rw-r--r-- 1  94 Apr  9 07:13 Makefile
-rw-r--r-- 1 897 Apr  9 07:13 README.md
-rw-r--r-- 1 110 Apr  9 07:13 some-prj
-rw-r--r-- 1 291 Apr  9 07:13 some-prj.c
/path/some-project$ vi some-prj.c  # The cursor is left here without the command run.
```

Then I hit ENTER to edit, or ^U to do some other management there.
Usually I type whatever the command was, but periodically it's nice not to, and to put it in one alias.

## Usage: In current console terminal only:

```
$ kbinsert "Hello world"
  *Will 'type' "Hello world"*
$ kbinsert Hello world   # Multiple arguments are added with spaces inbetween
```

### Usage: X11 version (inserts anywhere you are!)

```
$ sleep 5; kbinsertx -g "Hello world"
```

This is just an X11 version which will insert keys kind of like `xdotool key`.
I figured I'd add the support for it. HOWEVER, remember that `kbinsert` itself is not X11-based, and does not depend on X libraries, and will insert into the current term, so `sleep 5; kbinsert` will NOT accidentally type into a window or other term (but if you use `kbinsertx` it might).

## Notes:
    1. kbinsert is compiled with plain terminal support
    1. kbinsertx is compiled with X11 support (and needs -g to use it)

## Compiling
    1. Clone
    2. type `make`

