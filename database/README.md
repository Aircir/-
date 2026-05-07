# 数据库说明

当前目录用于存放竞赛智能咨询系统的 MySQL 脚本。

## 文件说明

- `schema.sql`：建库建表脚本
- `seed.sql`：演示数据脚本

## 当前实际落库的表

- `user_info`
- `competition_category`
- `competition_info`
- `search_log`
- `consult_record`
- `navigation_command`

这些表已经覆盖目前后端实际实现的功能：

- 登录与权限
- 竞赛分类管理
- 竞赛信息管理
- AI 咨询记录
- 搜索记录
- 页面导航记录

## 推荐使用顺序

1. 执行 `schema.sql`
2. 执行 `seed.sql`
3. 启动后端服务
4. 访问 `/api/health`，确认 `source` 为 `mysql`

## 演示账号

- 学生：`student / 123456`
- 管理员：`admin / admin123`

## 与后端运行方式的关系

即使你不手动执行脚本，后端启动时也会自动：

- 创建数据库 `college_competition_ai`
- 创建缺失的表
- 写入基础演示账号和竞赛数据
- 对旧版表结构补齐当前代码所需字段

也就是说：

- 手动执行脚本适合答辩前一次性初始化
- 直接启动后端适合日常开发联调

## 说明

- 当前统计页数据主要由 `consult_record`、`search_log`、`navigation_command` 实时聚合得出
- `schema.sql` 和 `seed.sql` 已与当前后端代码保持一致
- 如果你之前导入过旧版脚本，建议重新执行一遍最新脚本，或者直接让后端启动后自动补齐字段
