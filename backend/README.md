# 后端说明

当前目录是本项目第一阶段的可运行后端骨架，用于支撑毕业设计演示版本。

## 当前范围

当前已经完成：

- 基于 Windows 的 C++17 自定义 HTTP 服务
- 基于内存的竞赛演示数据仓库
- `GET /api/health`
- `GET /api/competitions`
- `GET /api/competitions/{id}`
- 已开启 CORS，方便本地前端原型直接访问

当前还未接入：

- MySQL 数据库连接
- 登录接口
- 后台 CRUD
- AI 咨询接口
- SSE 流式输出
- 记录与统计回写

## 接口说明

### `GET /api/health`

健康检查接口，用于确认后端服务是否正常启动。

### `GET /api/competitions`

竞赛列表接口，支持以下查询参数：

- `keyword`
- `major`
- `level` 或 `competition_level`
- `status` 或 `signup_status`
- `grade` 或 `suitable_grade`
- `category` 或 `category_name`
- `category_id` 或 `categoryId`

示例：

```text
http://127.0.0.1:8080/api/competitions?keyword=artificial%20intelligence&status=Warmup
```

### `GET /api/competitions/{id}`

竞赛详情接口。

示例：

```text
http://127.0.0.1:8080/api/competitions/4
```

## 启动方式

### 方式一：使用 PowerShell 启动

在项目根目录执行：

```powershell
cd backend
powershell -ExecutionPolicy Bypass -File .\run-dev.ps1
```

说明：

- 该命令会先自动编译后端，再启动服务。
- 默认端口是 `8080`。
- 启动成功后，可通过 `http://127.0.0.1:8080/api/health` 检查服务状态。

### 方式二：指定端口启动

如果你想换端口，比如 `8090`，执行：

```powershell
cd backend
powershell -ExecutionPolicy Bypass -File .\run-dev.ps1 -Port 8090
```

### 方式三：使用 bat 启动

如果本机 PowerShell 执行策略限制较多，推荐直接用批处理：

```powershell
cd backend
.\run-dev.bat
```

同样也可以带端口参数：

```powershell
cd backend
.\run-dev.bat -Port 8090
```

说明：

- 前端页面默认请求 `http://127.0.0.1:8080`。
- 如果后端不是跑在 `8080`，打开页面时可以在地址后面加上 `apiBase` 参数。
- 例如：

```text
file:///D:/本科毕业设计/system/pages/home.html?apiBase=http://127.0.0.1:8090
```

## 启动后怎么验证

启动成功后，你可以在浏览器里直接打开下面这些地址：

```text
http://127.0.0.1:8080/api/health
http://127.0.0.1:8080/api/competitions
http://127.0.0.1:8080/api/competitions/4
```

也可以测试带筛选条件的列表接口：

```text
http://127.0.0.1:8080/api/competitions?keyword=artificial%20intelligence&status=Warmup
```

## 备注

- 当前后端数据与 `database/seed.sql` 中的演示数据保持一致。
- 这一阶段故意先不接 MySQL 驱动，目的是优先把“后端服务能跑起来”这件事稳定下来。
- 下一步最适合做的是把 `pages/home.html` 和 `pages/competition-detail.html` 接到这两个真实接口上。
