project(
  'temp_kiosk',
  'c',
  meson_version : '>= 1.3.0',
  version : '0.1',
  default_options : ['warning_level=3'],
)

dependencies = [
  subproject('crossbg').get_variable('dep'),
]

src = [
  'src/temp_kiosk.c',
]

exe = executable(
  'main',
  src,
  dependencies : dependencies,
  install : true,
)
