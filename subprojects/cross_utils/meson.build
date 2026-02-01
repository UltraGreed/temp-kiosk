project('cross_utils', 'c')

inc = include_directories('.')

common_src = [
  'utils/files_utils.c',
  'utils/str_utils.c',
]

if host_machine.system() == 'linux'
  platform_src = [
    'linux/linux_bg.c',
    'linux/linux_time.c',
    'linux/linux_mem.c',
  ]
else
  platform_src = [
    'win/win_bg.c',
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
  'test1.c',
  link_with: lib,
)

test('test1', test_exe)
