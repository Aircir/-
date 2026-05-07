# 后端说明

当前目录是竞赛智能咨询系统的 C++ 后端，已经支持：

- 竞赛分类与竞赛信息查询
- 管理员分类/竞赛增删改查
- 登录鉴权
- AI 咨询接口
- 搜索记录、导航记录、咨询记录统计
- MySQL 持久化
- MySQL 不可用时自动回退到本地文件存储

## 启动前准备

需要本机具备以下环境：

- `g++`（支持 C++17）
- MySQL 8.0
- MySQL 开发库目录
  默认读取 `D:\MySQL\MySQL\MySQL Server 8.0`
  如果你的安装目录不同，可以在 `.env.local` 中配置 `MYSQL_ROOT`

## 配置方式

建议先复制一份示例配置：

```powershell
cd backend
Copy-Item .env.example .env.local
```

然后按需填写 `.env.local`：

```text
MYSQL_HOST=127.0.0.1
MYSQL_PORT=3306
MYSQL_USER=root
MYSQL_PASSWORD=你的数据库密码
MYSQL_DATABASE=college_competition_ai

OPENAI_API_KEY=你的密钥
OPENAI_BASE_URL=https://api.openai.com/v1
OPENAI_MODEL=gpt-5.4
OPENAI_API_FORMAT=auto
```

说明：

- `OPENAI_API_FORMAT=auto` 表示优先走 `responses`，失败时自动回退到 `chat/completions`
- 如果你使用的是兼容平台，连续对话报格式错误时，可以直接改成 `OPENAI_API_FORMAT=chat`
- 不配置 `OPENAI_API_KEY` 时，竞赛查询和后台 CRUD 仍可用，但 AI 咨询不可用

## 启动命令

在项目根目录执行：

```powershell
cd backend
powershell -ExecutionPolicy Bypass -File .\run-dev.ps1
```

指定端口：

```powershell
cd backend
powershell -ExecutionPolicy Bypass -File .\run-dev.ps1 -Port 8090
```

如果你更习惯批处理，也可以：

```powershell
cd backend
.\run-dev.bat
```

## 启动后的判断方式

启动成功后访问：

```text
http://127.0.0.1:8080/api/health
```

重点看返回值里的 `source`：

- `mysql`：说明已经连接 MySQL，数据会持久化到数据库
- `local_storage`：说明 MySQL 当前不可用，后端自动回退到了本地文件存储

后端启动时控制台也会输出当前是否成功连接 MySQL。

## 可直接测试的账号

- 学生：`student / 123456`
- 管理员：`admin / admin123`

如果你在 `.env.local` 里改了 `STUDENT_USERNAME`、`STUDENT_PASSWORD`、`ADMIN_USERNAME`、`ADMIN_PASSWORD`，系统会按你的配置生成演示账号。

## 数据库说明

后端启动时会自动创建缺失的数据表，并兼容补齐旧版表结构中缺少的字段。

当前实际使用的核心表有：

- `user_info`
- `competition_category`
- `competition_info`
- `search_log`
- `consult_record`
- `navigation_command`

数据库初始化脚本见：

- `../database/schema.sql`
- `../database/seed.sql`

如果你不想手动导入，也可以直接启动后端，让程序自动建表并写入基础演示数据。

## 常用接口

- `GET /api/health`
- `POST /api/auth/login`
- `GET /api/auth/me`
- `GET /api/categories`
- `POST /api/categories`
- `GET /api/competitions`
- `POST /api/competitions`
- `POST /api/consultations`
- `GET /api/records/dashboard`

## 当前限制

- SSE 流式输出还没有单独实现，当前咨询接口返回标准 JSON
- Session 令牌目前仍保存在后端内存中，重启后需要重新登录
