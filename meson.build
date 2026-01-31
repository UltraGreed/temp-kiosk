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

src_logger = [
  'src/logger.c',
]


logger_exe = executable(
  'logger',
  src_logger,
  dependencies : dependencies,
  install : true,
)

src_logger_cli = [
  'src/logger_cli.c',
]

logger_cli_exe = executable(
  'logger_cli',
  src_logger_cli,
  dependencies : dependencies,
  install : true,
)
