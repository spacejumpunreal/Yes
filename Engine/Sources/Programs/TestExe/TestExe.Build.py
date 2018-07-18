# -*- encoding: utf-8 -*-
from build_target import BuildTarget


class TestExe(BuildTarget):
    def __init__(self, base_dir):
        super(TestExe, self).__init__(base_dir)
        self.is_library = False
        self.dependencies.append("TestLib")
        

