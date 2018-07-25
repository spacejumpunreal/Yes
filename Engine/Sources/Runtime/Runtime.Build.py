# -*- encoding: utf-8 -*-
from build_target import BuildTarget


class Runtime(BuildTarget):
    def __init__(self, base_dir):
        super(Runtime, self).__init__(base_dir)
        self.is_library = True
        self.excluded_paths.append("Todo")