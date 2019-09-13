# -*- encoding: utf-8 -*-
import collector
import generator


def generate_vs_project(root_dir, build_dir, solution_path):
    c = collector.Collector(root_dir)
    c.run()
    g = generator.VS2019Generator(c, build_dir, solution_path)
    g.run()

