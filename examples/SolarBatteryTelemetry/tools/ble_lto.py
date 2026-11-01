"""Consumer-only optimization proven with the selected ESP32/NimBLE stack."""
Import("env")
env.Replace(LINKFLAGS=[flag for flag in env.get("LINKFLAGS",[]) if flag!="-fno-lto"])
env.Append(LINKFLAGS=["-flto"])
