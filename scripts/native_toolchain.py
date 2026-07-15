import os

Import("env")

toolchain_dir = env.PioPlatform().get_package_dir("toolchain-gccmingw32")
if toolchain_dir:
    env.PrependENVPath("PATH", os.path.join(toolchain_dir, "bin"))
