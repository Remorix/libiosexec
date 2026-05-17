# libiosexec

A shim library that both works to allow shell scripts to execute correctly on iOS, and provides a framework for true rootless support.

In Remorix, libiosexec is expected to eventually be absorbed by [librecompat](https://github.com/Torrekie/librecompat). librecompat already acts as the re-export layer that exposes libSystem, libiosexec, and additional libc-compatibility entry points together.

## Usage
There's not that much to say here; simply define the symbols in your project/script
    
    /* Wrapper functions to make iOS shells scripts function correctly */
    #include <libiosexec.h>

<sub>Remorix policy: unlike Procursus libiosexec, this fork follows XNU shebang tokenization semantics on iOS when emulating kernel shebang execution in userspace.</sub>

```text
for a script with shebang:
  #!/usr/bin/env bash -e

original exec call:
  execve("./script", ["./script", "arg1"])

Procursus-style argv:
  ["/usr/bin/env", "bash -e", "./script", "arg1"]

Remorix / XNU-style argv:
  ["/usr/bin/env", "bash", "-e", "./script", "arg1"]
```
