import os
import setuptools

from   distutils.core import Extension
from   distutils.core import setup
from   distutils.util import get_platform


MODULE_NAME                       = 'moxie'
MODULE_ROOT                       = os.path.join(os.path.dirname(os.path.abspath(__file__)), MODULE_NAME)
MODULE_AUTHOR                     = 'Russell Klenk'
MODULE_AUTHOR_EMAIL               = 'contact@russellklenk.com'
MODULE_BASE_URL                   = 'https://github.com/russellklenk/moxie/'
MODULE_ISSUES_URL                 = 'https://github.com/russellklenk/moxie/issues'
MODULE_VERSION_MAJOR              = 0
MODULE_VERSION_MINOR              = 1
MODULE_VERSION_REVISION           = 0
MODULE_VERSION_STRING             = '{}.{}.{}'.format(MODULE_VERSION_MAJOR, MODULE_VERSION_MINOR, MODULE_VERSION_REVISION)
MODULE_DESCRIPTION                = 'The moxie resource management toolkit'
MODULE_LONG_DESCRIPTION           = """
moxie is a cross-platform toolkit for management of system resources.
It allows fine-grained control over memory allocation and distribution
of work across hardware threads with semi-automated extraction of 
parallelism using a job scheduler.
"""

MOXIE_CORE_EXTENSION_NAME         = '_moxie_core'
MOXIE_CORE_ROOT                   = os.path.join(MODULE_ROOT, MOXIE_CORE_EXTENSION_NAME)
MOXIE_CORE_HEADER_ROOT            = os.path.join(MOXIE_CORE_ROOT, 'include')
MOXIE_CORE_SOURCE_ROOT            = os.path.join(MOXIE_CORE_ROOT, 'src')

MOXIE_CORE_COMMON_DEFINES         = [
    ('MOXIE_CORE_VERSION_MAJOR'   , '{}'.format(MODULE_VERSION_MAJOR   )),
    ('MOXIE_CORE_VERSION_MINOR'   , '{}'.format(MODULE_VERSION_MINOR   )),
    ('MOXIE_CORE_VERSION_REVISION', '{}'.format(MODULE_VERSION_REVISION)),
    ('__STDC_FORMAT_MACROS'       , '1'                                 ),
    ('PY_SSIZE_T_CLEAN'           , '1'                                 )
]
MOXIE_CORE_LINUX_DEFINES          = [
    ('_GNU_SOURCE'                , '1'                                 )
]
MOXIE_CORE_MACOS_DEFINES          = [
    # None
]
MOXIE_CORE_POSIX_DEFINES          = [
    # None
]
MOXIE_CORE_POWER_DEFINES          = [
    # None
]
MOXIE_CORE_WINOS_DEFINES          = [
    # None
]

MOXIE_CORE_COMMON_INCLUDE_DIRS    = [
    'moxie/_moxie_core/include'
]
MOXIE_CORE_LINUX_INCLUDE_DIRS     = [
    # None
]
MOXIE_CORE_MACOS_INCLUDE_DIRS     = [
    # None
]
MOXIE_CORE_POSIX_INCLUDE_DIRS     = [
    # None
]
MOXIE_CORE_POWER_INCLUDE_DIRS     = [
    # None
]
MOXIE_CORE_WINOS_INCLUDE_DIRS     = [
    # None
]

MOXIE_CORE_COMMON_HEADER_FILES    = [
    'moxie/_moxie_core/include/internal/memory.h',
    'moxie/_moxie_core/include/internal/platform.h',
    'moxie/_moxie_core/include/internal/rtloader.h',
    'moxie/_moxie_core/include/internal/scheduler.h',
    'moxie/_moxie_core/include/internal/version.h',
    'moxie/_moxie_core/include/moxie_core.h'
]
MOXIE_CORE_LINUX_HEADER_FILES     = [
    # None
]
MOXIE_CORE_MACOS_HEADER_FILES     = [
    # None
]
MOXIE_CORE_POSIX_HEADER_FILES     = [
    # None
]
MOXIE_CORE_POWER_HEADER_FILES     = [
    # None
]
MOXIE_CORE_WINOS_HEADER_FILES     = [
    # None
]

MOXIE_CORE_COMMON_LIBRARY_DIRS    = [
    # None
]
MOXIE_CORE_LINUX_LIBRARY_DIRS     = [
    # None
]
MOXIE_CORE_MACOS_LIBRARY_DIRS     = [
    # None
]
MOXIE_CORE_POWER_LIBRARY_DIRS     = [
    # None
]
MOXIE_CORE_WINOS_LIBRARY_DIRS     = [
    # None
]

MOXIE_CORE_COMMON_SOURCE_FILES    = [
    'moxie/_moxie_core/src/internal/memory.c',
    'moxie/_moxie_core/src/moxie_core.c'
]
MOXIE_CORE_POSIX_SOURCE_FILES     = [
    'moxie/_moxie_core/src/internal/posix/memory_posix.c',
    'moxie/_moxie_core/src/internal/posix/scheduler_posix.c'
]
MOXIE_CORE_WINOS_SOURCE_FILES     = [
    'moxie/_moxie_core/src/internal/winos/cvmarkers.h',
    'moxie/_moxie_core/src/internal/winos/memory_winos.c',
    'moxie/_moxie_core/src/internal/winos/scheduler_winos.c'
]
MOXIE_CORE_LINUX_SOURCE_FILES     = [
    # None
]
MOXIE_CORE_MACOS_SOURCE_FILES     = [
    # None
]
MOXIE_CORE_POWER_SOURCE_FILES     = [
    # None
]

MOXIE_CORE_COMMON_LIBRARY_FILES   = [
    # None
]
MOXIE_CORE_LINUX_LIBRARY_FILES    = [
    # None
]
MOXIE_CORE_MACOS_LIBRARY_FILES    = [
    # None
]
MOXIE_CORE_POWER_LIBRARY_FILES    = [
    # None
]
MOXIE_CORE_WINOS_LIBRARY_FILES    = [
    # None
]

MOXIE_CORE_LINUX_CCFLAGS          = ['-O0', '-g', '-fstrict-aliasing']
MOXIE_CORE_MACOS_CCFLAGS          = ['-O0', '-g', '-fstrict-aliasing']
MOXIE_CORE_POSIX_CCFLAGS          = ['-pthread']
MOXIE_CORE_POWER_CCFLAGS          = ['-O0', '-g', '-fstrict-aliasing']
MOXIE_CORE_WINOS_CCFLAGS          = []

MOXIE_CORE_LINUX_LDFLAGS          = []
MOXIE_CORE_MACOS_LDFLAGS          = []
MOXIE_CORE_POSIX_LDFLAGS          = []
MOXIE_CORE_POWER_LDFLAGS          = []
MOXIE_CORE_WINOS_LDFLAGS          = []

MOXIE_CORE_PLATFORM_DEFINES       = []
MOXIE_CORE_PLATFORM_CCFLAGS       = []
MOXIE_CORE_PLATFORM_LDFLAGS       = []
MOXIE_CORE_PLATFORM_HEADERS       = []
MOXIE_CORE_PLATFORM_SOURCES       = []
MOXIE_CORE_PLATFORM_LIBRARIES     = []
MOXIE_CORE_PLATFORM_INCLUDE_DIRS  = []
MOXIE_CORE_PLATFORM_LIBRARY_DIRS  = []

PLATFORM_NAME_UNKNOWN             = 'Unknown'
PLATFORM_NAME_LINUX               = 'Linux'
PLATFORM_NAME_MACOS               = 'macOS'
PLATFORM_NAME_POWER               = 'Power'
PLATFORM_NAME_WINOS               = 'Windows'
PLATFORM_NAME                     = PLATFORM_NAME_UNKNOWN


def detect_platform():
    """
    Attempt to detect the target platform and set one of IS_WINDOWS, IS_LINUX, IS_MACOS, IS_POWER, etc.
    This is really just a hack that will probably need to be substantially extended in the future as the result is used to specify platform-specific libraries, etc.

    Returns
    -------
      `True` if the platform is successfully detected.
    """
    global PLATFORM_NAME
    global PLATFORM_NAME_LINUX
    global PLATFORM_NAME_MACOS
    global PLATFORM_NAME_POWER
    global PLATFORM_NAME_WINOS
    global PLATFORM_NAME_UNKNOWN
    platform: str = get_platform()
    if not platform:
        print('ERROR: Unexpected empty return value from distutils.util.get_platform; target platform cannot be determined.')
        PLATFORM_NAME = PLATFORM_NAME_UNKNOWN
        return False

    platform = platform.lower()
    print(f'STATUS: Performing platform detection based on distutils.util.get_platform() => {platform}.')
    if 'linux' in platform:
        if 'ppc64le' in platform or 'power' in platform:
            PLATFORM_NAME = PLATFORM_NAME_POWER
        else:
            PLATFORM_NAME = PLATFORM_NAME_LINUX
        return True
    elif 'macos' in platform or 'darwin' in platform:
        PLATFORM_NAME = PLATFORM_NAME_MACOS
        return True
    elif 'win32' in platform or 'win' in platform:
        PLATFORM_NAME = PLATFORM_NAME_WINOS
        return True
    else:
        print('ERROR: Unable to detect target platform; please update detection logic in detect_platform() in setup.py.')
        PLATFORM_NAME = PLATFORM_NAME_UNKNOWN
        return False


def make_moxie_core_extension():
    """
    Set up the build environment based on the current target platform and construct the `distutils.core.Extension` object for the _moxie_core extension.

    Returns
    -------
      A `distutils.core.Extension` object defining the extension.
    """
    global PLATFORM_NAME
    global PLATFORM_NAME_UNKNOWN
    global PLATFORM_NAME_LINUX
    global PLATFORM_NAME_MACOS
    global PLATFORM_NAME_POWER
    global PLATFORM_NAME_WINOS
    if PLATFORM_NAME == PLATFORM_NAME_UNKNOWN:
        raise RuntimeError('Unable to set up build environment; unrecognized target platform.')

    global MOXIE_CORE_PLATFORM_DEFINES
    global MOXIE_CORE_PLATFORM_INCLUDE_DIRS
    global MOXIE_CORE_PLATFORM_LIBRARY_DIRS
    global MOXIE_CORE_PLATFORM_HEADERS
    global MOXIE_CORE_PLATFORM_SOURCES
    global MOXIE_CORE_PLATFORM_LIBRARIES
    global MOXIE_CORE_PLATFORM_CCFLAGS
    global MOXIE_CORE_PLATFORM_LDFLAGS
    
    global MOXIE_CORE_COMMON_DEFINES      , MOXIE_CORE_LINUX_DEFINES      , MOXIE_CORE_MACOS_DEFINES      , MOXIE_CORE_POSIX_DEFINES      , MOXIE_CORE_POWER_DEFINES      , MOXIE_CORE_WINOS_DEFINES
    global MOXIE_CORE_COMMON_INCLUDE_DIRS , MOXIE_CORE_LINUX_INCLUDE_DIRS , MOXIE_CORE_MACOS_INCLUDE_DIRS , MOXIE_CORE_POSIX_INCLUDE_DIRS , MOXIE_CORE_POWER_INCLUDE_DIRS , MOXIE_CORE_WINOS_INCLUDE_DIRS
    global MOXIE_CORE_COMMON_LIBRARY_DIRS , MOXIE_CORE_LINUX_LIBRARY_DIRS , MOXIE_CORE_MACOS_LIBRARY_DIRS ,                                 MOXIE_CORE_POWER_LIBRARY_DIRS , MOXIE_CORE_WINOS_LIBRARY_DIRS
    global MOXIE_CORE_COMMON_HEADER_FILES , MOXIE_CORE_LINUX_HEADER_FILES , MOXIE_CORE_MACOS_HEADER_FILES , MOXIE_CORE_POSIX_HEADER_FILES , MOXIE_CORE_POWER_HEADER_FILES , MOXIE_CORE_WINOS_HEADER_FILES
    global MOXIE_CORE_COMMON_SOURCE_FILES , MOXIE_CORE_LINUX_SOURCE_FILES , MOXIE_CORE_MACOS_SOURCE_FILES , MOXIE_CORE_POSIX_SOURCE_FILES , MOXIE_CORE_POWER_SOURCE_FILES , MOXIE_CORE_WINOS_SOURCE_FILES
    global MOXIE_CORE_COMMON_LIBRARY_FILES, MOXIE_CORE_LINUX_LIBRARY_FILES, MOXIE_CORE_MACOS_LIBRARY_FILES,                                 MOXIE_CORE_POWER_LIBRARY_FILES, MOXIE_CORE_WINOS_LIBRARY_FILES
    global                                  MOXIE_CORE_LINUX_CCFLAGS      , MOXIE_CORE_MACOS_CCFLAGS      , MOXIE_CORE_POSIX_CCFLAGS      , MOXIE_CORE_POWER_CCFLAGS      , MOXIE_CORE_WINOS_CCFLAGS
    global                                  MOXIE_CORE_LINUX_LDFLAGS      , MOXIE_CORE_MACOS_LDFLAGS      , MOXIE_CORE_POSIX_LDFLAGS      , MOXIE_CORE_POWER_LDFLAGS      , MOXIE_CORE_WINOS_LDFLAGS

    if PLATFORM_NAME == PLATFORM_NAME_LINUX:
        MOXIE_CORE_PLATFORM_DEFINES       = MOXIE_CORE_LINUX_DEFINES      + MOXIE_CORE_POSIX_DEFINES
        MOXIE_CORE_PLATFORM_INCLUDE_DIRS  = MOXIE_CORE_LINUX_INCLUDE_DIRS + MOXIE_CORE_POSIX_INCLUDE_DIRS
        MOXIE_CORE_PLATFORM_LIBRARY_DIRS  = MOXIE_CORE_LINUX_LIBRARY_DIRS
        MOXIE_CORE_PLATFORM_HEADERS       = MOXIE_CORE_LINUX_HEADER_FILES + MOXIE_CORE_POSIX_HEADER_FILES
        MOXIE_CORE_PLATFORM_SOURCES       = MOXIE_CORE_LINUX_SOURCE_FILES + MOXIE_CORE_POSIX_SOURCE_FILES
        MOXIE_CORE_PLATFORM_LIBRARIES     = MOXIE_CORE_LINUX_LIBRARY_FILES
        MOXIE_CORE_PLATFORM_CCFLAGS       = MOXIE_CORE_LINUX_CCFLAGS      + MOXIE_CORE_POSIX_CCFLAGS
        MOXIE_CORE_PLATFORM_LDFLAGS       = MOXIE_CORE_LINUX_LDFLAGS      + MOXIE_CORE_POSIX_LDFLAGS
    
    elif PLATFORM_NAME == PLATFORM_NAME_MACOS:
        MOXIE_CORE_PLATFORM_DEFINES       = MOXIE_CORE_MACOS_DEFINES      + MOXIE_CORE_POSIX_DEFINES
        MOXIE_CORE_PLATFORM_INCLUDE_DIRS  = MOXIE_CORE_MACOS_INCLUDE_DIRS + MOXIE_CORE_POSIX_INCLUDE_DIRS
        MOXIE_CORE_PLATFORM_LIBRARY_DIRS  = MOXIE_CORE_MACOS_LIBRARY_DIRS
        MOXIE_CORE_PLATFORM_HEADERS       = MOXIE_CORE_MACOS_HEADER_FILES + MOXIE_CORE_POSIX_HEADER_FILES
        MOXIE_CORE_PLATFORM_SOURCES       = MOXIE_CORE_MACOS_SOURCE_FILES + MOXIE_CORE_POSIX_SOURCE_FILES
        MOXIE_CORE_PLATFORM_LIBRARIES     = MOXIE_CORE_MACOS_LIBRARY_FILES
        MOXIE_CORE_PLATFORM_CCFLAGS       = MOXIE_CORE_MACOS_CCFLAGS      + MOXIE_CORE_POSIX_CCFLAGS
        MOXIE_CORE_PLATFORM_LDFLAGS       = MOXIE_CORE_MACOS_LDFLAGS      + MOXIE_CORE_POSIX_LDFLAGS


    elif PLATFORM_NAME == PLATFORM_NAME_POWER:
        MOXIE_CORE_PLATFORM_DEFINES       = MOXIE_CORE_POWER_DEFINES      + MOXIE_CORE_POSIX_DEFINES
        MOXIE_CORE_PLATFORM_INCLUDE_DIRS  = MOXIE_CORE_POWER_INCLUDE_DIRS + MOXIE_CORE_POSIX_INCLUDE_DIRS
        MOXIE_CORE_PLATFORM_LIBRARY_DIRS  = MOXIE_CORE_POWER_LIBRARY_DIRS
        MOXIE_CORE_PLATFORM_HEADERS       = MOXIE_CORE_POWER_HEADER_FILES + MOXIE_CORE_POSIX_HEADER_FILES
        MOXIE_CORE_PLATFORM_SOURCES       = MOXIE_CORE_POWER_SOURCE_FILES + MOXIE_CORE_POSIX_SOURCE_FILES
        MOXIE_CORE_PLATFORM_LIBRARIES     = MOXIE_CORE_POWER_LIBRARY_FILES
        MOXIE_CORE_PLATFORM_CCFLAGS       = MOXIE_CORE_POWER_CCFLAGS      + MOXIE_CORE_POSIX_CCFLAGS
        MOXIE_CORE_PLATFORM_LDFLAGS       = MOXIE_CORE_POWER_LDFLAGS      + MOXIE_CORE_POSIX_LDFLAGS

    elif PLATFORM_NAME == PLATFORM_NAME_WINOS:
        MOXIE_CORE_PLATFORM_DEFINES       = MOXIE_CORE_WINOS_DEFINES
        MOXIE_CORE_PLATFORM_INCLUDE_DIRS  = MOXIE_CORE_WINOS_INCLUDE_DIRS
        MOXIE_CORE_PLATFORM_LIBRARY_DIRS  = MOXIE_CORE_WINOS_LIBRARY_DIRS
        MOXIE_CORE_PLATFORM_HEADERS       = MOXIE_CORE_WINOS_HEADER_FILES
        MOXIE_CORE_PLATFORM_SOURCES       = MOXIE_CORE_WINOS_SOURCE_FILES
        MOXIE_CORE_PLATFORM_LIBRARIES     = MOXIE_CORE_WINOS_LIBRARY_FILES
        MOXIE_CORE_PLATFORM_CCFLAGS       = MOXIE_CORE_WINOS_CCFLAGS
        MOXIE_CORE_PLATFORM_LDFLAGS       = MOXIE_CORE_WINOS_LDFLAGS

    else:
        raise RuntimeError('Unable to set up build environment; unrecognized target platform.')

    return Extension(
        name               = '_moxie_core',
        sources            = MOXIE_CORE_COMMON_SOURCE_FILES + MOXIE_CORE_PLATFORM_SOURCES, 
        libraries          = MOXIE_CORE_PLATFORM_LIBRARIES,
        library_dirs       = MOXIE_CORE_COMMON_LIBRARY_DIRS + MOXIE_CORE_PLATFORM_LIBRARY_DIRS,
        include_dirs       = MOXIE_CORE_COMMON_INCLUDE_DIRS + MOXIE_CORE_PLATFORM_INCLUDE_DIRS,
        define_macros      = MOXIE_CORE_COMMON_DEFINES      + MOXIE_CORE_PLATFORM_DEFINES,
        extra_compile_args = MOXIE_CORE_PLATFORM_CCFLAGS,
        extra_link_args    = MOXIE_CORE_PLATFORM_LDFLAGS,
        depends            = MOXIE_CORE_COMMON_HEADER_FILES + MOXIE_CORE_PLATFORM_HEADERS
    )


if not detect_platform():
    raise RuntimeError('Failed to determine the target runtime platform.')

setup(
    name                          = MODULE_NAME,
    version                       = MODULE_VERSION_STRING,
    author                        = MODULE_AUTHOR,
    author_email                  = MODULE_AUTHOR_EMAIL,
    url                           = MODULE_BASE_URL,
    project_urls                  = {
        'Bug Tracker'             : MODULE_ISSUES_URL
    },
    description                   = MODULE_DESCRIPTION,
    long_description              = MODULE_LONG_DESCRIPTION,
    long_description_content_type = 'text/markdown',
    classifiers                   = [
        'Programming Language :: Python :: 3',
        'Programming Language :: C',
        'Programming Language :: Python :: Implementation :: CPython',
        'Development Status :: 3 - Alpha',
        'Intended Audience :: Developers',
        'License :: Other/Proprietary License',
        'Natural Language :: English',
        'Operating System :: MacOS :: MacOS X',
        'Operating System :: Microsoft :: Windows',
        'Operating System :: POSIX :: Linux',
        'Operating System :: POSIX :: Other',
        'Topic :: Scientific/Engineering',
        'Topic :: Software Development :: Libraries'
    ],
    python_requires              = '>=3.8',
    ext_modules                  = [
        make_moxie_core_extension()
    ],
    data_files                   = [
        ('moxie/_moxie_core/include'         , ['moxie/_moxie_core/include/moxie_core.h']),
        ('moxie/_moxie_core/include/internal', ['moxie/_moxie_core/include/internal/memory.h', 'moxie/_moxie_core/include/internal/platform.h', 'moxie/_moxie_core/include/internal/rtloader.h', 'moxie/_moxie_core/include/internal/scheduler.h', 'moxie/_moxie_core/include/internal/version.h'])
    ],
    packages                     = setuptools.find_namespace_packages(include=['moxie','moxie.*'])
)
