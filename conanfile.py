from conans import (
    ConanFile,
    python_requires,
    tools,
)
import re


b2 = python_requires('b2-helper/0.6.0@grisumbras/stable')


def get_version():
    try:
        content = tools.load('build.jam')
        match = re.search(r'constant\s*VERSION\s*:\s*(\S+)\s*;', content)
        return match.group(1)
    except:
        pass


@b2.build_with_b2
class XaosConan(ConanFile):
    name = 'xaos'
    version = get_version()
    license = 'BSL-1.0'
    description = 'Chaotic collection of C++ components'
    homepage = 'https://github.com/grisumbras/xaos'
    url = 'https://github.com/grisumbras/xaos'
    topics = 'C++'

    requires = 'boost/1.71.0'

    author = 'Dmitry Arkhipov <grisumbras@gmail.com>'
    default_user = 'grisumbras'
    default_channel = 'testing'

    exports_sources = (
        'LICENSE*',
        'build.jam',
        'include/*',
    )
    no_copy_source = True

    def package_info(self):
        self.info.header_only()
