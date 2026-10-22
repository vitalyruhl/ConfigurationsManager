# Consumer-only size optimization; never changes installed framework/toolchain files.
Import("env")
env.Replace(LINKFLAGS=[flag for flag in env.get("LINKFLAGS", []) if flag != "-fno-lto"])
env.Append(LINKFLAGS=["-flto"])
