project(
  'cross_utils', 
  'c',
  default_options : ['warning_level=2', 'werror=true', 'c_std=gnu99'],
)

cc = meson.get_compiler('c')

# Suppress silly warnings
add_project_arguments(
    cc.get_supported_arguments([
            '-Wno-padded',
            '-Wformat-truncation=1',
            '-Wno-arith-conversion',
            '-Wno-c++-compat',
            '-Wno-sign-conversion',
            '-Wno-sign-compare',
            '-ferror-limit=0',
    ]),
    language : 'c'
)

inc = include_directories('.')

common_src = [
  'utils.c',
]

if host_machine.system() == 'linux'
  platform_src = [
    'linux/linux_process.c',
    'linux/linux_time.c',
    'linux/linux_mem.c',
  ]
else
  platform_src = [
    'win/win_process.c',
    'win/win_time.c',
    'win/win_mem.c',
  ]
endif
    
lib = static_library(
  'cross_utils',
  platform_src,
  common_src,
  include_directories: inc,
)

dep = declare_dependency(
  include_directories: inc,
  link_with: lib,
)

test_exe = executable(
  'test1',
  'tests/test1.c',
  link_with: lib,
)

test('test1', test_exe)
