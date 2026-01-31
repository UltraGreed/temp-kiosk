#pragma once

#define LOGGER_SHM_LEN (sizeof(i32) + sizeof(i32) + sizeof(sem_t))

#ifdef __linux__
#define LOGGER_CLI_CMD "./logger_cli"
#define LOGGER_CMD "./logger"
#else
#define LOGGER_CLI_CMD "./logger_cli.exe"
#define LOGGER_CMD "./logger.exe"
#endif
