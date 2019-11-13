# -*- encoding: utf-8 -*-
import sys
import os


sys.path.append(os.path.abspath("Engine/Tools/BuildTool"))
pwd = os.path.dirname(os.path.abspath(__file__))
source_root = os.path.join(pwd, "Engine/Sources")
build_root = os.path.join(pwd, "Engine/Build")
if not os.path.exists(build_root):
    os.makedirs(build_root)


if __name__ == "__main__":
    import driver
    platform = "vs2019" if len(sys.argv) <= 1 else sys.argv[1]
    if platform == "vs2019":
        driver.generate_vs_project(pwd, source_root, build_root, os.path.join(pwd, "Yes.2019.sln"))

