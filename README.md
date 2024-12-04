# hybrid_ffvideo_streamer

### Project Description

This project provides a C++/Cython/Python application for video streaming using the FFmpeg library. It allows users to stream video from a source (e.g., webcam) to an output destination (e.g., RTMP server) with optional watermarking. Hybrid video streamer integrates Python and C++ parts using Boost.Python and pybind11 for seamless operability between the two languages. This project employs a component-based structure with clear separation of concerns (command-line parsing, version printing, video streaming).

Features:

- Stream video using FFmpeg libraries
- Set input and output destinations
- Apply watermark image (optional)
- Configure FFmpeg logging level
- Component-based design: well-structured codebase for maintainability

### Setup and Usage

Prerequisites:

- CMake 3.30.2 or higher
- C++20 compliant compiler
- FFmpeg libraries (libavcodec, libavdevice, libavfilter, libavformat, libavutil, libpostproc, libswresample, libswscale)
- Boost libraries (including Boost.Python and Boost.Program_options) installed on your system
- Poco libraries (Net, Foundation) installed on your system
- Python 3.x, pip, pybind11, Cython 3.x

To get started with hybrid_ffvideo_streamer, clone the repository from GitHub:

```
$ git clone https://github.com/roman-empire-roman/hybrid_ffvideo_streamer.git
$ cd hybrid_ffvideo_streamer/command_line_args_parser/
$ mkdir build
$ cd build/
$ cmake -DCMAKE_CXX_COMPILER=/usr/bin/clang++-18 -DCMAKE_C_COMPILER=/usr/bin/clang-18 ../
$ make
$ cd ../../version_printer/
$ python3.11 setup.py build_ext --inplace
$ cd ../video_streamer/
$ mkdir build
$ cd build/
$ cmake -DCMAKE_CXX_COMPILER=/usr/bin/clang++-18 -DCMAKE_C_COMPILER=/usr/bin/clang-18 ../
$ cd ../../
```

Update the config.json file with your desired settings.
Run the application:

```
$ python3.11 hybrid_ffvideo_streamer.py --config configs/config.json
```

### Versions Used

Below are the versions of tools and libraries used during development and testing:

- FFmpeg: 7.1
    - libavutil: 59.39.100
    - libavcodec: 61.19.100
    - libavformat: 61.7.100
    - libavdevice: 61.3.100
    - libavfilter: 10.4.100
    - libswscale: 8.3.100
    - libswresample: 5.3.100
    - libpostproc: 58.3.100
- Boost: 1.74.0
- Poco: 1.13.3
- Frozen: 1.1.1
- RapidJSON: 1.1.0
- LodePNG: 20241015
- C++ Standard: C++20
- C++ Compiler: Clang 18.1.8
- CMake: 3.30.2
- GNU Make: 4.3
- GNU gdb/gdbserver: 12.1
- Python: 3.11.0
- pip: 24.3.1
- pybind11: 2.13.6
- Cython: 3.0.11
- Visual Studio Code: 1.95.3
- Docker: 27.3.1
- Docker Desktop: 4.35.1
- Host OS: Ubuntu 24.04.1 LTS
- Container OS: Ubuntu 22.04.5 LTS
- Git: 2.34.1

Ensure that your environment meets these or newer versions.

### Contact Information

Want to contribute, report an issue, or just chat about the project? Feel free to reach out to me:

- Telegram: [@roman_empire_roman](https://t.me/roman_empire_roman)
