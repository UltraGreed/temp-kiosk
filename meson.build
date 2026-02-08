project(
  'temp_kiosk',
  'c',
  meson_version : '>= 1.3.0',
  version : '0.1',
  default_options : ['warning_level=everything', 'werror=true', 'c_std=c99'],
)

use_db = true

cc = meson.get_compiler('c')
m_dep = cc.find_library('m', required : false)
if m_dep.found()
  add_project_link_arguments('-lm', language : 'c')
endif

cross_utils_dep = subproject('cross_utils').get_variable('dep')

sqlite3_dep = subproject('sqlite3').get_variable('sqlite3_dep')

src_logger = [
  'src/logger/logger.c',
]

logger_exe = executable(
  'logger',
  src_logger,
  dependencies : cross_utils_dep,
  install : true,
)

src_logger_cli = [
  'src/logger/logger_cli.c',
]

logger_cli_exe = executable(
  'logger_cli',
  src_logger_cli,
  dependencies : cross_utils_dep,
  install : true,
)

src_temp_emulator = [
  'src/temp_emulator/emulator.c',
]

temp_emulator_exe = executable(
  'emulator',
  src_temp_emulator,
  dependencies : cross_utils_dep,
  install : true,
)

temp_logger_src = [
  'src/temp_logger/temp_logger.c',
]

if use_db
  temp_logger_logging_src = 'src/temp_logger/logger_db.c'
else
  temp_logger_logging_src = 'src/temp_logger/logger_fs.c'
endif

temp_logger_exe = executable(
  'temp_logger',
  temp_logger_src,
  temp_logger_logging_src,
  dependencies : [cross_utils_dep, sqlite3_dep],
  install : true,
)

if use_db
  message('Temp logger built with database support.')
else
  message('Temp logger built with filesystem support.')
endif
