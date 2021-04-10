import os

from   distutils.core import Extension
from   distutils.core import setup
from   distutils.util import get_platform

EXTENSION_NAME             = 'moxie'

EXTENSION_AUTHOR           = 'Russell Klenk'
EXTENSION_AUTHOR_EMAIL     = 'contact@russellklenk.com'
EXTENSION_URL              = 'https://github.com/russellklenk/moxie/'

EXTENSION_VERSION_MAJOR    = 0
EXTENSION_VERSION_MINOR    = 1
EXTENSION_VERSION_REVISION = 0
EXTENSION_VERSION_STRING   = '{}.{}.{}'.format(EXTENSION_VERSION_MAJOR, EXTENSION_VERSION_MINOR, EXTENSION_VERSION_REVISION)

EXTENSION_DESCRIPTION      = 'The moxie resource management toolkit'
EXTENSION_LONG_DESCRIPTION = """
moxie is a cross-platform toolkit for management of system resources.
It allows fine-grained control over memory allocation and distribution
of work across hardware threads with semi-automated extraction of 
parallelism using a job scheduler.
"""

IS_WINDOWS            = False
IS_LINUX              = False
IS_MACOS              = False
IS_POWER              = False
IS_UNKNOWN            = False

ROOT_PATH             = os.path.dirname(os.path.abspath(__file__))

COMMON_SOURCES        = [
    '{}/src/memory.c'  .format(ROOT_PATH),
    '{}/src/exttypes.c'.format(ROOT_PATH),
    '{}/src/extfuncs.c'.format(ROOT_PATH)
]

PLATFORM_SOURCES      = [] # Set based on platform detection results.
WINDOWS_SOURCES       = [
    '{}/src/winos/memory_winos.c'   .format(ROOT_PATH),
    '{}/src/winos/scheduler_winos.c'.format(ROOT_PATH)
]
LINUX_SOURCES         = [
    '{}/src/posix/memory_posix.c'   .format(ROOT_PATH),
    '{}/src/posix/scheduler_posix.c'.format(ROOT_PATH)
]
MACOS_SOURCES         = [
    '{}/src/posix/memory_posix.c'   .format(ROOT_PATH),
    '{}/src/posix/scheduler_posix.c'.format(ROOT_PATH)
]
POWER_SOURCES         = [
    '{}/src/posix/memory_posix.c'   .format(ROOT_PATH),
    '{}/src/posix/scheduler_posix.c'.format(ROOT_PATH)
]

WINDOWS_LIBRARIES     = []
LINUX_LIBRARIES       = []
MACOS_LIBRARIES       = []
POWER_LIBRARIES       = []
PLATFORM_LIBRARIES    = [] # Set based on platform detection results.

COMMON_INCLUDE_DIRS   = [
    '{}/include'.format(ROOT_PATH),
    '{}/src'    .format(ROOT_PATH)
]
WINDOWS_INCLUDE_DIRS  = []
LINUX_INCLUDE_DIRS    = []
MACOS_INCLUDE_DIRS    = []
POWER_INCLUDE_DIRS    = []
PLATFORM_INCLUDE_DIRS = [] # Set based on platform detection results.

WINDOWS_LIBRARY_DIRS  = []
LINUX_LIBRARY_DIRS    = []
MACOS_LIBRARY_DIRS    = []
POWER_LIBRARY_DIRS    = []
PLATFORM_LIBRARY_DIRS = [] # Set based on platform detection results.

COMMON_DEFINES        = [
    ('EXTENSION_NAME'            , '{}'.format(EXTENSION_NAME)),
    ('EXTENSION_VERSION_MAJOR'   , '{}'.format(EXTENSION_VERSION_MAJOR)),
    ('EXTENSION_VERSION_MINOR'   , '{}'.format(EXTENSION_VERSION_MINOR)),
    ('EXTENSION_VERSION_REVISION', '{}'.format(EXTENSION_VERSION_REVISION))
]
WINDOWS_DEFINES       = []
LINUX_DEFINES         = [
    ('_GNU_SOURCE'           , '1'),
    ('__STDC_FORMAT_MACROS'  , '1')
]
MACOS_DEFINES         = []
POWER_DEFINES         = []
PLATFORM_DEFINES      = [] # Set based on platform detection results.

WINDOWS_COMPILE_ARGS  = []
LINUX_COMPILE_ARGS    = ["-O2","-pthread","-fstrict-aliasing"]
MACOS_COMPILE_ARGS    = []
POWER_COMPILE_ARGS    = []
PLATFORM_COMPILE_ARGS = [] # Set based on platform detection results.

def detect_platform():
    """
    Attempt to detect the target platform and set one of IS_WINDOWS, IS_LINUX, IS_MACOS, IS_POWER, etc.
    This is really just a hack that will probably need to be substantially extended in the future as the result is used to specify platform-specific libraries, etc.

    Returns
    -------
      `True` if the platform is successfully detected.
    """
    global IS_WINDOWS
    global IS_LINUX
    global IS_MACOS
    global IS_POWER
    global IS_UNKNOWN
    platform: str = get_platform()
    if not platform:
        IS_UNKNOWN = True
        return False

    platform = platform.lower()
    if 'linux' in platform:
        IS_LINUX = True
        if 'ppc64le' in platform or 'power' in platform:
            IS_POWER = True
        return True
    elif 'macos' in platform or 'darwin' in platform:
        IS_MACOS = True
    elif 'win32' in platform or 'win' in platform:
        IS_WINDOWS = True
    else:
        IS_UNKNOWN = True
        return False

    return True


def make_extension(extension_name):
    """
    Set up the build environment based on the current target platform and construct the `distutils.core.Extension` object.

    Parameters
    ----------
      extension_name: A string specifying the name of the extension.

    Returns
    -------
      A `distutils.core.Extension` object defining the extension.
    """
    if not detect_platform():
        raise RuntimeError('Unable to set up build environment; unrecognized target platform.')

    global WINDOWS_DEFINES
    global WINDOWS_SOURCES
    global WINDOWS_LIBRARIES
    global WINDOWS_COMPILE_ARGS
    global WINDOWS_INCLUDE_DIRS
    global WINDOWS_LIBRARY_DIRS

    global LINUX_DEFINES
    global LINUX_SOURCES
    global LINUX_LIBRARIES
    global LINUX_COMPILE_ARGS
    global LINUX_INCLUDE_DIRS
    global LINUX_LIBRARY_DIRS

    global MACOS_DEFINES
    global MACOS_SOURCES
    global MACOS_LIBRARIES
    global MACOS_COMPILE_ARGS
    global MACOS_INCLUDE_DIRS
    global MACOS_LIBRARY_DIRS

    global POWER_DEFINES
    global POWER_SOURCES
    global POWER_LIBRARIES
    global POWER_COMPILE_ARGS
    global POWER_INCLUDE_DIRS
    global POWER_LIBRARY_DIRS

    global PLATFORM_DEFINES
    global PLATFORM_SOURCES
    global PLATFORM_LIBRARIES
    global PLATFORM_COMPILE_ARGS
    global PLATFORM_INCLUDE_DIRS
    global PLATFORM_LIBRARY_DIRS

    if IS_WINDOWS:
        PLATFORM_DEFINES      = WINDOWS_DEFINES
        PLATFORM_SOURCES      = WINDOWS_SOURCES
        PLATFORM_LIBRARIES    = WINDOWS_LIBRARIES
        PLATFORM_COMPILE_ARGS = WINDOWS_COMPILE_ARGS
        PLATFORM_INCLUDE_DIRS = WINDOWS_INCLUDE_DIRS
        PLATFORM_LIBRARY_DIRS = WINDOWS_LIBRARY_DIRS

    elif IS_POWER: # NOTE: Should appear _before_ IS_LINUX check
        PLATFORM_DEFINES      = POWER_DEFINES
        PLATFORM_SOURCES      = POWER_SOURCES
        PLATFORM_LIBRARIES    = POWER_LIBRARIES
        PLATFORM_COMPILE_ARGS = POWER_COMPILE_ARGS
        PLATFORM_INCLUDE_DIRS = POWER_INCLUDE_DIRS
        PLATFORM_LIBRARY_DIRS = POWER_LIBRARY_DIRS

    elif IS_LINUX:
        PLATFORM_DEFINES      = LINUX_DEFINES
        PLATFORM_SOURCES      = LINUX_SOURCES
        PLATFORM_LIBRARIES    = LINUX_LIBRARIES
        PLATFORM_COMPILE_ARGS = LINUX_COMPILE_ARGS
        PLATFORM_INCLUDE_DIRS = LINUX_INCLUDE_DIRS
        PLATFORM_LIBRARY_DIRS = LINUX_LIBRARY_DIRS

    elif IS_MACOS:
        PLATFORM_DEFINES      = MACOS_DEFINES
        PLATFORM_SOURCES      = MACOS_SOURCES
        PLATFORM_LIBRARIES    = MACOS_LIBRARIES
        PLATFORM_COMPILE_ARGS = MACOS_COMPILE_ARGS
        PLATFORM_INCLUDE_DIRS = MACOS_INCLUDE_DIRS
        PLATFORM_LIBRARY_DIRS = MACOS_LIBRARY_DIRS

    else:
        raise RuntimeError('Unable to set up build environment; unrecognized target platform.')

    return Extension(
        name=extension_name,
        sources=COMMON_SOURCES + PLATFORM_SOURCES, 
        libraries=PLATFORM_LIBRARIES,
        include_dirs=COMMON_INCLUDE_DIRS + PLATFORM_INCLUDE_DIRS,
        library_dirs=PLATFORM_LIBRARY_DIRS,
        define_macros=COMMON_DEFINES + PLATFORM_DEFINES,
        extra_compile_args=PLATFORM_COMPILE_ARGS
    )


setup(
    name=EXTENSION_NAME,
    version=EXTENSION_VERSION_STRING,
    description=EXTENSION_DESCRIPTION,
    long_description=EXTENSION_LONG_DESCRIPTION,
    author=EXTENSION_AUTHOR, 
    author_email=EXTENSION_AUTHOR_EMAIL,
    url=EXTENSION_URL,
    ext_modules=[make_extension(extension_name=EXTENSION_NAME)]
)
