from conan import ConanFile

class Recipe(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeToolchain", "CMakeDeps", "VirtualRunEnv"

    def layout(self):
        self.folders.generators = "conan"

    def requirements(self):
        self.requires("glfw/3.4")
        self.requires("glad/0.1.36")
        self.requires("glm/0.9.9.8")
        self.requires("stb/cci.20230920")
        self.requires("perlinnoise/3.0.0")
        self.requires("spdlog/1.13.0")
        self.requires("cli11/2.4.1")
