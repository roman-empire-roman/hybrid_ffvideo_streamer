#!/usr/bin/env python3.11
# -*- coding: UTF-8 -*-

import os
import sys

def module_exists(library_path, module_name):
    if not library_path:
        print(f"[module_exists]; library path is empty", file=sys.stderr)
        return False
    if not module_name:
        print(f"[module_exists]; module name is empty", file=sys.stderr)
        return False

    full_module_name = str()
    if module_name.endswith('.so'):
        if library_path.endswith('/'):
            full_module_name = library_path + module_name
        else:
            full_module_name = library_path + '/' + module_name
    else:
        if library_path.endswith('/'):
            full_module_name = library_path + module_name + '.so'
        else:
            full_module_name = library_path + '/' + module_name + '.so'

    if not os.path.isfile(full_module_name):
        print(
            f"[module_exists]; file '{full_module_name}' does NOT exist",
            file=sys.stderr
        )
        return False
    return True

if __name__ == "__main__":
    current_directory = os.getcwd()

    library_path = os.path.join(current_directory, 'libraries')
    if not os.path.exists(library_path):
        print(
            f"[main]; library path '{library_path}' does NOT exist",
            file=sys.stderr
        )
        sys.exit()
    if library_path not in sys.path:
        sys.path.append(library_path)

    try:
        if not module_exists(library_path, "command_line_args_parser"):
            sys.exit()
        import command_line_args_parser
        parser = command_line_args_parser.CommandLineArgsParser()
        if not parser.parse(sys.argv):
            sys.exit()
        config_file_name = parser.getConfigFileName()
        print(f"[main]; config file name: '{config_file_name}'")

        if not module_exists(library_path, "version_printer"):
            sys.exit()
        import version_printer
        version_printer.print_libraries_versions()

        if not module_exists(library_path, "video_streamer"):
            sys.exit()
        import video_streamer
        streamer = video_streamer.VideoStreamer()
        if not streamer.setup(config_file_name):
            sys.exit()
        if not streamer.process():
            sys.exit()
    except Exception as exception:
        print(
            f"[main]; exception '{exception}' was caught",
            file=sys.stderr
        )
    except:
        print(
            f"[main]; unknown exception was caught",
            file=sys.stderr
        )
