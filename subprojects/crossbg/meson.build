project('crossbg', 'c')

inc = include_directories('.')

lib = static_library(
  'crossbg',
  'crossbg.c',
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
