import os
import sys
from Cython.Build import cythonize
from setuptools import setup, Extension

project_directory = os.path.abspath(os.path.dirname(__file__))
include_path = os.path.join(project_directory, "include")
if not os.path.isdir(include_path):
    print(f"path '{include_path}' does NOT exist")
    sys.exit()

extensions = [
    Extension(
        name="version_printer",
        sources=["version_printer.pyx"],
        language="c++",
        include_dirs=[include_path],
        extra_compile_args=["-std=c++20", "-Wall", "-Wextra", "-v"],
        extra_link_args=["-v"]
    )
]

setup(
    name="version_printer",
    ext_modules=cythonize(
        extensions,
        compiler_directives={'language_level' : "3str"}
    )
)
