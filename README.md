# libiosexec

A shim library that both works to allow shell scripts to execute correctly on iOS, and provides a framework for true rootless support.

In Remorix, libiosexec is expected to eventually be absorbed by [librecompat](https://github.com/Torrekie/librecompat). librecompat already acts as the re-export layer that exposes libSystem, libiosexec, and additional libc-compatibility entry points together.

Remorix-specific behavior differences are documented in [REMORIX_POLICY.md](REMORIX_POLICY.md).

## Usage
There's not that much to say here; simply define the symbols in your project/script
    
    /* Wrapper functions to make iOS shells scripts function correctly */
    #include <libiosexec.h>
