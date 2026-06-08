from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout, CMakeDeps
from conan.tools.files import copy


class FastRulesConan(ConanFile):
    name = "fastrules"
    version = "0.1.0"
    license = "MIT"
    url = "https://github.com/asulwer/fastrules"
    description = "High-performance C++23 business rules engine with Lua expressions"
    topics = ("rules-engine", "lua", "cpp23", "business-logic")

    # Binary configuration
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "with_json": [True, False],
        "with_xml": [True, False],
        "with_db": [True, False],
        "use_luajit": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "with_json": True,
        "with_xml": False,
        "with_db": False,
        "use_luajit": False,
    }

    # Sources are located in the same place as this recipe, copy sources to the build directory
    exports_sources = "CMakeLists.txt", "src/*", "include/*", "extensions/*", "tests/*", "examples/*", "docs/*", "benchmarks/*", "scripts/*"

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def requirements(self):
        if self.options.use_luajit:
            self.requires("luajit/2.1.0-beta3")
        else:
            self.requires("lua/5.4.6")
        self.requires("sol2/3.3.0")
        if self.options.with_json:
            self.requires("nlohmann_json/3.11.3")
        if self.options.with_xml:
            self.requires("pugixml/1.14")
        if self.options.with_db:
            self.requires("soci/4.0.3")

    def build_requirements(self):
        self.tool_requires("cmake/[>=3.28]")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self)
        tc.variables["FASTRULES_BUILD_TESTS"] = False
        tc.variables["FASTRULES_BUILD_EXAMPLES"] = False
        tc.variables["FASTRULES_BUILD_EXTENSIONS"] = self.options.with_json or self.options.with_xml or self.options.with_db
        tc.variables["FASTRULES_BUILD_DB"] = self.options.with_db
        tc.variables["FASTRULES_USE_LUAJIT"] = self.options.use_luajit
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = ["fastrules"]
        if self.options.with_json:
            self.cpp_info.libs.append("fastrules-json")
        if self.options.with_xml:
            self.cpp_info.libs.append("fastrules-xml")
        if self.options.with_db:
            self.cpp_info.libs.append("fastrules-db")
        self.cpp_info.includedirs = ["include"]
        self.cpp_info.cppstd = 23
