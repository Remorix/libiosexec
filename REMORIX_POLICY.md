# Remorix Fork Policy

This fork intentionally differs from Procursus libiosexec in a few places where
Remorix needs closer iOS behavior or a safer rootless package policy.

## Shebang Tokenization

libiosexec emulates the kernel shebang retry in userspace because iOS rejects
script files before normal shebang handling can run.

Remorix follows XNU tokenization for the shebang tail. This means arguments
after the interpreter path are split the same way XNU would split them, instead
of preserving the whole tail as one argument.

```text
script:
  #!/usr/bin/env bash -e

initial request:
  execve("./script", ["./script", "arg1"])

Procursus-style:
  /usr/bin/env
      |
      +-- argv[1] = "bash -e"
      +-- argv[2] = "./script"
      +-- argv[3] = "arg1"

Remorix / XNU-style:
  /usr/bin/env
      |
      +-- argv[1] = "bash"
      +-- argv[2] = "-e"
      +-- argv[3] = "./script"
      +-- argv[4] = "arg1"
```

## Rootless Search Path Order

For rootless builds, Remorix prefers the redirected root first when generating
`_PATH_DEFPATH` and `_PATH_STDPATH`.

Procursus currently keeps the original path before the redirected path. That
can select host commands before rootless commands, which is not the policy this
fork wants for `/var/jb` environments.

```text
base path:
  /usr/bin:/bin

Procursus rootless order:
  /usr/bin
    -> /var/jb/usr/bin
    -> /bin
    -> /var/jb/bin

Remorix rootless order:
  /var/jb/usr/bin
    -> /usr/bin
    -> /var/jb/bin
    -> /bin
```

This keeps host paths available as fallback entries, but rootless-provided
tools win when both locations exist.

## Signed UID And GID Text Records

Remorix accepts signed numeric IDs in text passwd and group files. Darwin's
text account data intentionally uses entries such as `nobody` with ID `-2` and
`nogroup` with ID `-1`.

Apple's Libinfo file backend parses those fields with signed conversion and
then stores the result in `uid_t` or `gid_t`. Remorix follows that behavior:
the text value `-2` is accepted and then compares equal to `(uid_t)-2` or
`(gid_t)-2`.

```text
/etc/group:
  nogroup:*:-1:
  nobody:*:-2:

lookup:
  getgrgid((gid_t)-1) -> "nogroup"
  getgrgid((gid_t)-2) -> "nobody"
```

## Usershell Ownership

Remorix treats `getusershell`, `setusershell`, and `endusershell` as
librecompat-owned compatibility routines. libiosexec resolves the
`compat_*usershell` entry points from librecompat at runtime instead of carrying
an independent implementation in the rootless build.

```text
caller
  |
  +-- ie_getusershell()
        |
        +-- dlopen("@rpath/librecompat.0.dylib")
        |
        +-- dlsym("compat_getusershell")
              |
              +-- librecompat owns /etc/shells semantics
```

This matches the Remorix direction where librecompat is the long-term re-export
layer for libSystem, libiosexec, and libc compatibility functions.
