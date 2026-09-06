#pragma once

#include <QString>
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

// NO.60：数据库密码从环境变量 CHARGING_DB_PASSWORD 读取，未设置时回退实训默认值，
//        避免明文密码硬编码进代码、提交到 Git。
inline QString DbPassword()
{
    return qEnvironmentVariable("CHARGING_DB_PASSWORD", QStringLiteral("123456"));
}
}
