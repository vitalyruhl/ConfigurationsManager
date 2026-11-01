"""Keep link-time size optimization enabled after the Arduino build setup."""
Import("env")

env.Replace(LINKFLAGS=[flag for flag in env.get("LINKFLAGS", []) if flag != "-fno-lto"])
env.Append(LINKFLAGS=["-flto"])
