project(
  'temp_kiosk',
  'c',
  meson_version : '>= 1.3.0',
  version : '0.1',
  default_options : ['warning_level=3'],
)

cc = meson.get_compiler('c')
m_dep = cc.find_library('m', required : false)
if m_dep.found()
  add_project_link_arguments('-lm', language : 'c')
endif

dependencies = [
  subproject('cross_utils').get_variable('dep'),
]

src_logger = [
  'src/logger/logger.c',
]

logger_exe = executable(
  'logger',
  src_logger,
  dependencies : dependencies,
  install : true,
)

src_logger_cli = [
  'src/logger/logger_cli.c',
]

logger_cli_exe = executable(
  'logger_cli',
  src_logger_cli,
  dependencies : dependencies,
  install : true,
)

src_temp_emulator = [
  'src/temp_emulator/emulator.c',
]

temp_emulator_exe = executable(
  'emulator',
  src_temp_emulator,
  dependencies : dependencies,
  install : true,
)

temp_logger_src = [
  'src/temp_logger/temp_logger.c',
]

temp_logger_exe = executable(
  'temp_logger',
  temp_logger_src,
  dependencies : dependencies,
  install : true,
)
