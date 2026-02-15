#pragma once

#define LOGGER_SHM_LEN (sizeof(i32) + sizeof(i32))

#ifdef WIN32
#define LOGGER_CLI_CMD ".\\logger_cli.exe"
#define LOGGER_CMD ".\\logger.exe"
#define LOGGER_SHM_NAME "Global\\temp_kiosk_logger"
#define LOGGER_SEM_NAME "Global\\temp_kiosk_logger_sem"
#else
#define LOGGER_CLI_CMD "./logger_cli"
#define LOGGER_CMD "./logger"
#define LOGGER_SHM_NAME "/temp_kiosk_logger"
#define LOGGER_SEM_NAME "/temp_kiosk_logger_sem"
#endif
