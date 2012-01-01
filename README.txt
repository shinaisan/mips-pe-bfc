Brainf*ck compiler for MIPS (32-bit, little-endian).
It generates a PE file that runs on Windows NT 4.0 for MIPS.
Sample compilation (using Visual C++) to generate hello-world.exe from hello-world.bf:
> cl bfc.c
> bfc hello-world.bf
