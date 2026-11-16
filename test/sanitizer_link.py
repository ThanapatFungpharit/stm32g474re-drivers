# Link the native unit-test binaries with the sanitizer runtimes too.
Import("env")  # noqa: F821  (provided by PlatformIO/SCons)
env.Append(LINKFLAGS=["-fsanitize=address,undefined"])  # noqa: F821
