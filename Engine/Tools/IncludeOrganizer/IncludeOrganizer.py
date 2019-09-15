# -*- encoding: utf-8 -*-


import os
import sys


def collect_files_in_dir(base_dir, excludes):
    source_files = set()
    for root, _, files in os.walk(base_dir):
        if root in excludes:
            continue
        for f in files:
            source_files.add(os.path.join(root, f))
    return source_files


def is_include_line(line):
    return line.startswith("#include ")


class SectionInfo(object):
    def __init__(self, s, is_include):
        self.section_start = s
        self.section_end = s + 1
        self.is_include = is_include


def split_into_sections(lines):
    sections = [SectionInfo(0, is_include_line(lines[0]))]
    for line_index, line in enumerate(lines):
        flag = is_include_line(line)
        if flag == sections[-1].is_include:
            sections[-1].section_end = line_index + 1
        else:
            sections.append(SectionInfo(line_index, flag))
    return sections


def extract_include_path(line):
    # should starts with #include ., but may end with comments
    #.#include <".
    i = 10
    while i != len(line) - 2:
        if line[i] != '"' and line[i] != '>':
            i += 1
        else:
            break
    return line[10:i], line[i] == '>'


class LineStruct(object):
    THIS_PATH = -1
    SYSTEM_PATH = 0
    USER_PATH = 1
    system_weights = {
        "Public": -1,
        "Core": 0,
        "Misc": 1,
        "Platform": 2,
        "Graphics": 3,
    }

    def __init__(self, line, is_this_path):
        self.line = line
        if is_this_path:
            self.type = self.THIS_PATH
            return
        raw_path, flag = extract_include_path(line)
        self.type = self.SYSTEM_PATH if flag else self.USER_PATH
        if self.type == self.SYSTEM_PATH:
            return
        raw_path_parts = raw_path.split('/', 5)  # Runtime/Public/MainModule/SubModule/TailPath
        if raw_path_parts[0] != 'Runtime':
            raise RuntimeWarning("include path in Runtime should starts with Runtime")
        self.module = []
        for i in xrange(1, min(len(raw_path_parts) - 2, 5)):
            LineStruct.system_weights.get(raw_path_parts[i], 10000)
            self.module.append(raw_path_parts)
        self.tail_path = raw_path_parts[-1]


def fix_includes(lines, this_path):
    def builder(line):
        return LineStruct(line, this_path == line)
    infos = map(builder, lines)

    def key_compare(l, r):
        d = l.type - r.type
        if d != 0:
            return d
        if l.type == 0:
            return cmp(l.line, r.line)
        d = cmp(l.module, r.module)
        if d != 0:
            return d
        return cmp(l.tail_path, r.tail_path)
    infos.sort(cmp=key_compare)
    return map(lambda x: x.line, infos)


def simple_line_fix(line):
    if line.startswith('#include "Windows.h"'):
        return '#include <Windows.h>' + os.linesep
    elif line.startswith('#include "windowsx.h"'):
        return '#include <windowsx.h>' + os.linesep
    elif line.startswith('#include "d3dx12.h'):
        return '#include "Runtime/Platform/DX12/d3dx12.h"' + os.linesep
    elif line.startswith('#include "d3d12.h'):
        return '#include <d3d12.h>' + os.linesep
    elif line.startswith('#include "Yes.h"'):
        return '#include "Runtime/Public/Yes.h"'
    return line


def runtime_dir_fix(source_files, base_dir):
    for p in source_files:
        this_path = os.path.relpath(p, base_dir)
        with open(p, "rb") as rf:
            lines = rf.readlines()
        if len(lines) == 0:
            continue
        for i, l in enumerate(lines):
            lines[i] = simple_line_fix(l)
        pth, ext = os.path.splitext(this_path)
        if ext in {'cpp', 'c'}:
            this_include = pth + '.h' + os.linesep
        else:
            this_include = ""
        with open(p, "wb") as wf:
            sections = split_into_sections(lines)
            for s in sections:
                if s.is_include:
                    fixed_lines = fix_includes(lines[s.section_start:s.section_end], this_include)
                    wf.writelines(fixed_lines)
                else:
                    wf.writelines(lines[s.section_start:s.section_end])


def main():
    base_dir = r"C:\checkout\Yes\Engine\Sources\Runtime"
    collected = collect_files_in_dir(
        base_dir,
        r"C:\checkout\Yes\Engine\Sources\Runtime\Todo")
    runtime_dir_fix(collected, base_dir)


if __name__ == "__main__":
    main()

