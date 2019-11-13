from conans import (
    ConanFile,
    python_requires,
)


b2 = python_requires('b2-helper/0.5.0@grisumbras/stable')


@b2.build_with_b2
class XaosTestConan(ConanFile):
    settings = 'os', 'compiler', 'build_type', 'arch'

    def test(self):
        pass
