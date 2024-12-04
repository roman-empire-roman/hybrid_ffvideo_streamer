import sys

cdef extern from "libavutil/ffversion.h":
    const char* FFMPEG_VERSION

cdef extern from "boost/version.hpp":
    int BOOST_VERSION

cdef extern from "frozen/bits/version.h":
    int FROZEN_MAJOR_VERSION
    int FROZEN_MINOR_VERSION
    int FROZEN_PATCH_VERSION

cdef extern from "Poco/Version.h":
    int POCO_VERSION

cdef extern from "rapidjson/rapidjson.h":
    int RAPIDJSON_MAJOR_VERSION
    int RAPIDJSON_MINOR_VERSION
    int RAPIDJSON_PATCH_VERSION

cdef print_ffmpeg_version():
    cdef const char* raw_version_description = FFMPEG_VERSION
    if (NULL == raw_version_description):
        print(
            "[VersionPrinter::printFFmpegVersion]; "
            "pointer to raw version description is NULL",
            file=sys.stderr
        )
        return

    cdef str version_description = ""
    try:
        version_description = bytes(raw_version_description).decode('utf-8')
    except UnicodeDecodeError as exception:
        print(
            f"[VersionPrinter::printFFmpegVersion]; exception 'UnicodeDecodeError' was successfully caught "
            f"while decoding the raw version description; "
            f"exception description: \"{exception}\"",
            file=sys.stderr
        )
        return
    except:
        print(
            "[VersionPrinter::printFFmpegVersion]; unknown exception was caught "
            "while decoding the raw version description",
            file=sys.stderr
        )
        return

    cdef int i = 0
    for i in range(len(version_description)):
        if version_description[i].isdigit():
            break

    cdef str ffmpeg_version = ""
    for j in range(i, len(version_description)):
        if version_description[j].isdigit() or ('.' == version_description[j]):
            ffmpeg_version += version_description[j]
        else:
            break

    if ffmpeg_version:
        print(f"[VersionPrinter::printFFmpegVersion]; FFmpeg version: '{ffmpeg_version}'")

def print_libraries_versions():
    print_ffmpeg_version()

    boost_version = f"{BOOST_VERSION // 100000}.{(BOOST_VERSION // 100) % 1000}.{BOOST_VERSION % 100}"
    print(f"[VersionPrinter::printLibrariesVersions]; Boost version: '{boost_version}'")

    frozen_version = f"{FROZEN_MAJOR_VERSION}.{FROZEN_MINOR_VERSION}.{FROZEN_PATCH_VERSION}"
    print(f"[VersionPrinter::printLibrariesVersions]; Frozen version: '{frozen_version}'")

    poco_version = f"{(POCO_VERSION >> 24) & 0xFF}.{(POCO_VERSION >> 16) & 0xFF}.{(POCO_VERSION >> 8) & 0xFF}"
    print(f"[VersionPrinter::printLibrariesVersions]; Poco version: '{poco_version}'")

    rapidjson_version = f"{RAPIDJSON_MAJOR_VERSION}.{RAPIDJSON_MINOR_VERSION}.{RAPIDJSON_PATCH_VERSION}"
    print(f"[VersionPrinter::printLibrariesVersions]; RapidJSON version: '{rapidjson_version}'")
