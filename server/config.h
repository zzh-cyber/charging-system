#pragma once

#include <QtGlobal>

// 服务器配置。
// 注意：数据库密码此处为实训默认值，正式环境请改为从环境变量/配置文件读取，
//       且不要把真实密码提交到 Git。
namespace ServerConfig {
inline constexpr quint16     ListenPort = 9000;

inline constexpr const char *DbHost     = "127.0.0.1";
inline constexpr int         DbPort     = 3306;
inline constexpr const char *DbName     = "charging_system";
inline constexpr const char *DbUser     = "charging_user";
inline constexpr const char *DbPassword = "123456";
}
