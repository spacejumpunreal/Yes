# -*- encoding: utf-8 -*-
from build_target import BuildTarget


class SWR(BuildTarget):
    def __init__(self, base_dir):
        super(SWR, self).__init__(base_dir)
        self.is_library = True
        self.dependencies.append("Runtime")