#!/usr/bin/env python3
"""Validate Minecraft's own fragment shaders without redistributing game assets.

Usage: python3 minecraft_shaders.py /path/to/26.3.jar /path/to/shader_translation_test
"""
import pathlib
import re
import subprocess
import sys
import tempfile
import zipfile


def main():
    with zipfile.ZipFile(sys.argv[1]) as jar, tempfile.TemporaryDirectory() as temp:
        def expand(name):
            return re.sub(
                r"#include\s+<minecraft:([^>]+)>",
                lambda match: expand("assets/minecraft/shaders/include/" + match[1]),
                jar.read(name).decode(),
            )

        constants = ("#define OIT\n#define OIT_COEFF_COUNT 8\n"
                     "#define OIT_COEFF_ATTACHMENT_COUNT 2\n#define OIT_WAVELET_RANK 2\n")
        variants = {
            "solid": "",
            "transmittance": constants + "#define OIT_ALPHA_ONLY\n#define OIT_TRANSMITTANCE\n",
            "accumulate": constants + "#define OIT_ACCUMULATE\n",
            "depth_bounds": constants + "#define OIT_ALPHA_ONLY\n#define OIT_DEPTH_BOUNDS\n",
        }
        paths = []
        for name in jar.namelist():
            if not name.startswith("assets/minecraft/shaders/core/") or not name.endswith(".fsh"):
                continue
            source = expand(name)
            modes = variants if "executeAlphaOnlyPhase" in source else {"solid": ""}
            if pathlib.Path(name).stem.startswith("oit_"):
                modes = {"composite": constants}
            if pathlib.Path(name).stem == "rendertype_end_portal":
                modes = {"solid": "#define PORTAL_LAYERS 16\n"}
            if pathlib.Path(name).stem == "terrain":
                modes = dict(modes)
                modes.update({key + "_multidraw": value + "#define MULTIDRAW_TERRAIN\n"
                              for key, value in variants.items()})
                modes["cutout"] = "#define ALPHA_CUTOUT 0.1\n"
            for mode, defines in modes.items():
                path = pathlib.Path(temp, pathlib.Path(name).stem + "-" + mode + ".fsh")
                path.write_text(re.sub(r"(#version[^\n]*\n)",
                                       lambda match: match[0] + defines, source, count=1))
                paths.append(str(path))
        print(f"Validating {len(paths)} Minecraft fragment shader variants", flush=True)
        subprocess.run([str(pathlib.Path(sys.argv[2]).resolve()), *paths], check=True)


if __name__ == "__main__":
    main()
