# -*- encoding: utf-8 -*-
from build_target import BuildTarget


class SWRMain(BuildTarget):
    def __init__(self, base_dir):
        super(SWRMain, self).__init__(base_dir)
        self.is_library = False
        self.dependencies.append("Runtime")
        self.dependencies.append("SWR")
