
I'm going to need to use the posix module to implement `sh` in a "true" way.
However, the docs were a bit... confusing.

The main issues is that `posix.popen` (which executes a command on the shell)
doesn't accept the stdin/stdout/stderr as arugments. It _returns_ a who's
mode depends on whether the second argument is 'r' or 'w'. But assuming you
use 'r'... how do you pipe things to the process?

[This answer](https://stackoverflow.com/a/280587/1036670) helped clear up the
issue for me. It turns out that `popen` will _use the current stdout/stderr
file descriptors of the process_.

It turns out that popen is not a system call, it uses _fork_, which has this
documentation: https://man7.org/linux/man-pages/man2/fork.2.html

```
  The child inherits copies of the parent's set of open file
  descriptors.  Each file descriptor in the child refers to the
  same open file description (see open(2)) as the corresponding
  file descriptor in the parent.  This means that the two file
  descriptors share open file status flags, file offset, and
  signal-driven I/O attributes (see the description of F_SETOWN
  and F_SETSIG in fcntl(2)).
```


Putting this together, this post makes more sense

https://stackoverflow.com/a/16515126/1036670

Basically we need to do `local r, w = posix.pipe()` for each read or write pipe
we need and close the other one.

