-- MySQL 8.0+
-- College Competition AI System
-- Phase 1 schema

CREATE DATABASE IF NOT EXISTS college_competition_ai
  DEFAULT CHARACTER SET utf8mb4
  DEFAULT COLLATE utf8mb4_unicode_ci;

USE college_competition_ai;

SET NAMES utf8mb4;
SET FOREIGN_KEY_CHECKS = 0;

DROP TABLE IF EXISTS statistic_summary;
DROP TABLE IF EXISTS navigation_command;
DROP TABLE IF EXISTS consult_record;
DROP TABLE IF EXISTS search_log;
DROP TABLE IF EXISTS competition_info;
DROP TABLE IF EXISTS competition_category;
DROP TABLE IF EXISTS user_info;

SET FOREIGN_KEY_CHECKS = 1;

CREATE TABLE user_info (
    user_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT 'User primary key',
    username VARCHAR(50) NOT NULL COMMENT 'Username',
    password VARCHAR(255) NOT NULL COMMENT 'Password hash or demo password',
    real_name VARCHAR(50) DEFAULT NULL COMMENT 'Real name',
    major VARCHAR(50) DEFAULT NULL COMMENT 'Major',
    grade VARCHAR(20) DEFAULT NULL COMMENT 'Grade',
    user_type TINYINT NOT NULL DEFAULT 1 COMMENT '1 student, 2 admin',
    status TINYINT NOT NULL DEFAULT 1 COMMENT '1 active, 0 disabled',
    create_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT 'Created at',
    update_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT 'Updated at',
    PRIMARY KEY (user_id),
    UNIQUE KEY uk_user_info_username (username),
    KEY idx_user_info_user_type_status (user_type, status)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='User table';

CREATE TABLE competition_category (
    category_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT 'Category primary key',
    category_name VARCHAR(64) NOT NULL COMMENT 'Category name',
    category_desc VARCHAR(255) DEFAULT NULL COMMENT 'Category description',
    sort_no INT NOT NULL DEFAULT 0 COMMENT 'Sort number',
    create_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT 'Created at',
    update_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT 'Updated at',
    PRIMARY KEY (category_id),
    UNIQUE KEY uk_competition_category_name (category_name),
    KEY idx_competition_category_sort_no (sort_no)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Competition category table';

CREATE TABLE competition_info (
    competition_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT 'Competition primary key',
    competition_name VARCHAR(128) NOT NULL COMMENT 'Competition name',
    category_id BIGINT UNSIGNED NOT NULL COMMENT 'Category id',
    organizer VARCHAR(128) DEFAULT NULL COMMENT 'Organizer',
    competition_level VARCHAR(32) DEFAULT NULL COMMENT 'Competition level',
    signup_status VARCHAR(20) NOT NULL DEFAULT 'Open' COMMENT 'Signup status',
    suitable_major VARCHAR(128) DEFAULT NULL COMMENT 'Suitable majors',
    suitable_grade VARCHAR(64) DEFAULT NULL COMMENT 'Suitable grades',
    signup_start DATETIME DEFAULT NULL COMMENT 'Signup start time',
    signup_end DATETIME DEFAULT NULL COMMENT 'Signup end time',
    competition_start DATETIME DEFAULT NULL COMMENT 'Competition start time',
    competition_end DATETIME DEFAULT NULL COMMENT 'Competition end time',
    official_url VARCHAR(255) DEFAULT NULL COMMENT 'Official url',
    competition_desc TEXT DEFAULT NULL COMMENT 'Competition description',
    participation_rules TEXT DEFAULT NULL COMMENT 'Participation rules',
    display_status TINYINT NOT NULL DEFAULT 1 COMMENT '1 visible, 0 hidden',
    create_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT 'Created at',
    update_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT 'Updated at',
    PRIMARY KEY (competition_id),
    CONSTRAINT fk_competition_info_category
        FOREIGN KEY (category_id) REFERENCES competition_category (category_id),
    KEY idx_competition_info_category (category_id),
    KEY idx_competition_info_level_status (competition_level, signup_status, display_status),
    KEY idx_competition_info_signup_end (signup_end),
    KEY idx_competition_info_name (competition_name)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Competition info table';

CREATE TABLE search_log (
    search_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT 'Search log primary key',
    user_id BIGINT UNSIGNED DEFAULT NULL COMMENT 'User id',
    keyword VARCHAR(128) DEFAULT NULL COMMENT 'Search keyword',
    category_id BIGINT UNSIGNED DEFAULT NULL COMMENT 'Category filter id',
    filter_text VARCHAR(255) DEFAULT NULL COMMENT 'Filter text',
    filter_snapshot JSON DEFAULT NULL COMMENT 'Filter snapshot json',
    result_count INT NOT NULL DEFAULT 0 COMMENT 'Result count',
    search_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT 'Search time',
    PRIMARY KEY (search_id),
    CONSTRAINT fk_search_log_user
        FOREIGN KEY (user_id) REFERENCES user_info (user_id),
    CONSTRAINT fk_search_log_category
        FOREIGN KEY (category_id) REFERENCES competition_category (category_id),
    KEY idx_search_log_user_time (user_id, search_time),
    KEY idx_search_log_category_time (category_id, search_time)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Search log table';

CREATE TABLE consult_record (
    record_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT 'Consult record primary key',
    user_id BIGINT UNSIGNED DEFAULT NULL COMMENT 'User id',
    competition_id BIGINT UNSIGNED DEFAULT NULL COMMENT 'Related competition id',
    session_id VARCHAR(64) DEFAULT NULL COMMENT 'Conversation session id',
    question_text TEXT NOT NULL COMMENT 'User question',
    answer TEXT DEFAULT NULL COMMENT 'System answer',
    consult_type VARCHAR(32) DEFAULT NULL COMMENT 'Consult type',
    response_mode VARCHAR(16) DEFAULT NULL COMMENT 'JSON or SSE',
    prompt_context TEXT DEFAULT NULL COMMENT 'Prompt summary context',
    consult_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT 'Consult time',
    PRIMARY KEY (record_id),
    CONSTRAINT fk_consult_record_user
        FOREIGN KEY (user_id) REFERENCES user_info (user_id),
    CONSTRAINT fk_consult_record_competition
        FOREIGN KEY (competition_id) REFERENCES competition_info (competition_id),
    KEY idx_consult_record_user_time (user_id, consult_time),
    KEY idx_consult_record_session_time (session_id, consult_time),
    KEY idx_consult_record_competition_time (competition_id, consult_time)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Consult record table';

CREATE TABLE navigation_command (
    command_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT 'Navigation command primary key',
    user_id BIGINT UNSIGNED DEFAULT NULL COMMENT 'User id',
    user_input VARCHAR(255) NOT NULL COMMENT 'Original user input',
    intent_name VARCHAR(64) DEFAULT NULL COMMENT 'Intent name',
    action_name VARCHAR(64) DEFAULT NULL COMMENT 'Action name',
    target_page VARCHAR(64) DEFAULT NULL COMMENT 'Target page',
    keyword VARCHAR(128) DEFAULT NULL COMMENT 'Extracted keyword',
    command_params JSON DEFAULT NULL COMMENT 'Parameters json',
    execute_result VARCHAR(32) DEFAULT NULL COMMENT 'Execution result',
    execute_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT 'Execution time',
    PRIMARY KEY (command_id),
    CONSTRAINT fk_navigation_command_user
        FOREIGN KEY (user_id) REFERENCES user_info (user_id),
    KEY idx_navigation_command_user_time (user_id, execute_time),
    KEY idx_navigation_command_intent_time (intent_name, execute_time)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Navigation command table';

CREATE TABLE statistic_summary (
    summary_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT 'Summary primary key',
    summary_type VARCHAR(32) NOT NULL COMMENT 'Summary type',
    summary_key VARCHAR(128) NOT NULL COMMENT 'Summary key',
    summary_value INT NOT NULL DEFAULT 0 COMMENT 'Summary value',
    summary_label VARCHAR(128) DEFAULT NULL COMMENT 'Summary label',
    user_id BIGINT UNSIGNED DEFAULT NULL COMMENT 'Related user id',
    record_id BIGINT UNSIGNED DEFAULT NULL COMMENT 'Related consult record id',
    search_id BIGINT UNSIGNED DEFAULT NULL COMMENT 'Related search log id',
    command_id BIGINT UNSIGNED DEFAULT NULL COMMENT 'Related command id',
    category_id BIGINT UNSIGNED DEFAULT NULL COMMENT 'Related category id',
    competition_id BIGINT UNSIGNED DEFAULT NULL COMMENT 'Related competition id',
    stats_date DATE DEFAULT NULL COMMENT 'Statistics date',
    update_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT 'Updated at',
    PRIMARY KEY (summary_id),
    CONSTRAINT fk_statistic_summary_user
        FOREIGN KEY (user_id) REFERENCES user_info (user_id),
    CONSTRAINT fk_statistic_summary_record
        FOREIGN KEY (record_id) REFERENCES consult_record (record_id),
    CONSTRAINT fk_statistic_summary_search
        FOREIGN KEY (search_id) REFERENCES search_log (search_id),
    CONSTRAINT fk_statistic_summary_command
        FOREIGN KEY (command_id) REFERENCES navigation_command (command_id),
    CONSTRAINT fk_statistic_summary_category
        FOREIGN KEY (category_id) REFERENCES competition_category (category_id),
    CONSTRAINT fk_statistic_summary_competition
        FOREIGN KEY (competition_id) REFERENCES competition_info (competition_id),
    KEY idx_statistic_summary_type_key (summary_type, summary_key),
    KEY idx_statistic_summary_date (stats_date)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Statistic summary table';
