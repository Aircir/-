# 数据库说明

当前目录用于存放大学生竞赛智能咨询系统的一期数据库脚本，目标是支撑：

- 首页竞赛检索
- 竞赛详情
- 智能咨询
- 自然语言导航
- 记录与统计
- 后台竞赛分类与竞赛信息管理

## 文件说明

- `schema.sql`：建库、建表脚本
- `seed.sql`：演示数据初始化脚本

## 建议使用顺序

1. 执行 `schema.sql`
2. 执行 `seed.sql`

## 数据库命名

- 数据库名：`college_competition_ai`

## 当前核心表

- `user_info`
- `competition_category`
- `competition_info`
- `search_log`
- `consult_record`
- `navigation_command`
- `statistic_summary`

## 与论文设计的关系

表名与论文中已经确认的一期设计保持一致，同时补充了少量工程化字段，用于支撑真实接口开发：

- `competition_info.signup_status`
  说明：便于首页按报名状态筛选

- `search_log.filter_snapshot`
  说明：保存用户完整筛选条件，方便后续统计和复盘

- `consult_record.competition_id`
  说明：把咨询记录和具体竞赛关联起来，便于从详情页进入咨询页后落库

- `consult_record.session_id`
  说明：支撑连续对话

- `navigation_command.action_name`
  说明：前端执行页面动作时更清晰

- `navigation_command.keyword`
  说明：单独记录提取出的检索关键词

- `statistic_summary.summary_label`
  说明：便于前端页面直接展示

## 当前演示数据覆盖

### 用户

- 学生演示账号 `student_demo`
- 学生演示账号 `student_math`
- 管理员演示账号 `admin_demo`

### 竞赛分类

- 程序设计类
- 数学建模类
- 人工智能类
- 创新创业类

### 竞赛信息

- ICPC 国际大学生程序设计竞赛
- 蓝桥杯全国软件和信息技术专业人才大赛
- 全国大学生数学建模竞赛
- 全球人工智能创意挑战赛
- “创客中国”大学生创新创业大赛

### 记录与统计

- 检索记录示例
- 咨询记录示例
- 导航记录示例
- 热门统计示例

## 下一步建议

数据库脚本准备好后，下一步最适合进入：

1. `feat/backend-bootstrap-competition`
2. 先把竞赛列表接口和竞赛详情接口接起来

## Notes

- Optional foreign keys now use `ON DELETE SET NULL` so phase-1 admin CRUD is not blocked by historical logs or statistics.
- `competition_info.category_id` may become `NULL` after a category is deleted. Backend code should treat that case as uncategorized data.
- Demo passwords in `seed.sql` are for local development and defense demos only.
- `signup_status` is constrained to `Draft`, `Warmup`, `Open`, `Closed`, or `Ended`.
