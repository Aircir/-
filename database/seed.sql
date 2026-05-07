USE college_competition_ai;

SET NAMES utf8mb4;
SET FOREIGN_KEY_CHECKS = 0;

TRUNCATE TABLE navigation_command;
TRUNCATE TABLE consult_record;
TRUNCATE TABLE search_log;
TRUNCATE TABLE competition_info;
TRUNCATE TABLE competition_category;
TRUNCATE TABLE user_info;

SET FOREIGN_KEY_CHECKS = 1;

INSERT INTO user_info (
    user_id, username, password, real_name, major, grade, user_type, status, create_time, update_time
) VALUES
    (1, 'student', '123456', '学生演示账号', 'Artificial Intelligence', '2022', 1, 1, '2026-05-01 09:00:00', '2026-05-01 09:00:00'),
    (2, 'admin', 'admin123', '管理员演示账号', NULL, NULL, 2, 1, '2026-05-01 09:05:00', '2026-05-01 09:05:00'),
    (3, 'student_ai', '123456', 'AI 测试账号', 'Artificial Intelligence', '2023', 1, 1, '2026-05-01 09:10:00', '2026-05-01 09:10:00');

INSERT INTO competition_category (
    category_id, category_name, category_desc, sort_no, create_time, update_time
) VALUES
    (1, 'Programming', 'Algorithm, programming and software development competitions', 10, '2026-05-01 10:00:00', '2026-05-01 10:00:00'),
    (2, 'Mathematical Modeling', 'Modeling, statistics and optimization competitions', 20, '2026-05-01 10:00:00', '2026-05-01 10:00:00'),
    (3, 'Artificial Intelligence', 'AI, machine learning and computer vision competitions', 30, '2026-05-01 10:00:00', '2026-05-01 10:00:00'),
    (4, 'Innovation and Entrepreneurship', 'Innovation, startup and project practice competitions', 40, '2026-05-01 10:00:00', '2026-05-01 10:00:00');

INSERT INTO competition_info (
    competition_id, competition_name, category_id, organizer, competition_level, signup_status,
    suitable_major, suitable_grade, signup_start, signup_end, competition_start, competition_end,
    official_url, competition_desc, participation_rules, display_status, create_time, update_time
) VALUES
    (
        1,
        'ICPC International Collegiate Programming Contest',
        1,
        'ICPC Foundation',
        'International',
        'Open',
        'Computer Science, Software Engineering, Artificial Intelligence',
        'Sophomore and above',
        '2026-05-01 08:00:00',
        '2026-10-15 23:59:59',
        '2026-11-10 09:00:00',
        '2026-11-12 18:00:00',
        'https://icpc.global/',
        'ICPC is a well-known global programming contest that emphasizes algorithms, data structures and teamwork.',
        'Usually three students form one team. Strong algorithm and collaboration skills are recommended.',
        1,
        '2026-05-01 10:30:00',
        '2026-05-01 10:30:00'
    ),
    (
        2,
        'Lanqiao Cup Software and Information Technology Competition',
        1,
        'Talent Exchange Center of MIIT',
        'National',
        'Open',
        'Software Engineering, Computer Science, Artificial Intelligence, Electronic Information',
        'Freshman and above',
        '2026-05-10 09:00:00',
        '2026-11-01 23:59:59',
        '2026-12-05 09:00:00',
        '2026-12-05 17:30:00',
        'https://dasai.lanqiao.cn/',
        'Lanqiao Cup focuses on programming, embedded systems, MCU and other software engineering tracks.',
        'It is usually an individual competition and is friendly for students who want to improve through practice and past papers.',
        1,
        '2026-05-01 10:35:00',
        '2026-05-01 10:35:00'
    ),
    (
        3,
        'National College Student Mathematical Contest in Modeling',
        2,
        'China Society for Industrial and Applied Mathematics',
        'National A',
        'Open',
        'Mathematics, Statistics, Computer Science, Engineering',
        'Full-time university students',
        '2026-05-01 09:00:00',
        '2026-09-10 23:59:59',
        '2026-09-20 08:00:00',
        '2026-09-23 20:00:00',
        'https://www.mcm.edu.cn/',
        'This is a highly influential modeling contest in China and emphasizes modeling, coding and paper writing.',
        'Teams usually have three members. It is suitable for students with modeling, data analysis, programming and writing skills.',
        1,
        '2026-05-01 10:40:00',
        '2026-05-01 10:40:00'
    ),
    (
        4,
        'Global AI Innovation Challenge',
        3,
        'International AI Education Alliance',
        'International',
        'Warmup',
        'Artificial Intelligence, Data Science, Software Engineering, Digital Media Technology',
        'Freshman and above',
        '2026-07-01 00:00:00',
        '2026-08-20 23:59:59',
        '2026-09-05 09:00:00',
        '2026-09-07 18:00:00',
        'https://example.org/ai-challenge',
        'This competition emphasizes AI innovation, prototype design and interdisciplinary problem solving.',
        'Cross-major teams are allowed. Teams usually submit a prototype, written proposal and demo video.',
        1,
        '2026-05-01 10:45:00',
        '2026-05-01 10:45:00'
    ),
    (
        5,
        'Maker China University Innovation Competition',
        4,
        'Local industry and university alliance',
        'National',
        'Open',
        'All majors',
        'Undergraduate and postgraduate',
        '2026-05-15 09:00:00',
        '2026-10-30 23:59:59',
        '2026-11-20 09:00:00',
        '2026-11-25 18:00:00',
        'https://example.org/maker-china',
        'An innovation and entrepreneurship competition that highlights project value, teamwork and business presentation.',
        'Both individuals and teams can apply. It is suitable for students with prototypes, pitch decks or startup ideas.',
        1,
        '2026-05-01 10:50:00',
        '2026-05-01 10:50:00'
    );

INSERT INTO search_log (
    search_id, user_id, keyword, major, competition_level, signup_status, trigger_source, result_count, create_time
) VALUES
    (1, 1, 'artificial intelligence', 'Artificial Intelligence', 'International', 'Warmup', 'manual-search', 1, '2026-05-06 14:10:00'),
    (2, 1, 'mathematical modeling', 'Artificial Intelligence', 'National A', 'Open', 'manual-search', 1, '2026-05-06 14:15:00'),
    (3, 3, 'lanqiao', 'Artificial Intelligence', 'National', 'Open', 'quick-search', 1, '2026-05-06 15:20:00');

INSERT INTO consult_record (
    record_id, user_id, competition_id, session_id, question_text, answer, consult_type, response_mode, prompt_context, consult_time
) VALUES
    (
        1, 1, 3, 'session-mcm-001',
        'Is this contest suitable for an artificial intelligence student?',
        'Yes. Although it is not a pure AI contest, it is still very helpful because it strengthens modeling, data analysis and paper writing ability.',
        'general',
        'JSON',
        'competition=National College Student Mathematical Contest in Modeling;major=Artificial Intelligence;grade=2022',
        '2026-05-06 16:00:00'
    ),
    (
        2, 1, 3, 'session-mcm-001',
        'What should I prepare first?',
        'You can start from three directions: modeling basics, Python or MATLAB practice, and team-based paper writing.',
        'general',
        'JSON',
        'previous_session=session-mcm-001',
        '2026-05-06 16:03:00'
    ),
    (
        3, 3, 2, 'session-lanqiao-001',
        'What is the difference between Lanqiao Cup and ICPC?',
        'Lanqiao Cup is more friendly for individual staged training, while ICPC emphasizes teamwork and algorithmic pressure.',
        'general',
        'JSON',
        'competition_a=Lanqiao Cup;competition_b=ICPC',
        '2026-05-06 17:10:00'
    );

INSERT INTO navigation_command (
    command_id, user_id, from_page, to_page, action_name, competition_id, competition_name, create_time
) VALUES
    (1, 1, 'home', 'home', 'open_competition_list', 0, '', '2026-05-06 14:08:00'),
    (2, 1, 'home', 'home', 'search_ai_competition', 4, 'Global AI Innovation Challenge', '2026-05-06 14:09:00'),
    (3, 3, 'home', 'competition-detail', 'open_competition_detail', 2, 'Lanqiao Cup Software and Information Technology Competition', '2026-05-06 15:25:00');
