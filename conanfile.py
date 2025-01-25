from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout, CMakeDeps
from conan.tools.scm import Git
from conan.tools.files import collect_libs, copy
import os
import shutil

required_conan_version = ">=2.0"

class test(ConanFile):

	name = "test"
	version = "0.1.0"

	settings = "os", "compiler", "build_type", "arch"

	def layout(self):
		cmake_layout(self)

	def configure(self):
		self.settings.rm_safe("compiler.cppstd")
		self.settings.rm_safe("compiler.libcxx")

	def generate(self):

		deps = CMakeDeps(self)
		deps.generate()

		tc = CMakeToolchain(self)
		tc.cache_variables["CMAKE_CONFIGURATION_TYPES"] = str(self.settings.build_type)
		tc.generate()

	def build(self):
		cmake = CMake(self)
		cmake.configure()
		cmake.build()
