# -*- encoding: utf-8 -*-
import os


BUILD_RULE_SUFFIX = ".Build.py"


class DependencyMissingError(Exception):
    def __init__(self, owner, missed):
        super(DependencyMissingError, self).__init__()
        self.owner = owner
        self.missed = missed

    def __str__(self):
        return "DependencyMissingError(%s, %s)" % (self.owner, self.missed)


class BuildRuleClassNotFoundError(Exception):
    def __init__(self, build_rule_class_name, file_path):
        self.build_rule_class_name = build_rule_class_name
        self.file_path = file_path

    def __str__(self):
        return "BuildRuleClassNotFoundError(%s, %s)" % (self.build_rule_class_name, self.file_path)


class DuplicatedTargetError(Exception):
    def __init__(self, name):
        self.name = name

    def __str__(self):
        return "DuplicatedTargetError(%s)" % self.name


class Collector(object):
    def __init__(self, root_dir):
        self.root_dir = os.path.abspath(root_dir)
        self.targets = {}

    def run(self):
        # walk dirs
        for root, dirs, files in os.walk(self.root_dir):
            for fn in files:
                if fn.endswith(BUILD_RULE_SUFFIX):
                    module_name = fn[: -len(BUILD_RULE_SUFFIX)]
                    build_file = os.path.join(root, fn)
                    gd = {}
                    execfile(build_file, gd)
                    if module_name not in gd:
                        raise BuildRuleClassNotFoundError(module_name, build_file)
                    clz = gd[module_name]
                    if module_name in self.targets:
                        raise DuplicatedTargetError(module_name)
                    self.targets[module_name] = clz(os.path.join(self.root_dir, root))

        # validate dependencies
        for k, v in self.targets.iteritems():
            for d in v.dependencies:
                if d not in self.targets:
                    raise DependencyMissingError(k, d)
