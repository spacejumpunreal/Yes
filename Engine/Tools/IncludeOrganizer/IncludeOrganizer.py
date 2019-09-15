# -*- encoding: utf-8 -*-


import os
import sys


def collect_files_in_dir(base_dir):
    source_files = set()
    for root, _, files in os.walk(base_dir):
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


class LineStruct(object):
    SYSTEM_PATH = 0
    USER_PATH = 1
    system_weights = {
        "Public": -1,
        "Core": 0,
        "Misc": 1,
        "Platform": 2,
        "Graphics": 3,
    }

    def __init__(self, line):
        self.line = line
        raw_path = line[8:].strip()
        if raw_path.startswith("<"):
            self.type = self.SYSTEM_PATH
            return
        self.type = self.USER_PATH
        raw_path_parts = raw_path.split('/', 5)  # Runtime/Public/MainModule/SubModule/TailPath
        if raw_path_parts[0] != 'Runtime':
            raise RuntimeWarning("include path in Runtime should starts with Runtime")
        self.module = []
        for i in xrange(1, min(len(raw_path_parts) - 2, 5)):
            LineStruct.system_weights.get(raw_path_parts[i], 10000)
            self.module.append(raw_path_parts)
        self.tail_path = raw_path_parts[-1]


def fix_includes(lines):
    infos = map(LineStruct, lines)

    def key_compare(l, r):
        d = l.type - r.type
        if d != 0:
            return d
        d = cmp(l.module, r.module)
        if d != 0:
            return d
        return cmp(l.tail_path, r.tail_path)
    infos.sort(cmp=key_compare)
    return map(lambda x: x.line, infos)


def runtime_dir_fix(source_files):
    for p in source_files:
        with open(p, "wb") as wf:
            lines = p.readlines()
            sections = split_into_sections(lines)
            for s in sections:
                if s.is_include:
                    fixed_lines = fix_includes(lines[s.section_start, s.section_end])
                    wf.writelines(fixed_lines)
                else:
                    wf.writelines(lines[s.section_start, s.section_end])


if __name__ == "__main__":
    collected = collect_files_in_dir(r"C:\checkout\Yes\Engine\Sources\Runtime")
    runtime_dir_fix(collected)
