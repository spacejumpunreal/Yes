# -*- encoding: utf-8 -*-
import collector
import generator


def generate_vs_project(repo_dir, source_dir, build_dir, solution_path):
    c = collector.Collector(source_dir)
    c.run()
    g = generator.VS2019Generator(c, repo_dir, build_dir, solution_path)
    g.run()

