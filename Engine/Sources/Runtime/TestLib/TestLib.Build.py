# -*- encoding: utf-8 -*-
from build_target import BuildTarget


class TestLib(BuildTarget):
    def __init__(self, base_dir):
        super(TestLib, self).__init__(base_dir)
        self.is_library = True
        

