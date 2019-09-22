# -*- encoding: utf-8 -*-
import os
import uuid
import _winreg
from xml_format import XmlNode


VCXPROJ_TEMPLATE = """<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" ToolsVersion="15.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
{ProjectConfiguration}
  <PropertyGroup Label="Globals">
    <VCProjectVersion>15.0</VCProjectVersion>
    <ProjectGuid>{TargetGUID}</ProjectGuid>
    <RootNamespace>{TargetName}</RootNamespace>
    <WindowsTargetPlatformVersion>{WindowsTargetPlatformVersion}</WindowsTargetPlatformVersion>
  </PropertyGroup>
  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.Default.props" />
{PropertyGroupConfiguration}
  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.props" />
{ImportGroupPropertySheet}
  <PropertyGroup Label="UserMacros" />
  <PropertyGroup />
{ItemDefinitionGroup}
{ItemGroupCLCompile}
{ItemGroupCLInclude}
{ItemGroupNone}
{ItemGroupProjectReference}
  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.targets" />
</Project>
"""

FILTER_TEMPLATE = """<?xml version="1.0" encoding="utf-8"?>
<Project ToolsVersion="4.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
{ItemGroupSources}
{ItemGroupFilters}
{ItemGroupIncludes}
{ItemGroupBuildFile}
</Project>

"""

SLN_TEMPLATE = """
Microsoft Visual Studio Solution File, Format Version 12.00
# Visual Studio 16
VisualStudioVersion = 16.0.29306.81
MinimumVisualStudioVersion = 10.0.40219.1
{ProjectsBlock}
Global
    GlobalSection(SolutionConfigurationPlatforms) = preSolution
{SolutionConfigurationPlatforms}
    EndGlobalSection
    GlobalSection(ProjectConfigurationPlatforms) = postSolution
{ProjectConfigurationPlatforms}
    EndGlobalSection
    GlobalSection(SolutionProperties) = preSolution
        HideSolutionNode = FALSE
    EndGlobalSection
    GlobalSection(NestedProjects) = preSolution
{NestedProjects}
    EndGlobalSection
    GlobalSection(ExtensibilityGlobals) = postSolution
        SolutionGuid = {SolutionGUID}
    EndGlobalSection
EndGlobal

"""

Configurations = ("Debug", "Release")
Platforms = ("x64", "Win32")
PlatformToolset = "v142"


def get_latest_windows_sdk_version():
    def cmp_version(a, b):
        a = a.split('.')
        b = b.split('.')
        la, lb = len(a), len(b)
        cmp_length = min(la, lb)
        for c in cmp_length:
            if a[c] < b[c]:
                return -1
            elif a[c] > b[c]:
                return 1
        return -1
    root = _winreg.OpenKey(_winreg.HKEY_LOCAL_MACHINE, r"SOFTWARE\Microsoft\Windows Kits\Installed Roots")
    idx = 0
    subs = []
    while True:
        try:
            subs.append(_winreg.EnumKey(root, idx))
            idx += 1
        except EnvironmentError:
            break
    return sorted(subs, cmp_version)[-1]


class VS2019Generator(object):
    def __init__(self, collector, build_dir, solution_path):
        self._targets = collector.targets
        self._source_root_dir = collector.root_dir
        self._build_dir = build_dir
        self._solution_path = solution_path

    def get_all_dependencies(self, target):
        q = [target]
        all_deps = set()
        while q:
            x = q.pop()
            if x in all_deps:
                continue
            all_deps.add(x)
            for d in x.dependencies:
                dd = self._targets[d]
                if dd not in all_deps:
                    q.append(dd)
        return all_deps

    @staticmethod
    def get_vcxproj_file_name(target):
        return target.get_name() + ".vs2019.vcxproj"

    @staticmethod
    def get_filter_file_name(target):
        return target.get_name() + ".vs2019.vcxproj.filters"

    @staticmethod
    def get_target_name(target):
        return target.get_name() + ".vs2019"

    @staticmethod
    def format_guid(guid):
        return "{%s}" % guid

    def run(self):
        indent = "  "
        print "targets:"
        for k, v in self._targets.iteritems():
            print "\t", k, ":", v.collect_source_files()
        print "source_root_dir:", self._source_root_dir
        print "build_dir", self._build_dir

        generate_prj_files = []
        for _, target in self._targets.iteritems():
            # write vcxproj
            vcxproj_file = os.path.join(self._build_dir, self.get_vcxproj_file_name(target))
            generate_prj_files.append((target, target.order, vcxproj_file))
            prj_config = XmlNode("ItemGroup", (), {"Label": "ProjectConfigurations"})
            for c in Configurations:
                for p in Platforms:
                    prj_config.append(
                        XmlNode("ProjectConfiguration", (
                            XmlNode("Configuration", c),
                            XmlNode("Platform", p),
                        ), {"Include": "%s|%s" % (c, p)})
                    )

            prop_group_config_frags = []
            target_type = "StaticLibrary" if target.is_library else "Application"
            for c in Configurations:
                for p in Platforms:
                    is_debug = "true" if c == "Debug" else "false"
                    is_not_debug = "true" if c != "Debug" else "false"
                    prop_group_config_frags.append(
                        XmlNode("PropertyGroup", (
                            XmlNode("ConfigurationType", target_type),
                            XmlNode("UseDebugLibraries", is_debug),
                            XmlNode("PlatformToolset", PlatformToolset),
                            XmlNode("WholeProgramOptimization", is_not_debug),
                            XmlNode("CharacterSet", "Unicode"),
                            XmlNode("OutDir", os.path.join("..", "Binaries", p) + '\\'),
                            XmlNode("IntDir", os.path.join(
                                "Intermediate", "$(Platform)", "$(Configuration)", "$(ProjectName)") + "\\"),
                        ), {"Condition": "'$(Configuration)|$(Platform)'=='%s|%s'" % (c, p)})
                    )

            import_group_property_sheet = []
            for c in Configurations:
                for p in Platforms:
                    d = {
                        "Label": "PropertySheets",
                        "Condition": "'$(Configuration)|$(Platform)'=='%s|%s'" % (c, p)
                    }
                    dd = {
                        "Project": r'$(UserRootDir)\Microsoft.Cpp.$(Platform).user.props',
                        "Condition": r"exists('$(UserRootDir)\Microsoft.Cpp.$(Platform).user.props')",
                        "Label": "LocalAppDataPlatform",
                    }
                    import_group_property_sheet.append(
                        XmlNode("ImportGroup", (
                            XmlNode("Import", (), dd),
                        ), d)
                    )

            item_def_group = []
            all_deps = self.get_all_dependencies(target)

            if target.include_source_dir:
                additional_include_directories = [self._source_root_dir]
            else:
                additional_include_directories = []
            additional_include_directories += [
                os.path.relpath(d, self._build_dir) for t in all_deps for d in t.exported_dirs]
            if target.export_base_dir:
                additional_include_directories.append(os.path.relpath(target.base_dir, self._build_dir))
            additional_include_directories.append("%(AdditionalIncludeDirectories)")
            additional_link_libs = ["d3d12.lib", "dxgi.lib", "d3dcompiler.lib", "%(AdditionalDependencies)"]
            for c in Configurations:
                for p in Platforms:
                    is_debug = c == "Debug"
                    cd = {"Condition": "'$(Configuration)|$(Platform)'=='%s|%s'" % (c, p)}
                    macros = ["_DEBUG" if is_debug else "NDEBUG"]
                    if p == "Win32":
                        macros.append("WIN32")
                    macros.append("%(PreprocessorDefinitions)")
                    item_def_group.append(
                        XmlNode("ItemDefinitionGroup", (
                            XmlNode("ClCompile", (
                                XmlNode("WarningLevel", "Level4"),
                                XmlNode("Optimization", "Disabled" if is_debug else "MaxSpeed"),
                                XmlNode("SDLCheck", "true"),
                                XmlNode("PreprocessorDefinitions", ";".join(macros)),
                                XmlNode("ConformanceMode", "true"),
                                XmlNode("MultiProcessorCompilation", "true"),
                                XmlNode("AdditionalIncludeDirectories", ";".join(additional_include_directories)),
                                XmlNode("LanguageStandard", "stdcpplatest"),
                                XmlNode("EnableEnhancedInstructionSet", "AdvancedVectorExtensions2"),
                                XmlNode("TreatWarningAsError", "true"),
                            )),
                            XmlNode("Link", (
                                XmlNode("EnableCOMDATFolding", "false" if is_debug else "true"),
                                XmlNode("OptimizeReferences", "false" if is_debug else "true"),
                                XmlNode("AdditionalDependencies", ";".join(additional_link_libs))
                            )),
                        ), cd)
                    )

            headers, sources = target.collect_source_files()

            def create_item_group(tag, files):
                root = XmlNode("ItemGroup", ())
                for f in files:
                    root.append(XmlNode(tag, (), {"Include": os.path.relpath(f, self._build_dir)}))
                return root
            item_group_clcompile = create_item_group("ClCompile", sources)
            item_group_clinclude = create_item_group("ClInclude", headers)

            nones = [target.get_build_file_path()]
            item_group_none = create_item_group("None", nones)

            item_group_project_reference = XmlNode("ItemGroup", ())
            for d in all_deps:
                if d is target:
                    continue
                pnode = XmlNode(
                    "ProjectReference",
                    content=(XmlNode("Project", self.format_guid(d.guid)),),
                    attrib={"Include": self.get_vcxproj_file_name(d)})
                item_group_project_reference.append(pnode)

            sdk_version = get_latest_windows_sdk_version()
            content = VCXPROJ_TEMPLATE.format(
                ProjectConfiguration=prj_config.format(indent, 1),
                TargetGUID="{%s}" % target.guid,
                TargetName=self.get_target_name(target),
                WindowsTargetPlatformVersion=sdk_version,
                PropertyGroupConfiguration="".join(map(lambda x: x.format(indent, 1), prop_group_config_frags)),
                ImportGroupPropertySheet="".join(map(lambda x: x.format(indent, 1), import_group_property_sheet)),
                ItemDefinitionGroup="".join(map(lambda x: x.format(indent, 1), item_def_group)),
                ItemGroupCLCompile=item_group_clcompile.format(indent, 1),
                ItemGroupCLInclude=item_group_clinclude.format(indent, 1),
                ItemGroupProjectReference=item_group_project_reference.format(indent, 1),
                ItemGroupNone=item_group_none.format(indent, 1),
            )
            with open(vcxproj_file, "wb") as wf:
                wf.write(content)

            # write filter file
            used_filters = set()

            def add_all_parents_for_path(pth, st):
                while pth:
                    st.add(pth)
                    pth = os.path.dirname(pth)
                return st

            def create_item_group_with_filter(tag, files):
                children = []
                for f in files:
                    rp = os.path.dirname(target.get_relative_path(f))
                    if rp:
                        add_all_parents_for_path(rp, used_filters)
                    children.append(XmlNode(tag, (
                        XmlNode("Filter", rp),
                    ), {"Include": os.path.relpath(f, self._build_dir)}))
                return XmlNode("ItemGroup", children)
            item_group_sources = create_item_group_with_filter("ClCompile", sources)
            item_group_includes = create_item_group_with_filter("ClInclude", headers)

            def create_item_group_filters():
                children = []
                for f in used_filters:
                    child = XmlNode("Filter", (
                        XmlNode("UniqueIdentifier", "{%s}" % uuid.uuid4()),
                    ), {"Include": f})
                    children.append(child)
                return XmlNode("ItemGroup", children)
            item_group_filters = create_item_group_filters()

            item_group_build_file = XmlNode("ItemGroup", (
                XmlNode("None", "", {"Include": os.path.relpath(self._build_dir, target.get_build_file_path())}),
            ))

            content = FILTER_TEMPLATE.format(
                ItemGroupSources=item_group_sources.format(indent, 1),
                ItemGroupFilters=item_group_filters.format(indent, 1),
                ItemGroupIncludes=item_group_includes.format(indent, 1),
                ItemGroupBuildFile=item_group_build_file.format(indent, 1),
            )
            filter_file = os.path.join(self._build_dir, self.get_filter_file_name(target))
            with open(filter_file, "wb") as wf:
                wf.write(content)

        # write sln
        # SolutionConfigurationPlatforms
        frags = []
        for c in Configurations:
            for p in Platforms:
                line = "\t\t%s|%s = %s|%s\n" % (c, p, c, p)
                frags.append(line)
        solution_config_platform_block = "".join(frags)

        # ProjectConfigurationPlatforms
        frags = []
        for t in self._targets.itervalues():
            for c in Configurations:
                for p in Platforms:
                    line = "\t\t{%s}.%s|%s.ActiveCfg = %s|%s\n" % (str(t.guid).upper(), c, p, c, p)
                    frags.append(line)
                    line = ("\t\t{%s}.%s|%s.Build.0 = %s|%s\n" % (str(t.guid).upper(), c, p, c, p))
                    frags.append(line)
        project_config_platform_block = "".join(frags)

        # NestedProjects
        path2guid = {}
        nested_projects = []
        for t in self._targets.itervalues():
            rp = os.path.relpath(os.path.dirname(t.base_dir), self._source_root_dir)
            if rp and rp != '.':
                if rp not in path2guid:
                    guid = uuid.uuid4()
                    path2guid[rp] = guid
                else:
                    guid = path2guid[rp]
                nested_projects.append("\t\t{%s} = {%s}\n" % (str(t.guid).upper(), str(guid).upper()))

        frags = []
        for path, guid in path2guid.iteritems():
            s = 'Project("{%s}") = "%s", "%s", "{%s}"\nEndProject\n' % (
                "2150E333-8FDC-42A3-9474-1A3956D46DE8", path, path, str(guid).upper())
            frags.append(s)

        solution_parent = os.path.dirname(self._solution_path)
        sorted_prj_files = sorted(generate_prj_files, key=lambda x: x[1])
        for target, _, vcxproj_file in sorted_prj_files:
            s = 'Project("{ProjectType}") = "{TargetName}", "{VCXProjPath}", "{PrjGUID}"\nEndProject\n'.format(
                ProjectType="{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}",
                TargetName=self.get_target_name(target),
                VCXProjPath=os.path.relpath(vcxproj_file, solution_parent),
                PrjGUID="{%s}" % str(target.guid).upper()
            )
            frags.append(s)
        project_block = "".join(frags)

        content = SLN_TEMPLATE.format(
            ProjectsBlock=project_block,
            SolutionConfigurationPlatforms=solution_config_platform_block,
            ProjectConfigurationPlatforms=project_config_platform_block,
            NestedProjects="".join(nested_projects),
            SolutionGUID="{%s}" % uuid.uuid4(),
        )
        with open(self._solution_path, "wb") as wf:
            wf.write(content)

