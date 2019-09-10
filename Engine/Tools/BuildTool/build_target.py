# -*- encoding: utf-8 -*-
import os
import re
import uuid


SOURCE_SUFFIXES = re.compile(r".*\.cpp"),
HEADER_SUFFIXES = re.compile(r".*\.h"),


def is_parent_of(directory, path):
    return os.path.join(path, "").startswith(os.path.join(directory, ""))


class BuildTarget(object):
    def __init__(self, base_dir):
        self.is_library = True
        self.export_base_dir = False
        self.include_source_dir = True
        self.base_dir = base_dir
        self.dependencies = []
        self.exported_dirs = []
        self.excluded_paths = []
        self.guid = uuid.uuid4()

    def collect_source_files(self):
        headers = []
        sources = []
        excluded_paths = [os.path.join(self.base_dir, p) for p in self.excluded_paths]
        for root, _, files in os.walk(self.base_dir):
            for fn in files:
                ok = False
                rp = os.path.join(root, fn)
                # check if is excluded
                should_skip = False
                for epath in excluded_paths:
                    if is_parent_of(epath, rp):
                        should_skip = True
                        break
                if should_skip:
                    continue
                for pat in SOURCE_SUFFIXES:
                    if pat.match(fn):
                        sources.append(rp)
                        ok = True
                        break
                if ok:
                    continue
                for pat in HEADER_SUFFIXES:
                    if pat.match(fn):
                        headers.append(rp)
                        break
        return headers, sources

    def get_relative_path(self, p):
        return os.path.relpath(p, self.base_dir)

    def get_name(self):
        return self.__class__.__name__

    def get_build_file_path(self):
        return os.path.join(self.base_dir, "%s.Build.py" % self.get_name())

