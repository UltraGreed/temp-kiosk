project('cross_utils', 'c')

inc = include_directories('.')

if host_machine.system() == 'linux'
  platform_src = [
    'linux_bg.c',
    'linux_time.c',
    'linux_mem.c',
  ]
else
  platform_src = [
    'win_bg.c',
    'win_time.c',
    'win_mem.c',
  ]
endif
    
lib = static_library(
  'cross_utils',
  platform_src,
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
