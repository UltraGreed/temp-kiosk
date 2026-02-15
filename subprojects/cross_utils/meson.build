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
            '-Wno-format-truncation',
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

if host_machine.system() == 'windows'
  platform_src = [
    'win/win_process.c',
    'win/win_time.c',
    'win/win_mem.c',
    'win/win_socket.c',
  ]
  add_project_arguments(language : 'c')
  sock_dep = cc.find_library('ws2_32', required : true)
else
  platform_src = [
    'posix/posix_process.c',
    'posix/posix_time.c',
    'posix/posix_mem.c',
    'posix/posix_socket.c',
  ]
  sock_dep = []
endif

    
lib = static_library(
  'cross_utils',
  platform_src,
  common_src,
  include_directories: inc,
  dependencies : sock_dep,
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
