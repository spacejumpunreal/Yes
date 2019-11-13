# -*- encoding: utf-8 -*-
from build_target import BuildTarget


class FastTest(BuildTarget):
    def __init__(self, base_dir):
        super(FastTest, self).__init__(base_dir)
        self.is_library = False
        self.dependencies.append("Runtime")
        self.order = 0
