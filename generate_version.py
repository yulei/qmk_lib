#!/usr/bin/env python3
import sys
import random
import time
import subprocess
import os

TIME_FMT = '%Y-%m-%d-%H:%M:%S'

def main():
    if len(sys.argv) != 3:
        print("Usage: generate_version.py path_to_reposity path_to_version.h")
        return 1


    with open(sys.argv[2], "w") as f:
        os.chdir(sys.argv[1])
        result = subprocess.run(["git", "rev-parse", "--short", "head"], capture_output=True, text=True)
        current_version = result.stdout
        current_time = time.strftime(TIME_FMT)
        current_build_id = random.randrange(0, 2 ** 24 - 1)
        f.write("#pragma once\n\n")
        f.write("#define QMK_VERSION {}".format(current_version))
        f.write("#define QMK_BUILDDATE {}\n".format(current_time))
        f.write("#define BUILD_ID ((uint32_t)0x{:08X})\n\n".format(current_build_id))

    return 0

if __name__ == "__main__":
    sys.exit(main())
