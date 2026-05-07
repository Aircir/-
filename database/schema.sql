CREATE DATABASE IF NOT EXISTS college_competition_ai
  DEFAULT CHARACTER SET utf8mb4
  DEFAULT COLLATE utf8mb4_unicode_ci;

USE college_competition_ai;

SET NAMES utf8mb4;
SET FOREIGN_KEY_CHECKS = 0;

DROP TABLE IF EXISTS navigation_command;
DROP TABLE IF EXISTS consult_record;
DROP TABLE IF EXISTS search_log;
DROP TABLE IF EXISTS competition_info;
DROP TABLE IF EXISTS competition_category;
DROP TABLE IF EXISTS user_info;

SET FOREIGN_KEY_CHECKS = 1;

CREATE TABLE user_info (
    user_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '用户主键',
    username VARCHAR(50) NOT NULL COMMENT '登录账号',
    password VARCHAR(255) NOT NULL COMMENT '登录密码',
    real_name VARCHAR(50) DEFAULT NULL COMMENT '姓名',
    major VARCHAR(50) DEFAULT NULL COMMENT '专业',
    grade VARCHAR(20) DEFAULT NULL COMMENT '年级',
    user_type TINYINT NOT NULL DEFAULT 1 COMMENT '1 学生，2 管理员',
    status TINYINT NOT NULL DEFAULT 1 COMMENT '1 启用，0 禁用',
    create_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
    update_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '更新时间',
    PRIMARY KEY (user_id),
    UNIQUE KEY uk_user_info_username (username)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='用户表';

CREATE TABLE competition_category (
    category_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '分类主键',
    category_name VARCHAR(64) NOT NULL COMMENT '分类名称',
    category_desc VARCHAR(255) DEFAULT NULL COMMENT '分类描述',
    sort_no INT NOT NULL DEFAULT 0 COMMENT '排序值',
    create_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
    update_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '更新时间',
    PRIMARY KEY (category_id),
    UNIQUE KEY uk_competition_category_name (category_name),
    KEY idx_competition_category_sort_no (sort_no)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='竞赛分类表';

CREATE TABLE competition_info (
    competition_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '竞赛主键',
    competition_name VARCHAR(128) NOT NULL COMMENT '竞赛名称',
    category_id BIGINT UNSIGNED DEFAULT NULL COMMENT '分类主键',
    organizer VARCHAR(128) DEFAULT NULL COMMENT '主办方',
    competition_level VARCHAR(32) DEFAULT NULL COMMENT '赛事级别',
    signup_status VARCHAR(20) NOT NULL DEFAULT 'Open' COMMENT '报名状态',
    suitable_major VARCHAR(128) DEFAULT NULL COMMENT '适合专业',
    suitable_grade VARCHAR(64) DEFAULT NULL COMMENT '适合年级',
    signup_start DATETIME DEFAULT NULL COMMENT '报名开始时间',
    signup_end DATETIME DEFAULT NULL COMMENT '报名结束时间',
    competition_start DATETIME DEFAULT NULL COMMENT '比赛开始时间',
    competition_end DATETIME DEFAULT NULL COMMENT '比赛结束时间',
    official_url VARCHAR(255) DEFAULT NULL COMMENT '官网链接',
    competition_desc TEXT DEFAULT NULL COMMENT '竞赛简介',
    participation_rules TEXT DEFAULT NULL COMMENT '参赛说明',
    display_status TINYINT NOT NULL DEFAULT 1 COMMENT '1 显示，0 隐藏',
    create_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
    update_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '更新时间',
    PRIMARY KEY (competition_id),
    KEY idx_competition_info_category (category_id),
    KEY idx_competition_info_level_status (competition_level, signup_status, display_status),
    CONSTRAINT fk_competition_info_category
        FOREIGN KEY (category_id) REFERENCES competition_category (category_id)
        ON DELETE SET NULL
        ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='竞赛信息表';

CREATE TABLE search_log (
    search_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '搜索记录主键',
    user_id BIGINT UNSIGNED DEFAULT NULL COMMENT '用户主键',
    keyword VARCHAR(128) DEFAULT NULL COMMENT '搜索关键词',
    major VARCHAR(64) DEFAULT NULL COMMENT '专业筛选',
    competition_level VARCHAR(64) DEFAULT NULL COMMENT '赛事级别筛选',
    signup_status VARCHAR(32) DEFAULT NULL COMMENT '报名状态筛选',
    trigger_source VARCHAR(64) DEFAULT NULL COMMENT '触发来源',
    result_count INT NOT NULL DEFAULT 0 COMMENT '结果数量',
    create_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '记录时间',
    PRIMARY KEY (search_id),
    KEY idx_search_log_user_time (user_id, create_time),
    CONSTRAINT fk_search_log_user
        FOREIGN KEY (user_id) REFERENCES user_info (user_id)
        ON DELETE SET NULL
        ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='搜索记录表';

CREATE TABLE consult_record (
    record_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '咨询记录主键',
    user_id BIGINT UNSIGNED DEFAULT NULL COMMENT '用户主键',
    competition_id BIGINT UNSIGNED DEFAULT NULL COMMENT '竞赛主键',
    session_id VARCHAR(64) DEFAULT NULL COMMENT '连续对话会话 ID',
    question_text TEXT NOT NULL COMMENT '用户问题',
    answer TEXT DEFAULT NULL COMMENT 'AI 回复',
    consult_type VARCHAR(32) DEFAULT NULL COMMENT '咨询类型',
    response_mode VARCHAR(16) DEFAULT NULL COMMENT '返回模式',
    prompt_context TEXT DEFAULT NULL COMMENT '提交给 AI 的上下文摘要',
    consult_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '咨询时间',
    PRIMARY KEY (record_id),
    KEY idx_consult_record_user_time (user_id, consult_time),
    KEY idx_consult_record_session_time (session_id, consult_time),
    KEY idx_consult_record_competition_time (competition_id, consult_time),
    CONSTRAINT fk_consult_record_user
        FOREIGN KEY (user_id) REFERENCES user_info (user_id)
        ON DELETE SET NULL
        ON UPDATE CASCADE,
    CONSTRAINT fk_consult_record_competition
        FOREIGN KEY (competition_id) REFERENCES competition_info (competition_id)
        ON DELETE SET NULL
        ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='AI 咨询记录表';

CREATE TABLE navigation_command (
    command_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '导航记录主键',
    user_id BIGINT UNSIGNED DEFAULT NULL COMMENT '用户主键',
    from_page VARCHAR(64) DEFAULT NULL COMMENT '来源页面',
    to_page VARCHAR(64) DEFAULT NULL COMMENT '目标页面',
    action_name VARCHAR(64) DEFAULT NULL COMMENT '触发动作',
    competition_id BIGINT UNSIGNED DEFAULT NULL COMMENT '关联竞赛主键',
    competition_name VARCHAR(128) DEFAULT NULL COMMENT '关联竞赛名称',
    create_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '记录时间',
    PRIMARY KEY (command_id),
    KEY idx_navigation_command_user_time (user_id, create_time),
    CONSTRAINT fk_navigation_command_user
        FOREIGN KEY (user_id) REFERENCES user_info (user_id)
        ON DELETE SET NULL
        ON UPDATE CASCADE,
    CONSTRAINT fk_navigation_command_competition
        FOREIGN KEY (competition_id) REFERENCES competition_info (competition_id)
        ON DELETE SET NULL
        ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='页面导航记录表';
