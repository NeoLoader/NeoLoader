TEMPLATE = subdirs

module_qtftp_src.subdir = src
module_qtftp_src.target = sub-src

module_qtftp_examples.subdir = examples
module_qtftp_examples.target = sub-examples
module_qtftp_examples.depends = module_qtftp_src
!contains(QT_BUILD_PARTS,examples) {
    module_qtftp_examples.CONFIG += no_default_install no_default_target
}

module_qtftp_tests.subdir = tests
module_qtftp_tests.target = sub-tests
module_qtftp_tests.depends = module_qtftp_src
module_qtftp_tests.CONFIG = no_default_install
!contains(QT_BUILD_PARTS,tests):module_qtftp_tests.CONFIG += no_default_target

SUBDIRS += module_qtftp_src \
           module_qtftp_examples \
           module_qtftp_tests \

