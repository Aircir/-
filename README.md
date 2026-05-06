# 大学生竞赛智能咨询系统

当前仓库已经按“文档”和“页面原型”做了基础整理，便于后续继续接前端、后端和数据库。

## 目录结构

```text
system/
├─ docs/
│  ├─ PRD.md
│  ├─ requirements-summary.md
│  └─ thesis/
│     ├─ thesis.docx
│     └─ 大学生竞赛智能咨询系统-论文初稿.docx
├─ pages/
│  ├─ home.html
│  ├─ competition-detail.html
│  ├─ ai-consult.html
│  ├─ records-statistics.html
│  └─ admin-competition-management.html
├─ .gitignore
└─ README.md
```

## 页面对应关系

- `pages/home.html`：首页 / 竞赛检索页
- `pages/competition-detail.html`：竞赛详情页
- `pages/ai-consult.html`：智能咨询页
- `pages/records-statistics.html`：记录与统计页
- `pages/admin-competition-management.html`：后台管理页

## 文档说明

- `docs/PRD.md`：当前版本产品需求文档
- `docs/requirements-summary.md`：论文功能梳理与一期范围摘要
- `docs/thesis/`：论文相关原始文档

## 当前整理原则

- 需求和论文文档统一放到 `docs/`
- 页面原型统一放到 `pages/`
- 页面文件统一使用英文短名，避免中文、空格和符号影响后续开发

## 建议的后续方向

1. 统一 5 个页面之间的导航跳转
2. 抽离公共样式和公共头部
3. 按 PRD 接入真实数据和接口
4. 再拆分前端与后端目录
