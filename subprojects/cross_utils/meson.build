project('cross_utils', 'c')

inc = include_directories('.')

lib = static_library(
  'cross_utils',
  'cross_bg.c',
  'cross_time.c',
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
