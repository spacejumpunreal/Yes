# -*- encoding: utf-8 -*-
import os
import re
import uuid


SOURCE_SUFFIXES = re.compile(r".*\.cpp"),
HEADER_SUFFIXES = re.compile(r".*\.h"),


class BuildTarget(object):
    def __init__(self, base_dir):
        self.is_library = True
        self.base_dir = base_dir
        self.dependencies = []
        self.public_dirs = []
        # if has Public dir, use it as public include
        public_dir = os.path.join(base_dir, "Public")
        self.guid = uuid.uuid4()
        if os.path.exists(public_dir):
            self.public_dirs.append(public_dir)

    def collect_source_files(self):
        headers = []
        sources = []
        for root, _, files in os.walk(self.base_dir):
            for fn in files:
                ok = False
                for pat in SOURCE_SUFFIXES:
                    if pat.match(fn):
                        sources.append(os.path.join(root, fn))
                        ok = True
                        break
                if ok:
                    continue
                for pat in HEADER_SUFFIXES:
                    if pat.match(fn):
                        headers.append(os.path.join(root, fn))
                        break
        return headers, sources

    def get_relative_path(self, p):
        return os.path.relpath(p, self.base_dir)

    def get_name(self):
        return self.__class__.__name__

    def get_build_file_path(self):
        return os.path.join(self.base_dir, "%s.Build.py" % self.get_name())

