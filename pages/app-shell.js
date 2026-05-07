(() => {
    const routes = {
        launch: "../index.html",
        home: "home.html",
        competitionList: "home.html#competition-results",
        competitionDetail: "competition-detail.html",
        aiConsult: "ai-consult.html",
        records: "records-statistics.html",
        recordsConsult: "records-statistics.html#consult-records",
        recordsHot: "records-statistics.html#hot-statistics",
        admin: "admin-competition-management.html"
    };

    const DEFAULT_API_BASE = "http://127.0.0.1:8080";
    const STORAGE_KEYS = {
        apiBase: "competitionApiBase",
        lastCompetitionId: "lastCompetitionId",
        authToken: "competitionAuthToken",
        authRole: "competitionAuthRole",
        authDisplayName: "competitionAuthDisplayName",
        authUsername: "competitionAuthUsername"
    };
    const pageName = window.location.pathname.split("/").pop();

    function normalizeBaseUrl(value) {
        return value.replace(/\/+$/, "");
    }

    function getApiBase() {
        const currentUrl = new URL(window.location.href);
        const queryBase = currentUrl.searchParams.get("apiBase");
        if (queryBase) {
            const normalized = normalizeBaseUrl(queryBase);
            window.localStorage.setItem(STORAGE_KEYS.apiBase, normalized);
            return normalized;
        }

        const storedBase = window.localStorage.getItem(STORAGE_KEYS.apiBase);
        if (storedBase) {
            return normalizeBaseUrl(storedBase);
        }

        return DEFAULT_API_BASE;
    }

    const apiBase = getApiBase();

    function getAuthToken() {
        return window.localStorage.getItem(STORAGE_KEYS.authToken) || "";
    }

    function getAuthRole() {
        return window.localStorage.getItem(STORAGE_KEYS.authRole) || "";
    }

    function clearAuthSession() {
        window.localStorage.removeItem(STORAGE_KEYS.authToken);
        window.localStorage.removeItem(STORAGE_KEYS.authRole);
        window.localStorage.removeItem(STORAGE_KEYS.authDisplayName);
        window.localStorage.removeItem(STORAGE_KEYS.authUsername);
    }

    function applyAuthHeaders(headers = {}) {
        const nextHeaders = { ...headers };
        const token = getAuthToken();
        if (token) {
            nextHeaders.Authorization = `Bearer ${token}`;
        }
        return nextHeaders;
    }

    function buildLaunchUrl(params = {}) {
        const url = new URL(routes.launch, window.location.href);
        Object.entries(params).forEach(([key, value]) => {
            if (value !== undefined && value !== null && value !== "") {
                url.searchParams.set(key, value);
            }
        });
        return url.toString();
    }

    function buildApiUrl(path, params = {}) {
        const url = new URL(`${apiBase}${path}`);
        Object.entries(params).forEach(([key, value]) => {
            if (value !== undefined && value !== null && value !== "") {
                url.searchParams.set(key, value);
            }
        });
        return url.toString();
    }

    async function fetchApiJson(path, params = {}) {
        const response = await fetch(buildApiUrl(path, params), {
            method: "GET",
            headers: applyAuthHeaders({
                Accept: "application/json"
            })
        });

        let payload = null;
        try {
            payload = await response.json();
        } catch (error) {
            throw new Error("接口返回的不是合法 JSON。");
        }

        if (!response.ok) {
            const message = payload && payload.message ? payload.message : `请求失败：${response.status}`;
            throw new Error(message);
        }

        if (payload && typeof payload.code === "number" && payload.code !== 0) {
            throw new Error(payload.message || "接口返回异常。");
        }

        return payload;
    }

    async function postApiForm(path, formData = {}, options = {}) {
        const { keepalive = false } = options;
        const body = new URLSearchParams();
        Object.entries(formData).forEach(([key, value]) => {
            if (value !== undefined && value !== null && value !== "") {
                body.set(key, String(value));
            }
        });

        const response = await fetch(buildApiUrl(path), {
            method: "POST",
            headers: applyAuthHeaders({
                Accept: "application/json",
                "Content-Type": "application/x-www-form-urlencoded; charset=UTF-8"
            }),
            body: body.toString(),
            keepalive
        });

        let payload = null;
        try {
            payload = await response.json();
        } catch (error) {
            throw new Error("接口返回的不是有效 JSON。");
        }

        if (!response.ok) {
            throw new Error(payload && payload.message ? payload.message : `请求失败：${response.status}`);
        }

        if (payload && typeof payload.code === "number" && payload.code !== 0) {
            throw new Error(payload.message || "接口返回异常。");
        }

        return payload;
    }

    function currentPageToken() {
        return (pageName || "").replace(/\.html$/i, "") || "unknown";
    }

    function trackSearchRecord(filters = {}, resultCount = 0, triggerSource = "") {
        void postApiForm("/api/records/searches", {
            keyword: filters.keyword || "",
            major: filters.major || "",
            competition_level: filters.competition_level || "",
            signup_status: filters.signup_status || "",
            result_count: resultCount,
            trigger_source: triggerSource || ""
        }, { keepalive: true }).catch(() => {});
    }

    function trackNavigationRecord(toPage, actionName, competitionId = 0) {
        void postApiForm("/api/records/navigation", {
            from_page: currentPageToken(),
            to_page: toPage,
            action_name: actionName,
            competition_id: competitionId || ""
        }, { keepalive: true }).catch(() => {});
    }

    function scrollToHashTarget() {
        const hash = window.location.hash;
        if (!hash) {
            return;
        }

        const target = document.querySelector(hash);
        if (target) {
            target.scrollIntoView({ behavior: "smooth", block: "start" });
        }
    }

    function getQueryParam(name) {
        return new URL(window.location.href).searchParams.get(name);
    }

    function parsePositiveInteger(value) {
        if (!value || !/^\d+$/.test(value)) {
            return null;
        }

        const parsedValue = Number.parseInt(value, 10);
        return Number.isSafeInteger(parsedValue) && parsedValue > 0 ? parsedValue : null;
    }

    function rememberCompetitionId(competitionId) {
        window.sessionStorage.setItem(STORAGE_KEYS.lastCompetitionId, String(competitionId));
    }

    function readRememberedCompetitionId() {
        return parsePositiveInteger(window.sessionStorage.getItem(STORAGE_KEYS.lastCompetitionId));
    }

    function competitionDetailUrl(competitionId) {
        if (!competitionId) {
            return routes.competitionDetail;
        }
        return `${routes.competitionDetail}?id=${encodeURIComponent(String(competitionId))}`;
    }

    function aiConsultUrl(competitionId) {
        if (!competitionId) {
            return routes.aiConsult;
        }
        return `${routes.aiConsult}?competitionId=${encodeURIComponent(String(competitionId))}`;
    }

    function escapeHtml(value) {
        return String(value)
            .replace(/&/g, "&amp;")
            .replace(/</g, "&lt;")
            .replace(/>/g, "&gt;")
            .replace(/"/g, "&quot;")
            .replace(/'/g, "&#39;");
    }

    function formatDate(value) {
        return value ? String(value).slice(0, 10) : "--";
    }

    function formatHost(value) {
        if (!value) {
            return "--";
        }

        try {
            return new URL(value).hostname;
        } catch (error) {
            return value;
        }
    }

    function splitListText(value) {
        return String(value || "")
            .split(",")
            .map((item) => item.trim())
            .filter(Boolean);
    }

    function splitRuleLines(value) {
        return String(value || "")
            .split(/(?:\.\s+|;\s+|\n+)/)
            .map((item) => item.trim())
            .filter(Boolean);
    }

    function trimSnippet(value, limit = 110) {
        const text = String(value || "").trim();
        if (!text) {
            return "暂无竞赛简介。";
        }
        return text.length > limit ? `${text.slice(0, limit - 3)}...` : text;
    }

    function compareDateStrings(left, right) {
        const leftTime = left ? Date.parse(left) : 0;
        const rightTime = right ? Date.parse(right) : 0;
        return leftTime - rightTime;
    }

    function debounce(callback, delay) {
        let timer = null;
        return (...args) => {
            window.clearTimeout(timer);
            timer = window.setTimeout(() => {
                callback(...args);
            }, delay);
        };
    }

    function statusMeta(status) {
        switch (status) {
            case "Open":
                return {
                    label: "报名中",
                    chipClass: "bg-secondary-container text-on-secondary",
                    detailClass: "bg-secondary-container/10 text-secondary"
                };
            case "Warmup":
                return {
                    label: "预热中",
                    chipClass: "bg-primary text-on-primary",
                    detailClass: "bg-primary/10 text-primary"
                };
            case "Closed":
                return {
                    label: "已截止",
                    chipClass: "bg-surface-container-highest text-on-surface-variant",
                    detailClass: "bg-surface-container-highest text-on-surface-variant"
                };
            case "Ended":
                return {
                    label: "已结束",
                    chipClass: "bg-surface-container-highest text-on-surface-variant",
                    detailClass: "bg-surface-container-highest text-on-surface-variant"
                };
            case "Draft":
                return {
                    label: "草稿",
                    chipClass: "bg-surface-container text-on-surface-variant",
                    detailClass: "bg-surface-container text-on-surface-variant"
                };
            default:
                return {
                    label: status || "未设置",
                    chipClass: "bg-surface-container text-on-surface-variant",
                    detailClass: "bg-surface-container text-on-surface-variant"
                };
        }
    }

    function levelMeta(level) {
        return {
            label: level || "未设置级别",
            className: "bg-primary/10 text-primary px-sm py-1 rounded-full font-label-caps text-label-caps"
        };
    }

    function computeSignupProgress(signupStart, signupEnd) {
        const startTime = Date.parse(signupStart || "");
        const endTime = Date.parse(signupEnd || "");

        if (!Number.isFinite(startTime) || !Number.isFinite(endTime) || endTime <= startTime) {
            return {
                percent: 0,
                message: "暂未提供完整报名时间。"
            };
        }

        const currentTime = Date.now();
        if (currentTime <= startTime) {
            return {
                percent: 0,
                message: `报名尚未开始，开始时间：${formatDate(signupStart)}`
            };
        }

        if (currentTime >= endTime) {
            return {
                percent: 100,
                message: `报名已截止，截止时间：${formatDate(signupEnd)}`
            };
        }

        const percent = Math.min(100, Math.max(0, ((currentTime - startTime) / (endTime - startTime)) * 100));
        const leftDays = Math.max(1, Math.ceil((endTime - currentTime) / (24 * 60 * 60 * 1000)));
        return {
            percent,
            message: `报名进行中，预计还剩 ${leftDays} 天`
        };
    }

    function makeBrandClickable() {
        const brand = document.querySelector("header span.text-h1.font-h1") || document.querySelector("header .text-h1.font-h1");
        if (!brand) {
            return;
        }

        brand.classList.add("cursor-pointer");
        brand.setAttribute("title", "返回演示入口");
        brand.addEventListener("click", () => {
            window.location.href = routes.launch;
        });
    }

    function wireTopNav() {
        const navLinks = document.querySelectorAll("header nav a");
        if (navLinks.length < 4) {
            return;
        }

        navLinks[0].setAttribute("href", routes.home);
        navLinks[1].setAttribute("href", routes.aiConsult);
        navLinks[2].setAttribute("href", routes.competitionList);
        navLinks[3].setAttribute("href", routes.records);
    }

    function assignFooterLinks(links, hrefs) {
        hrefs.forEach((href, index) => {
            if (links[index]) {
                links[index].setAttribute("href", href);
            }
        });
    }

    function wireHomePage() {
        const state = {
            rawItems: [],
            commandCategoryName: "",
            navigationBusy: false
        };
        const majorValuesByIndex = ["", "Artificial Intelligence", "Mathematics", "Design", "All majors"];
        const statusValuesByIndex = ["", "Open", "Warmup"];
        const levelValuesByIndex = ["National", "Provincial", "International", "School"];

        const headerActions = document.querySelector("header .flex.items-center.gap-sm");
        if (headerActions && !headerActions.querySelector("[data-shell='admin-entry']")) {
            const adminEntry = document.createElement("a");
            adminEntry.href = routes.admin;
            adminEntry.dataset.shell = "admin-entry";
            adminEntry.className = "rounded-lg border border-outline-variant px-sm py-xs text-body-md text-on-surface-variant hover:border-primary hover:text-primary transition-colors";
            adminEntry.textContent = "管理入口";
            adminEntry.addEventListener("click", (event) => {
                if (getAuthRole() === "admin" && getAuthToken()) {
                    return;
                }

                event.preventDefault();
                window.location.href = buildLaunchUrl({
                    redirect: "pages/admin-competition-management.html",
                    login: "admin"
                });
            });
            headerActions.prepend(adminEntry);
        }

        const commandInput = document.querySelector("section.max-w-3xl input");
        const commandButton = document.querySelector("section.max-w-3xl button");
        const commandSection = document.querySelector("section.max-w-3xl");
        const filterPanel = document.querySelector("aside > div:first-child");
        const resultsSection = document.querySelector("main [class*='md:col-span-9']");
        const resultTitle = resultsSection ? resultsSection.querySelector("h2.font-h1") : null;
        const sortSelect = resultsSection ? resultsSection.querySelector("select") : null;
        const competitionGrid = resultsSection ? resultsSection.querySelector("div.grid.grid-cols-1.lg\\:grid-cols-2.gap-md") : null;
        const pagination = resultsSection ? resultsSection.querySelector("div.flex.justify-center.items-center.gap-sm.pt-xl") : null;

        if (resultsSection) {
            resultsSection.id = "competition-results";
        }

        let feedback = document.getElementById("nl-feedback");
        if (commandSection && !feedback) {
            feedback = document.createElement("p");
            feedback.id = "nl-feedback";
            feedback.className = "mx-sm mt-xs hidden text-body-md";
            commandSection.appendChild(feedback);
        }

        let resultsNotice = resultsSection ? resultsSection.querySelector("[data-shell='results-notice']") : null;
        if (resultsSection && !resultsNotice) {
            resultsNotice = document.createElement("div");
            resultsNotice.dataset.shell = "results-notice";
            resultsNotice.className = "hidden rounded-xl border px-md py-sm text-body-md";
            const titleBar = resultsSection.querySelector("div.flex.justify-between.items-end");
            if (titleBar) {
                titleBar.insertAdjacentElement("afterend", resultsNotice);
            }
        }

        if (!filterPanel || !competitionGrid) {
            return;
        }

        const filterKeywordInput = filterPanel.querySelector("input[type='text']");
        const majorSelect = filterPanel.querySelector("select");
        const statusButtons = Array.from(filterPanel.querySelectorAll("div.flex.flex-wrap.gap-xs button"));
        const levelCheckboxes = Array.from(filterPanel.querySelectorAll("input[type='checkbox']"));
        const footerLinks = document.querySelectorAll("footer a[href='#']");

        assignFooterLinks(footerLinks, [
            routes.competitionList,
            routes.aiConsult,
            routes.recordsHot,
            routes.aiConsult,
            routes.recordsConsult,
            routes.records
        ]);

        function showHomeFeedback(message, isError = false) {
            if (!feedback) {
                return;
            }

            feedback.textContent = message;
            feedback.classList.remove("hidden", "text-primary", "text-error");
            feedback.classList.add(isError ? "text-error" : "text-primary");
        }

        function clearHomeFeedback() {
            if (!feedback) {
                return;
            }

            feedback.textContent = "";
            feedback.classList.add("hidden");
        }

        function showResultsNotice(message, isError = false) {
            if (!resultsNotice) {
                return;
            }

            resultsNotice.textContent = message;
            resultsNotice.className = "rounded-xl border px-md py-sm text-body-md";
            resultsNotice.classList.add(
                isError
                    ? "border-error/30"
                    : "border-primary/20",
                isError
                    ? "bg-error-container/40"
                    : "bg-primary/5",
                isError
                    ? "text-on-error-container"
                    : "text-primary"
            );
        }

        function clearResultsNotice() {
            if (!resultsNotice) {
                return;
            }
            resultsNotice.textContent = "";
            resultsNotice.className = "hidden rounded-xl border px-md py-sm text-body-md";
        }

        function setStatusButtonStyles(activeIndex) {
            statusButtons.forEach((button, index) => {
                button.className = "px-sm py-xs rounded-full text-body-md transition-colors";
                if (index === activeIndex) {
                    button.classList.add("bg-primary", "text-on-primary");
                } else {
                    button.classList.add("bg-surface-container-high", "text-on-surface-variant", "hover:bg-outline-variant");
                }
            });
        }

        function getActiveStatusIndex() {
            const activeIndex = statusButtons.findIndex((button) => button.classList.contains("bg-primary"));
            return activeIndex >= 0 ? activeIndex : 0;
        }

        function setMajorByValue(value) {
            if (!majorSelect) {
                return;
            }

            const nextIndex = majorValuesByIndex.findIndex((item) => item === value);
            majorSelect.selectedIndex = nextIndex >= 0 ? nextIndex : 0;
        }

        function setStatusByValue(value) {
            const nextIndex = statusValuesByIndex.findIndex((item) => item === value);
            setStatusButtonStyles(nextIndex >= 0 ? nextIndex : 0);
        }

        function setLevelByValue(value) {
            const nextIndex = levelValuesByIndex.findIndex((item) => item === value);
            levelCheckboxes.forEach((checkbox, index) => {
                checkbox.checked = nextIndex >= 0 && index === nextIndex;
            });
        }

        function resetHomeFilters() {
            if (filterKeywordInput) {
                filterKeywordInput.value = "";
            }
            setMajorByValue("");
            setStatusByValue("");
            setLevelByValue("");
            state.commandCategoryName = "";
        }

        function clearCommandCategoryFilter() {
            state.commandCategoryName = "";
        }

        function clearCardHighlights() {
            competitionGrid.querySelectorAll("[data-shell-highlight='true']").forEach((card) => {
                card.removeAttribute("data-shell-highlight");
                card.classList.remove("ring-2", "ring-secondary", "ring-offset-2", "ring-offset-background", "scale-[1.01]");
            });
        }

        function applyHomeHashAction() {
            clearCardHighlights();

            if (window.location.hash === "#competition-results" && resultsSection) {
                resultsSection.scrollIntoView({ behavior: "smooth", block: "start" });
                return;
            }

            if (window.location.hash !== "#ai-competition-card") {
                return;
            }

            const aiCard = document.getElementById("ai-competition-card");
            if (!aiCard) {
                showHomeFeedback("当前结果中没有人工智能类竞赛。", true);
                return;
            }

            aiCard.dataset.shellHighlight = "true";
            aiCard.classList.add("ring-2", "ring-secondary", "ring-offset-2", "ring-offset-background", "scale-[1.01]");
            aiCard.scrollIntoView({ behavior: "smooth", block: "center" });
            showHomeFeedback("已为你定位到人工智能类竞赛。");
        }

        function readHomeFilters() {
            const activeLevelIndex = levelCheckboxes.findIndex((checkbox) => checkbox.checked);
            return {
                keyword: filterKeywordInput ? filterKeywordInput.value.trim() : "",
                major: majorSelect ? majorValuesByIndex[majorSelect.selectedIndex] || "" : "",
                signup_status: statusValuesByIndex[getActiveStatusIndex()] || "",
                competition_level: activeLevelIndex >= 0 ? levelValuesByIndex[activeLevelIndex] || "" : "",
                category_name: state.commandCategoryName || ""
            };
        }

        function sortCompetitions(items) {
            const sortedItems = [...items];
            const sortMode = sortSelect ? sortSelect.selectedIndex : 0;

            if (sortMode === 1) {
                sortedItems.sort((left, right) => compareDateStrings(left.signup_end, right.signup_end));
            } else if (sortMode === 2) {
                sortedItems.sort((left, right) => compareDateStrings(right.signup_start, left.signup_start));
            }

            return sortedItems;
        }

        function renderCompetitionCard(competition, index) {
            const status = statusMeta(competition.signup_status);
            const tags = [competition.category_name, competition.competition_level]
                .filter(Boolean)
                .map((tag) => `<span class="px-xs py-[2px] bg-surface-container text-on-surface-variant text-label-caps rounded">${escapeHtml(tag)}</span>`)
                .join("");
            const cardId =
                competition.category_name === "Artificial Intelligence"
                    ? "ai-competition-card"
                    : competition.competition_id === 1
                        ? "icpc-card"
                        : competition.competition_id === 2
                            ? "lanqiao-card"
                            : `competition-card-${competition.competition_id}`;
            const cardClasses = index === 0
                ? "bg-surface-container-lowest rounded-xl p-md border border-outline-variant/30 shadow-sm hover:shadow-md transition-all group"
                : "bg-surface-container-lowest rounded-xl p-md border border-outline-variant/30 shadow-sm hover:shadow-md transition-all group";

            return `
                <article id="${cardId}" data-competition-id="${competition.competition_id}" class="${cardClasses}">
                    <div class="flex justify-between items-start mb-sm">
                        <span class="${status.chipClass} px-sm py-base rounded-full font-label-caps text-label-caps">${escapeHtml(status.label)}</span>
                        <span class="material-symbols-outlined text-outline group-hover:text-primary cursor-pointer" data-icon="bookmark">bookmark</span>
                    </div>
                    <h4 class="font-h1 text-h1 text-on-surface mb-xs">${escapeHtml(competition.competition_name)}</h4>
                    <div class="flex flex-wrap gap-xs mb-sm">${tags}</div>
                    <p class="text-body-md text-on-surface-variant leading-relaxed min-h-[72px]">${escapeHtml(trimSnippet(competition.competition_desc))}</p>
                    <div class="flex items-center justify-between mt-auto pt-md border-t border-surface-container">
                        <div class="flex items-center gap-xs text-body-md text-on-surface-variant">
                            <span class="material-symbols-outlined text-[18px]" data-icon="calendar_today">calendar_today</span>
                            <span>截止日期: ${escapeHtml(formatDate(competition.signup_end))}</span>
                        </div>
                        <button data-detail-trigger="true" class="text-primary font-h2 flex items-center gap-xs group/btn">
                            详情 <span class="material-symbols-outlined text-[18px] group-hover/btn:translate-x-1 transition-transform" data-icon="arrow_forward">arrow_forward</span>
                        </button>
                    </div>
                </article>
            `;
        }

        function renderCompetitionList(items) {
            if (resultTitle) {
                resultTitle.textContent = `搜索结果 (${items.length})`;
            }

            if (pagination) {
                pagination.classList.add("hidden");
            }

            if (!items.length) {
                competitionGrid.innerHTML = `
                    <div class="lg:col-span-2 rounded-xl border border-dashed border-outline-variant bg-surface-container-lowest p-xl text-center text-on-surface-variant">
                        <p class="font-h2 text-h2 text-on-surface mb-sm">暂无匹配的竞赛结果</p>
                        <p class="text-body-md">可以尝试更换关键词、专业或赛事级别后重新筛选。</p>
                    </div>
                `;
                return;
            }

            competitionGrid.innerHTML = sortCompetitions(items)
                .map((competition, index) => renderCompetitionCard(competition, index))
                .join("");

            competitionGrid.querySelectorAll("[data-detail-trigger='true']").forEach((button) => {
                button.classList.add("cursor-pointer");
                button.addEventListener("click", (event) => {
                    const card = event.currentTarget.closest("[data-competition-id]");
                    const competitionId = parsePositiveInteger(card ? card.getAttribute("data-competition-id") : "");
                    if (!competitionId) {
                        return;
                    }
                    rememberCompetitionId(competitionId);
                    trackNavigationRecord("competition-detail", "open_competition_detail", competitionId);
                    window.location.href = competitionDetailUrl(competitionId);
                });
            });
        }

        async function loadCompetitions(options = {}) {
            const { feedbackMessage = "", preserveNotice = false, trackSource = "" } = options;
            const filters = readHomeFilters();

            if (!preserveNotice) {
                showResultsNotice(`正在从 ${apiBase} 加载竞赛数据...`);
            }

            try {
                const payload = await fetchApiJson("/api/competitions", filters);
                const items = payload && payload.data && Array.isArray(payload.data.items) ? payload.data.items : [];
                state.rawItems = items;
                renderCompetitionList(items);
                if (trackSource) {
                    trackSearchRecord(filters, items.length, trackSource);
                }
                clearResultsNotice();
                if (feedbackMessage) {
                    showHomeFeedback(feedbackMessage);
                }
                applyHomeHashAction();
            } catch (error) {
                showResultsNotice(`加载竞赛数据失败：${error.message}。请先启动 backend/run-dev.bat。`, true);
                if (feedbackMessage) {
                    showHomeFeedback(feedbackMessage, true);
                }
            }
        }

        const debouncedReload = debounce(() => {
            window.location.hash = "competition-results";
            void loadCompetitions({ trackSource: "keyword_input" });
        }, 300);

        async function requestNavigationIntent(command) {
            const payload = await postApiForm("/api/navigation/interpret", {
                command
            });
            return payload && payload.data ? payload.data : null;
        }

        async function applyNavigationIntent(intent, originalCommand) {
            const action = intent && intent.action ? String(intent.action) : "unknown";
            const feedbackMessage = intent && intent.feedback_message
                ? String(intent.feedback_message)
                : "已解析你的指令。";

            if (action === "open_competition_list") {
                resetHomeFilters();
                window.location.hash = "competition-results";
                await loadCompetitions({ feedbackMessage, trackSource: "command_nl_open_list" });
                return;
            }

            if (action === "search_competition") {
                const keyword = String(intent.keyword || intent.competition_name || "").trim();
                if (filterKeywordInput) {
                    filterKeywordInput.value = keyword;
                }
                setMajorByValue(String(intent.major || "").trim());
                setStatusByValue(String(intent.signup_status || "").trim());
                setLevelByValue(String(intent.competition_level || "").trim());
                state.commandCategoryName = String(intent.category_name || "").trim();
                window.location.hash = "competition-results";
                await loadCompetitions({ feedbackMessage, trackSource: "command_nl_search" });
                return;
            }

            if (action === "open_competition_detail" && Number(intent.competition_id) > 0) {
                const competitionId = Number(intent.competition_id);
                rememberCompetitionId(competitionId);
                trackNavigationRecord("competition-detail", "open_competition_detail", competitionId);
                window.location.href = competitionDetailUrl(competitionId);
                return;
            }

            if (action === "open_ai_consult" && Number(intent.competition_id) > 0) {
                const competitionId = Number(intent.competition_id);
                rememberCompetitionId(competitionId);
                trackNavigationRecord("ai-consult", "open_ai_consult", competitionId);
                window.location.href = aiConsultUrl(competitionId);
                return;
            }

            if (action === "open_records") {
                trackNavigationRecord("records-statistics", "open_records");
                window.location.href = routes.records;
                return;
            }

            if (action === "view_consult_records") {
                trackNavigationRecord("records-statistics", "view_consult_records");
                window.location.href = routes.recordsConsult;
                return;
            }

            if (action === "view_hot_stats") {
                trackNavigationRecord("records-statistics", "view_hot_stats");
                window.location.href = routes.recordsHot;
                return;
            }

            if (action === "open_admin") {
                trackNavigationRecord("admin-competition-management", "open_admin");
                if (getAuthRole() === "admin" && getAuthToken()) {
                    window.location.href = routes.admin;
                } else {
                    window.location.href = buildLaunchUrl({
                        redirect: "pages/admin-competition-management.html",
                        login: "admin"
                    });
                }
                return;
            }

            if (filterKeywordInput) {
                filterKeywordInput.value = originalCommand;
            }
            clearCommandCategoryFilter();
            setMajorByValue("");
            setStatusByValue("");
            setLevelByValue("");
            window.location.hash = "competition-results";
            await loadCompetitions({
                feedbackMessage: feedbackMessage || `已按关键词“${originalCommand}”执行搜索。`,
                trackSource: "command_nl_fallback"
            });
        }

        if (commandButton) {
            commandButton.addEventListener("click", async () => {
                const command = commandInput ? commandInput.value.trim() : "";
                if (state.navigationBusy) {
                    return;
                }
                if (!command) {
                    clearHomeFeedback();
                    showHomeFeedback("可以尝试输入：打开竞赛列表页、搜索人工智能类竞赛、打开蓝桥杯详情。");
                    return;
                }

                state.navigationBusy = true;
                commandButton.disabled = true;
                showHomeFeedback("正在理解你的指令...");

                try {
                    const intent = await requestNavigationIntent(command);
                    await applyNavigationIntent(intent, command);
                } catch (error) {
                    if (filterKeywordInput) {
                        filterKeywordInput.value = command;
                    }
                    clearCommandCategoryFilter();
                    setMajorByValue("");
                    setStatusByValue("");
                    setLevelByValue("");
                    window.location.hash = "competition-results";
                    await loadCompetitions({
                        feedbackMessage: `导航理解失败，已按关键词“${command}”执行搜索。`,
                        trackSource: "command_nl_error"
                    });
                    showHomeFeedback(`导航理解失败：${error.message}`, true);
                } finally {
                    state.navigationBusy = false;
                    commandButton.disabled = false;
                }
            });
        }

        if (commandInput) {
            commandInput.addEventListener("keydown", (event) => {
                if (event.key === "Enter") {
                    event.preventDefault();
                    if (commandButton) {
                        commandButton.click();
                    }
                }
            });
        }

        if (filterKeywordInput) {
            filterKeywordInput.addEventListener("input", () => {
                clearHomeFeedback();
                clearCommandCategoryFilter();
                debouncedReload();
            });
        }

        if (majorSelect) {
            majorSelect.addEventListener("change", () => {
                clearHomeFeedback();
                clearCommandCategoryFilter();
                window.location.hash = "competition-results";
                void loadCompetitions({ trackSource: "major_filter" });
            });
        }

        statusButtons.forEach((button, index) => {
            button.addEventListener("click", () => {
                setStatusButtonStyles(index);
                clearHomeFeedback();
                clearCommandCategoryFilter();
                window.location.hash = "competition-results";
                void loadCompetitions({ trackSource: "status_filter" });
            });
        });

        levelCheckboxes.forEach((checkbox, index) => {
            checkbox.addEventListener("change", () => {
                levelCheckboxes.forEach((currentCheckbox, currentIndex) => {
                    if (currentIndex !== index) {
                        currentCheckbox.checked = false;
                    }
                });

                clearHomeFeedback();
                clearCommandCategoryFilter();
                window.location.hash = "competition-results";
                void loadCompetitions({ trackSource: "level_filter" });
            });
        });

        if (sortSelect) {
            sortSelect.addEventListener("change", () => {
                renderCompetitionList(state.rawItems);
                applyHomeHashAction();
            });
        }

        setStatusButtonStyles(0);
        window.addEventListener("hashchange", applyHomeHashAction);
        void loadCompetitions({ preserveNotice: false });
    }

    function wireCompetitionDetailPage() {
        const backButton = document.querySelector("main .mb-sm button");
        const detailMain = document.querySelector("main");
        const overviewSection = document.querySelector("main > section.bg-surface-container-lowest");
        const leftColumn = document.querySelector("main .col-span-12.lg\\:col-span-4.space-y-gutter");
        const rightColumn = document.querySelector("main .col-span-12.lg\\:col-span-8.space-y-gutter");
        const footerLinks = document.querySelectorAll("footer a[href='#']");

        assignFooterLinks(footerLinks, [routes.launch, routes.records, routes.aiConsult]);

        if (backButton) {
            backButton.classList.add("cursor-pointer");
            backButton.addEventListener("click", () => {
                trackNavigationRecord("home", "back_to_competition_list");
                window.location.href = routes.competitionList;
            });
        }

        if (!detailMain || !overviewSection || !leftColumn || !rightColumn) {
            return;
        }

        let detailNotice = detailMain.querySelector("[data-shell='detail-notice']");
        if (!detailNotice) {
            detailNotice = document.createElement("div");
            detailNotice.dataset.shell = "detail-notice";
            detailNotice.className = "hidden mb-md rounded-xl border px-md py-sm text-body-md";
            const backWrapper = detailMain.querySelector(".mb-sm");
            if (backWrapper) {
                backWrapper.insertAdjacentElement("afterend", detailNotice);
            }
        }

        function showDetailNotice(message, isError = false) {
            if (!detailNotice) {
                return;
            }

            detailNotice.textContent = message;
            detailNotice.className = "mb-md rounded-xl border px-md py-sm text-body-md";
            detailNotice.classList.add(
                isError ? "border-error/30" : "border-primary/20",
                isError ? "bg-error-container/40" : "bg-primary/5",
                isError ? "text-on-error-container" : "text-primary"
            );
        }

        function clearDetailNotice() {
            if (!detailNotice) {
                return;
            }
            detailNotice.textContent = "";
            detailNotice.className = "hidden mb-md rounded-xl border px-md py-sm text-body-md";
        }

        const overviewHeader = overviewSection.querySelector("div.flex.flex-wrap.items-center.gap-sm.mb-xs");
        const overviewTitle = overviewHeader ? overviewHeader.querySelector("h1") : null;
        const overviewBadges = overviewHeader ? overviewHeader.querySelectorAll("span") : [];
        const overviewMeta = overviewSection.querySelector("p.font-body-lg");
        const overviewConsultButton = overviewSection.querySelector("button");
        const leftCards = Array.from(leftColumn.children);
        const rightCards = Array.from(rightColumn.children);
        const registrationCard = leftCards[0] || null;
        const eligibilityCard = leftCards[1] || null;
        const officialCard = leftCards[2] || null;
        const introCard = rightCards[0] || null;
        const requirementsCard = rightCards[1] || null;
        const teaserCard = rightCards[2] || null;

        function renderCompetitionDetail(competition) {
            document.title = `${competition.competition_name} - 竞赛详情`;

            if (overviewTitle) {
                overviewTitle.textContent = competition.competition_name;
            }

            if (overviewBadges[0]) {
                const level = levelMeta(competition.competition_level);
                overviewBadges[0].className = level.className;
                overviewBadges[0].textContent = level.label;
            }

            if (overviewBadges[1]) {
                const status = statusMeta(competition.signup_status);
                overviewBadges[1].className = `${status.detailClass} px-sm py-1 rounded-full font-label-caps text-label-caps`;
                overviewBadges[1].textContent = status.label;
            }

            if (overviewMeta) {
                overviewMeta.textContent = `分类：${competition.category_name || "--"} / 主办方：${competition.organizer || "--"}`;
            }

                if (overviewConsultButton) {
                    overviewConsultButton.classList.add("cursor-pointer");
                    overviewConsultButton.addEventListener("click", () => {
                        trackNavigationRecord("ai-consult", "open_ai_consult", competition.competition_id);
                        window.location.href = aiConsultUrl(competition.competition_id);
                    }, { once: true });
                }

            if (registrationCard) {
                const valueSpans = registrationCard.querySelectorAll(".flex.justify-between.items-center span:last-child");
                if (valueSpans[0]) {
                    valueSpans[0].textContent = formatDate(competition.signup_start);
                }
                if (valueSpans[1]) {
                    valueSpans[1].textContent = formatDate(competition.signup_end);
                }

                const progressBar = registrationCard.querySelector(".w-full.bg-surface-container.rounded-full > div");
                const summary = registrationCard.querySelector("p.text-right");
                const progress = computeSignupProgress(competition.signup_start, competition.signup_end);
                if (progressBar) {
                    progressBar.style.width = `${Math.round(progress.percent)}%`;
                }
                if (summary) {
                    summary.textContent = progress.message;
                }
            }

            if (eligibilityCard) {
                const majorContainer = eligibilityCard.querySelector(".flex.flex-wrap.gap-xs");
                const gradeText = eligibilityCard.querySelector("div.pt-sm p");
                const majorTags = splitListText(competition.suitable_major);

                if (majorContainer) {
                    majorContainer.innerHTML = majorTags.length
                        ? majorTags
                            .map((major) => `<span class="bg-surface-container px-sm py-1 rounded text-body-md text-on-surface">${escapeHtml(major)}</span>`)
                            .join("")
                        : `<span class="bg-surface-container px-sm py-1 rounded text-body-md text-on-surface">不限专业</span>`;
                }

                if (gradeText) {
                    gradeText.textContent = competition.suitable_grade || "未限制年级";
                }
            }

            if (officialCard) {
                const officialLink = officialCard.querySelector("a");
                if (officialLink) {
                    const textNode = officialLink.querySelector("span");
                    officialLink.setAttribute("href", competition.official_url || "#");
                    officialLink.setAttribute("target", "_blank");
                    officialLink.setAttribute("rel", "noreferrer");
                    if (textNode) {
                        textNode.textContent = formatHost(competition.official_url);
                    }
                }
            }

            if (introCard) {
                const introParagraphs = introCard.querySelectorAll("div.font-body-lg p");
                if (introParagraphs[0]) {
                    introParagraphs[0].textContent = competition.competition_desc || "暂无竞赛简介。";
                }
                if (introParagraphs[1]) {
                    introParagraphs[1].textContent = `主办方：${competition.organizer || "--"}。报名时间：${formatDate(competition.signup_start)} 至 ${formatDate(competition.signup_end)}。`;
                }
            }

            if (requirementsCard) {
                const featureCards = requirementsCard.querySelectorAll(".grid.grid-cols-1.md\\:grid-cols-2.gap-md > div");
                const listHeading = requirementsCard.querySelector("h3.font-h2");
                const bulletList = requirementsCard.querySelector("ul");

                if (featureCards[0]) {
                    const title = featureCards[0].querySelector("h4");
                    const body = featureCards[0].querySelector("p");
                    if (title) {
                        title.textContent = "参赛说明";
                    }
                    if (body) {
                        body.textContent = competition.participation_rules || "暂无参赛规则说明。";
                    }
                }

                if (featureCards[1]) {
                    const title = featureCards[1].querySelector("h4");
                    const body = featureCards[1].querySelector("p");
                    if (title) {
                        title.textContent = "适合对象";
                    }
                    if (body) {
                        body.textContent = `适合专业：${competition.suitable_major || "不限"}；适合年级：${competition.suitable_grade || "不限"}`;
                    }
                }

                if (listHeading) {
                    listHeading.textContent = "关键信息";
                }

                if (bulletList) {
                    const bullets = [
                        `赛事分类：${competition.category_name || "--"}`,
                        `赛事级别：${competition.competition_level || "--"}`,
                        `报名状态：${statusMeta(competition.signup_status).label}`,
                        `报名时间：${formatDate(competition.signup_start)} - ${formatDate(competition.signup_end)}`
                    ].concat(splitRuleLines(competition.participation_rules).slice(0, 3));

                    bulletList.innerHTML = bullets
                        .map((item) => `<li>${escapeHtml(item)}</li>`)
                        .join("");
                }
            }

            if (teaserCard) {
                const teaserTitle = teaserCard.querySelector("h3");
                const teaserParagraphs = teaserCard.querySelectorAll("p");
                const teaserButton = teaserCard.querySelector("button");

                if (teaserTitle) {
                    teaserTitle.textContent = "还不确定是否参赛？";
                }

                if (teaserParagraphs[0]) {
                    teaserParagraphs[0].textContent = `${competition.competition_name} 已接入咨询上下文，可以继续追问适合度、准备建议和备赛重点。`;
                }

                if (teaserParagraphs[1]) {
                    teaserParagraphs[1].textContent = `我想了解 ${competition.competition_name} 是否适合我的专业，以及应该先准备什么。`;
                }

                if (teaserButton) {
                    teaserButton.classList.add("cursor-pointer");
                    teaserButton.addEventListener("click", () => {
                        trackNavigationRecord("ai-consult", "open_ai_consult", competition.competition_id);
                        window.location.href = aiConsultUrl(competition.competition_id);
                    }, { once: true });
                }
            }
        }

        async function loadCompetitionDetail() {
            const competitionId = parsePositiveInteger(getQueryParam("id")) || readRememberedCompetitionId() || 3;
            rememberCompetitionId(competitionId);
            showDetailNotice(`正在从 ${apiBase} 加载竞赛详情...`);

            try {
                const payload = await fetchApiJson(`/api/competitions/${competitionId}`);
                const competition = payload && payload.data ? payload.data : null;
                if (!competition) {
                    throw new Error("未获取到竞赛详情数据。");
                }

                renderCompetitionDetail(competition);
                clearDetailNotice();
            } catch (error) {
                showDetailNotice(`加载竞赛详情失败：${error.message}。请先启动 backend/run-dev.bat。`, true);
            }
        }

        void loadCompetitionDetail();
    }

    function wireAiConsultPage() {
        const competitionId = parsePositiveInteger(getQueryParam("competitionId")) || readRememberedCompetitionId() || 3;
        rememberCompetitionId(competitionId);

        const pageTitle = document.querySelector("header span.text-h1.font-h1");
        const conversationPanel = document.querySelector("main .col-span-12.lg\\:col-span-8");
        const messagesContainer = conversationPanel ? conversationPanel.querySelector(".flex-1.overflow-y-auto.p-md.space-y-md") : null;
        const inputPanel = conversationPanel ? conversationPanel.querySelector(".p-md.bg-surface-container-low.border-t") : null;
        const exampleButtonsWrap = inputPanel ? inputPanel.querySelector(".flex.flex-wrap.gap-xs.mb-md") : null;
        const modeSelect = inputPanel ? inputPanel.querySelector("select") : null;
        const textarea = inputPanel ? inputPanel.querySelector("textarea") : null;
        const sendButton = inputPanel ? inputPanel.querySelector("button.bg-primary") : null;
        const contextCard = document.querySelector("aside > div:first-child");
        const insightCard = document.querySelector("aside > div:last-child");

        if (!conversationPanel || !messagesContainer || !inputPanel || !textarea || !sendButton) {
            return;
        }

        const storageKeyPrefix = `consultation:${competitionId}`;
        const state = {
            competitionId,
            competition: null,
            sessionId: window.sessionStorage.getItem(`${storageKeyPrefix}:sessionId`) || "",
            messages: [],
            exampleQuestions: [],
            followUpQuestions: [],
            welcomeMessage: "",
            mode: modeSelect && modeSelect.selectedIndex === 1 ? "normal" : "stream",
            sending: false,
            loading: true,
            loadingMessage: "正在加载竞赛咨询上下文...",
            errorMessage: "",
            draftAssistantText: "",
            draftVisible: false
        };

        function requestConsultApiJson(path, options = {}) {
            const {
                method = "GET",
                params = {},
                formData = null
            } = options;

            const headers = {
                Accept: "application/json"
            };
            const requestOptions = {
                method
            };

            if (formData) {
                const body = new URLSearchParams();
                Object.entries(formData).forEach(([key, value]) => {
                    if (value !== undefined && value !== null) {
                        body.set(key, String(value));
                    }
                });
                headers["Content-Type"] = "application/x-www-form-urlencoded; charset=UTF-8";
                requestOptions.body = body.toString();
            }

            requestOptions.headers = applyAuthHeaders(headers);

            return fetch(buildApiUrl(path, params), requestOptions).then(async (response) => {
                let payload = null;
                try {
                    payload = await response.json();
                } catch (error) {
                    throw new Error("接口返回的不是有效 JSON。");
                }

                if (!response.ok) {
                    throw new Error(payload && payload.message ? payload.message : `请求失败：${response.status}`);
                }

                if (payload && typeof payload.code === "number" && payload.code !== 0) {
                    throw new Error(payload.message || "接口返回异常。");
                }

                return payload;
            });
        }

        async function requestConsultStream(path, formData, handlers = {}) {
            const body = new URLSearchParams();
            Object.entries(formData || {}).forEach(([key, value]) => {
                if (value !== undefined && value !== null) {
                    body.set(key, String(value));
                }
            });

            const response = await fetch(buildApiUrl(path), {
                method: "POST",
                headers: applyAuthHeaders({
                    Accept: "text/event-stream",
                    "Content-Type": "application/x-www-form-urlencoded; charset=UTF-8"
                }),
                body: body.toString()
            });

            if (!response.ok) {
                const rawText = await response.text();
                try {
                    const payload = JSON.parse(rawText);
                    throw new Error(payload && payload.message ? payload.message : `请求失败：${response.status}`);
                } catch (error) {
                    if (error instanceof Error && error.message) {
                        throw error;
                    }
                    throw new Error(rawText || `请求失败：${response.status}`);
                }
            }

            if (!response.body) {
                throw new Error("当前浏览器环境不支持流式响应。");
            }

            const reader = response.body.getReader();
            const decoder = new TextDecoder("utf-8");
            let buffer = "";

            const consumeEventBlock = (block) => {
                const lines = block.split("\n");
                let eventName = "message";
                const dataLines = [];

                lines.forEach((line) => {
                    const normalizedLine = line.trim();
                    if (!normalizedLine) {
                        return;
                    }
                    if (normalizedLine.startsWith("event:")) {
                        eventName = normalizedLine.slice(6).trim() || "message";
                    } else if (normalizedLine.startsWith("data:")) {
                        dataLines.push(normalizedLine.slice(5).trim());
                    }
                });

                if (!dataLines.length) {
                    return;
                }

                const payloadText = dataLines.join("\n");
                let payload = null;
                try {
                    payload = JSON.parse(payloadText);
                } catch (error) {
                    payload = { text: payloadText };
                }

                if (eventName === "start" && typeof handlers.onStart === "function") {
                    handlers.onStart(payload);
                } else if (eventName === "delta" && typeof handlers.onDelta === "function") {
                    handlers.onDelta(payload);
                } else if (eventName === "done" && typeof handlers.onDone === "function") {
                    handlers.onDone(payload);
                } else if (eventName === "error") {
                    const message = payload && payload.message ? payload.message : "咨询失败。";
                    throw new Error(message);
                }
            };

            while (true) {
                const { value, done } = await reader.read();
                buffer += decoder.decode(value || new Uint8Array(), { stream: !done });
                buffer = buffer.replace(/\r\n/g, "\n");

                let separatorIndex = buffer.indexOf("\n\n");
                while (separatorIndex >= 0) {
                    const block = buffer.slice(0, separatorIndex);
                    buffer = buffer.slice(separatorIndex + 2);
                    if (block.trim()) {
                        consumeEventBlock(block);
                    }
                    separatorIndex = buffer.indexOf("\n\n");
                }

                if (done) {
                    break;
                }
            }

            if (buffer.trim()) {
                consumeEventBlock(buffer);
            }
        }

        function messageStorageKey() {
            return `${storageKeyPrefix}:messages`;
        }

        function persistConversation() {
            window.sessionStorage.setItem(`${storageKeyPrefix}:sessionId`, state.sessionId || "");
            window.sessionStorage.setItem(messageStorageKey(), JSON.stringify(state.messages));
        }

        function restoreConversation() {
            const raw = window.sessionStorage.getItem(messageStorageKey());
            if (!raw) {
                return;
            }

            try {
                const parsed = JSON.parse(raw);
                if (Array.isArray(parsed)) {
                    state.messages = parsed.filter((item) => item && item.role && item.content).map((item) => ({
                        role: item.role,
                        content: String(item.content),
                        createdAt: item.createdAt || item.create_time || ""
                    }));
                }
            } catch (error) {
                state.messages = [];
            }
        }

        function normalizeConversationMessages(messages) {
            if (!Array.isArray(messages)) {
                return [];
            }

            return messages.filter((item) => item && item.role && item.content).map((item) => ({
                role: String(item.role),
                content: String(item.content),
                createdAt: item.createdAt || item.create_time || ""
            }));
        }

        function buildHistoryPayload() {
            return state.messages.map((message) => ({
                role: message.role,
                content: message.content,
                create_time: message.createdAt || ""
            }));
        }

        function resetConversationWithWelcome() {
            state.messages = state.welcomeMessage
                ? [{
                    role: "assistant",
                    content: state.welcomeMessage,
                    createdAt: ""
                }]
                : [];
            persistConversation();
        }

        function activeExampleQuestions() {
            return state.followUpQuestions.length ? state.followUpQuestions : state.exampleQuestions;
        }

        function autoResizeTextarea() {
            textarea.style.height = "auto";
            textarea.style.height = `${Math.min(160, Math.max(44, textarea.scrollHeight))}px`;
        }

        function roleBubbleMarkup(message) {
            const isAssistant = message.role === "assistant";
            const wrapperClass = isAssistant ? "justify-start" : "justify-end";
            const bubbleClass = isAssistant
                ? "bg-surface-container bubble-ai"
                : "bg-surface-container-high bubble-user";
            const iconShell = isAssistant
                ? "bg-primary-container text-on-primary-container"
                : "bg-secondary-container text-on-secondary-container";
            const iconName = isAssistant ? "smart_toy" : "person";
            const contentHtml = escapeHtml(message.content).replace(/\n/g, "<br>");

            return `
                <div class="flex items-start gap-sm ${wrapperClass}">
                    ${isAssistant ? `
                        <div class="w-8 h-8 rounded-lg ${iconShell} flex items-center justify-center text-[20px] shrink-0">
                            <span class="material-symbols-outlined text-[20px]">${iconName}</span>
                        </div>
                    ` : ""}
                    <div class="max-w-[85%] ${bubbleClass} p-md rounded-2xl text-on-surface font-body-md whitespace-normal">
                        ${contentHtml}
                    </div>
                    ${isAssistant ? "" : `
                        <div class="w-8 h-8 rounded-lg ${iconShell} flex items-center justify-center text-[20px] shrink-0">
                            <span class="material-symbols-outlined text-[20px]">${iconName}</span>
                        </div>
                    `}
                </div>
            `;
        }

        function draftAssistantMarkup() {
            if (!state.draftVisible) {
                return "";
            }

            if (state.draftAssistantText) {
                return roleBubbleMarkup({
                    role: "assistant",
                    content: state.draftAssistantText
                });
            }

            return `
                <div class="flex items-start gap-sm">
                    <div class="w-8 h-8 rounded-lg bg-primary-container flex items-center justify-center text-on-primary-container shrink-0">
                        <span class="material-symbols-outlined text-[20px]">smart_toy</span>
                    </div>
                    <div class="max-w-[85%] bg-primary-container/10 p-md rounded-2xl text-on-surface font-body-md bubble-ai border border-primary/20">
                        <p class="mb-sm">AI 正在结合当前竞赛信息实时生成回复...</p>
                        <div class="flex items-center gap-xs text-primary font-label-caps">
                            <span class="w-2 h-2 bg-primary rounded-full animate-pulse"></span>
                            <span class="w-2 h-2 bg-primary rounded-full animate-pulse delay-75"></span>
                            <span class="w-2 h-2 bg-primary rounded-full animate-pulse delay-150"></span>
                            <span class="ml-xs">生成中...</span>
                        </div>
                    </div>
                </div>
            `;
        }

        function renderMessages() {
            const notice = state.errorMessage
                ? `<div class="rounded-xl border border-error/30 bg-error-container/40 px-md py-sm text-body-md text-on-error-container">${escapeHtml(state.errorMessage)}</div>`
                : "";

            const loading = state.loading
                ? `<div class="rounded-xl border border-primary/20 bg-primary/5 px-md py-sm text-body-md text-primary">${escapeHtml(state.loadingMessage)}</div>`
                : "";

            messagesContainer.innerHTML = `
                ${notice}
                ${loading}
                ${state.messages.map((message) => roleBubbleMarkup(message)).join("")}
                ${draftAssistantMarkup()}
            `;

            messagesContainer.scrollTop = messagesContainer.scrollHeight;
        }

        function renderExampleButtons() {
            if (!exampleButtonsWrap) {
                return;
            }

            exampleButtonsWrap.innerHTML = activeExampleQuestions().map((question) => `
                <button type="button" class="px-sm py-xs bg-surface-container-lowest border border-outline-variant rounded-full text-body-md text-on-surface-variant hover:border-primary hover:text-primary transition-all">
                    ${escapeHtml(question)}
                </button>
            `).join("");

            exampleButtonsWrap.querySelectorAll("button").forEach((button) => {
                button.addEventListener("click", () => {
                    textarea.value = button.textContent.trim();
                    autoResizeTextarea();
                    textarea.focus();
                });
            });
        }

        function renderContextCard() {
            if (!contextCard || !state.competition) {
                return;
            }

            contextCard.innerHTML = `
                <h3 class="font-h2 text-h2 text-on-surface mb-md flex items-center gap-xs">
                    <span class="material-symbols-outlined text-primary">info</span>
                    当前咨询背景
                </h3>
                <div class="space-y-sm">
                    <div>
                        <span class="font-label-caps text-label-caps text-on-surface-variant block mb-xs">竞赛名称</span>
                        <p class="font-body-md text-body-md text-on-surface">${escapeHtml(state.competition.competition_name || "--")}</p>
                    </div>
                    <div class="flex gap-sm">
                        <div class="flex-1">
                            <span class="font-label-caps text-label-caps text-on-surface-variant block mb-xs">赛事级别</span>
                            <span class="inline-flex items-center px-sm py-1 bg-primary/10 text-primary text-label-caps font-label-caps rounded-full">${escapeHtml(levelMeta(state.competition.competition_level).label)}</span>
                        </div>
                        <div class="flex-1">
                            <span class="font-label-caps text-label-caps text-on-surface-variant block mb-xs">报名状态</span>
                            <span class="inline-flex items-center px-sm py-1 bg-secondary-container/20 text-on-secondary-container text-label-caps font-label-caps rounded-full">${escapeHtml(statusMeta(state.competition.signup_status).label)}</span>
                        </div>
                    </div>
                    <div>
                        <span class="font-label-caps text-label-caps text-on-surface-variant block mb-xs">适合专业</span>
                        <p class="font-body-md text-body-md text-on-surface">${escapeHtml(state.competition.suitable_major || "不限专业")}</p>
                    </div>
                    <div>
                        <span class="font-label-caps text-label-caps text-on-surface-variant block mb-xs">报名时间</span>
                        <p class="font-body-md text-body-md text-on-surface">${escapeHtml(formatDate(state.competition.signup_start))} - ${escapeHtml(formatDate(state.competition.signup_end))}</p>
                    </div>
                </div>
                <div class="mt-md pt-md border-t border-outline-variant/30">
                    <button class="w-full py-sm bg-surface-container-high text-on-surface font-h2 text-h2 rounded-xl hover:bg-surface-container-highest transition-all flex items-center justify-center gap-xs">
                        查看完整竞赛详情
                        <span class="material-symbols-outlined">open_in_new</span>
                    </button>
                </div>
            `;

            const detailButton = contextCard.querySelector("button");
            if (detailButton) {
                detailButton.classList.add("cursor-pointer");
                detailButton.addEventListener("click", () => {
                    trackNavigationRecord("competition-detail", "back_to_competition_detail", state.competitionId);
                    window.location.href = competitionDetailUrl(state.competitionId);
                });
            }
        }

        function renderInsightCard() {
            if (!insightCard || !state.competition) {
                return;
            }

            insightCard.innerHTML = `
                <h3 class="font-h2 text-h2 text-on-surface mb-md flex items-center gap-xs">
                    <span class="material-symbols-outlined text-primary">tips_and_updates</span>
                    提问建议
                </h3>
                <div class="space-y-md">
                    <div class="rounded-xl bg-surface-container p-md">
                        <p class="text-label-caps font-label-caps text-on-surface-variant mb-xs">会话状态</p>
                        <p class="font-body-md text-body-md text-on-surface">
                            已关联 ${escapeHtml(state.competition.category_name || "竞赛")} 上下文，当前对话消息 ${escapeHtml(String(state.messages.length))} 条。
                        </p>
                    </div>
                    <div class="rounded-xl bg-surface-container-low p-md">
                        <p class="text-label-caps font-label-caps text-on-surface-variant mb-sm">建议继续追问</p>
                        <div class="space-y-sm">
                            ${activeExampleQuestions().map((question) => `
                                <button type="button" class="w-full text-left rounded-xl border border-outline-variant/30 bg-surface-container-lowest px-sm py-sm text-body-md text-on-surface hover:border-primary hover:text-primary transition-colors">
                                    ${escapeHtml(question)}
                                </button>
                            `).join("")}
                        </div>
                    </div>
                    <p class="font-body-md text-body-md text-on-surface-variant leading-relaxed">
                        当前问答会优先结合竞赛名称、级别、报名状态、适合专业、参赛规则和简介生成建议，也会延续当前会话上下文继续追问。
                    </p>
                </div>
            `;

            insightCard.querySelectorAll("button").forEach((button) => {
                button.addEventListener("click", () => {
                    textarea.value = button.textContent.trim();
                    autoResizeTextarea();
                    textarea.focus();
                });
            });
        }

        function renderAll() {
            if (pageTitle) {
                pageTitle.textContent = state.competition ? `${state.competition.competition_name} 咨询` : "竞赛智能咨询";
            }

            renderExampleButtons();
            renderMessages();
            renderContextCard();
            renderInsightCard();
        }

        function pushMessage(role, content) {
            state.messages.push({
                role,
                content,
                createdAt: new Date().toISOString()
            });
            persistConversation();
        }

        async function animateAssistantReply(answer, finalMessages) {
            state.draftVisible = true;
            state.draftAssistantText = "";
            renderMessages();

            if (state.mode !== "stream") {
                state.draftVisible = false;
                state.draftAssistantText = "";
                if (Array.isArray(finalMessages) && finalMessages.length) {
                    state.messages = normalizeConversationMessages(finalMessages);
                    persistConversation();
                } else {
                    pushMessage("assistant", answer);
                }
                renderAll();
                return;
            }

            const step = Math.max(4, Math.floor(answer.length / 30));
            for (let index = 0; index < answer.length; index += step) {
                state.draftAssistantText = answer.slice(0, index + step);
                renderMessages();
                await new Promise((resolve) => window.setTimeout(resolve, 35));
            }

            state.draftVisible = false;
            state.draftAssistantText = "";
            if (Array.isArray(finalMessages) && finalMessages.length) {
                state.messages = normalizeConversationMessages(finalMessages);
                persistConversation();
            } else {
                pushMessage("assistant", answer);
            }
            renderAll();
        }

        async function sendQuestion() {
            const question = textarea.value.trim();
            if (!question || state.sending || !state.competition) {
                return;
            }

            const historyMessages = buildHistoryPayload();
            state.errorMessage = "";
            state.sending = true;
            pushMessage("user", question);
            textarea.value = "";
            autoResizeTextarea();
            state.draftVisible = true;
            state.draftAssistantText = "";
            renderMessages();

            try {
                if (state.mode === "stream") {
                    let finalPayload = null;

                    await requestConsultStream("/api/consultations/stream", {
                        competition_id: state.competitionId,
                        session_id: state.sessionId,
                        history: JSON.stringify(historyMessages),
                        question
                    }, {
                        onStart: (payload) => {
                            state.sessionId = payload && payload.session_id ? String(payload.session_id) : state.sessionId;
                            window.sessionStorage.setItem(`${storageKeyPrefix}:sessionId`, state.sessionId || "");
                        },
                        onDelta: (payload) => {
                            const text = payload && payload.text ? String(payload.text) : "";
                            if (!text) {
                                return;
                            }
                            state.draftVisible = true;
                            state.draftAssistantText += text;
                            renderMessages();
                        },
                        onDone: (payload) => {
                            finalPayload = payload;
                            state.sessionId = payload && payload.session_id ? String(payload.session_id) : state.sessionId;
                            state.followUpQuestions = payload && Array.isArray(payload.follow_up_questions)
                                ? payload.follow_up_questions
                                : [];
                            state.draftVisible = false;
                            state.draftAssistantText = "";

                            if (payload && Array.isArray(payload.messages) && payload.messages.length) {
                                state.messages = normalizeConversationMessages(payload.messages);
                                persistConversation();
                            } else if (payload && payload.answer) {
                                pushMessage("assistant", String(payload.answer));
                            }

                            renderAll();
                        }
                    });

                    if (!finalPayload) {
                        throw new Error("流式响应未正常结束。");
                    }
                } else {
                    const payload = await requestConsultApiJson("/api/consultations", {
                        method: "POST",
                        formData: {
                            competition_id: state.competitionId,
                            session_id: state.sessionId,
                            history: JSON.stringify(historyMessages),
                            question,
                            response_mode: state.mode
                        }
                    });

                    const data = payload && payload.data ? payload.data : null;
                    if (!data || !data.answer) {
                        throw new Error("没有获取到问答结果。");
                    }

                    state.sessionId = data.session_id || state.sessionId;
                    state.followUpQuestions = Array.isArray(data.follow_up_questions) ? data.follow_up_questions : [];
                    window.sessionStorage.setItem(`${storageKeyPrefix}:sessionId`, state.sessionId || "");
                    await animateAssistantReply(data.answer, data.messages);
                }
            } catch (error) {
                state.draftVisible = false;
                state.draftAssistantText = "";
                state.errorMessage = `咨询失败：${error.message}`;
                renderMessages();
            } finally {
                if (state.errorMessage) {
                    state.messages = normalizeConversationMessages(historyMessages);
                    persistConversation();
                }
                state.sending = false;
            }
        }

        async function loadConsultationContext() {
            state.loading = true;
            state.loadingMessage = "正在加载竞赛咨询上下文...";
            state.errorMessage = "";
            renderMessages();

            try {
                const payload = await requestConsultApiJson("/api/consultations/context", {
                    method: "GET",
                    params: {
                        competition_id: state.competitionId
                    }
                });

                const data = payload && payload.data ? payload.data : null;
                if (!data || !data.competition) {
                    throw new Error("没有获取到竞赛咨询上下文。");
                }

                state.competition = data.competition;
                state.welcomeMessage = data.welcome_message || "";
                state.exampleQuestions = Array.isArray(data.example_questions) ? data.example_questions : [];
                state.followUpQuestions = [];
                restoreConversation();
                if (!state.messages.length) {
                    resetConversationWithWelcome();
                }
                document.title = `${state.competition.competition_name} - 智能咨询`;
            } catch (error) {
                state.errorMessage = `加载咨询上下文失败：${error.message}。请先启动 backend/run-dev.bat。`;
            } finally {
                state.loading = false;
                renderAll();
            }
        }

        if (modeSelect) {
            modeSelect.addEventListener("change", () => {
                state.mode = modeSelect.selectedIndex === 1 ? "normal" : "stream";
            });
        }

        textarea.addEventListener("input", autoResizeTextarea);
        textarea.addEventListener("keydown", (event) => {
            if (event.key === "Enter" && !event.shiftKey) {
                event.preventDefault();
                void sendQuestion();
            }
        });

        sendButton.classList.add("cursor-pointer");
        sendButton.addEventListener("click", () => {
            void sendQuestion();
        });

        autoResizeTextarea();
        renderAll();
        void loadConsultationContext();
    }

    function wireRecordsPage() {
        const statsSection = document.querySelector("section.grid.grid-cols-1.md\\:grid-cols-4");
        if (statsSection) {
            statsSection.id = "hot-statistics";
        }

        const recordsSection = document.querySelector("section.lg\\:col-span-8.space-y-md");
        if (recordsSection) {
            recordsSection.id = "consult-records";
        }

        const footerLinks = document.querySelectorAll("footer a[href='#']");
        assignFooterLinks(footerLinks, [routes.launch, routes.home, routes.aiConsult]);

        const asideSection = document.querySelector("aside.lg\\:col-span-4.space-y-md");
        if (!statsSection || !recordsSection || !asideSection) {
            scrollToHashTarget();
            return;
        }

        const state = {
            loading: true,
            errorMessage: "",
            overview: {
                total_sessions: 0,
                total_messages: 0,
                competition_total: 0,
                hot_category_total: 0
            },
            items: [],
            hotCategories: []
        };

        function formatDateTime(value) {
            if (!value) {
                return "--";
            }

            return String(value).slice(0, 16).replace("T", " ");
        }

        function renderStatsSection() {
            const overview = state.overview || {};
            const cards = [
                {
                    label: "咨询会话数",
                    icon: "chat",
                    value: overview.total_sessions || 0,
                    hint: state.loading ? "正在同步最新记录..." : "AI 咨询已沉淀为会话记录"
                },
                {
                    label: "累计消息数",
                    icon: "forum",
                    value: overview.total_messages || 0,
                    hint: "统计用户提问与 AI 回复总数"
                },
                {
                    label: "涉及竞赛数",
                    icon: "emoji_events",
                    value: overview.competition_total || 0,
                    hint: "有咨询行为的竞赛数量"
                },
                {
                    label: "热门分类数",
                    icon: "insights",
                    value: overview.hot_category_total || 0,
                    hint: "已形成咨询热度分布"
                }
            ];

            statsSection.innerHTML = cards.map((card) => `
                <div class="bg-surface-container-lowest p-md rounded-xl shadow-sm border border-outline-variant/10">
                    <div class="flex items-center justify-between mb-sm">
                        <span class="text-label-caps font-label-caps text-on-surface-variant uppercase">${escapeHtml(card.label)}</span>
                        <span class="material-symbols-outlined text-primary">${card.icon}</span>
                    </div>
                    <p class="text-display font-display text-on-surface">${escapeHtml(String(card.value))}</p>
                    <p class="text-body-md text-on-surface-variant mt-xs">${escapeHtml(card.hint)}</p>
                </div>
            `).join("");
        }

        function renderRecordsSection() {
            const statusNotice = state.loading
                ? `<div class="rounded-xl border border-primary/20 bg-primary/5 px-md py-sm text-body-md text-primary">正在加载咨询记录...</div>`
                : state.errorMessage
                    ? `<div class="rounded-xl border border-error/30 bg-error-container/40 px-md py-sm text-body-md text-on-error-container">${escapeHtml(state.errorMessage)}</div>`
                    : "";

            const recordsHtml = state.items.length
                ? state.items.map((item) => `
                    <button type="button" data-record-competition-id="${escapeHtml(String(item.competition_id || 0))}" class="w-full flex items-start gap-md p-sm hover:bg-surface-container-low rounded-lg transition-all group text-left">
                        <div class="bg-primary-container/10 p-2 rounded-lg">
                            <span class="material-symbols-outlined text-primary">smart_toy</span>
                        </div>
                        <div class="flex-1 min-w-0">
                            <div class="flex justify-between items-start gap-sm">
                                <div>
                                    <h4 class="font-h2 text-body-md text-on-surface">${escapeHtml(item.latest_question || `${item.competition_name} 咨询记录`)}</h4>
                                    <p class="text-label-caps text-on-surface-variant mt-1">${escapeHtml(item.competition_name || "未命名竞赛")} · ${escapeHtml(item.category_name || "未分类")}</p>
                                </div>
                                <span class="text-label-caps text-on-surface-variant shrink-0">${escapeHtml(formatDateTime(item.update_time))}</span>
                            </div>
                            <p class="text-body-md text-on-surface-variant mt-2 line-clamp-2">${escapeHtml(trimSnippet(item.latest_answer || item.latest_question || "暂无咨询内容。", 120))}</p>
                            <p class="text-label-caps text-primary mt-2">消息数 ${escapeHtml(String(item.message_count || 0))}</p>
                        </div>
                        <span class="material-symbols-outlined text-on-surface-variant opacity-0 group-hover:opacity-100 transition-opacity shrink-0">chevron_right</span>
                    </button>
                `).join("")
                : `
                    <div class="rounded-xl border border-dashed border-outline-variant/40 bg-surface-container-low px-md py-lg text-center">
                        <p class="text-h2 text-on-surface mb-sm">还没有咨询记录</p>
                        <p class="text-body-md text-on-surface-variant mb-md">先去竞赛详情页或 AI 咨询页提几个问题，这里就会自动生成记录。</p>
                        <a href="${routes.home}" class="inline-flex items-center gap-xs px-md py-sm rounded-lg bg-primary text-on-primary font-h2 text-body-md hover:opacity-90 transition-opacity">
                            去首页开始咨询
                            <span class="material-symbols-outlined">arrow_forward</span>
                        </a>
                    </div>
                `;

            recordsSection.innerHTML = `
                <div class="bg-surface-container-lowest rounded-xl shadow-sm border border-outline-variant/10 overflow-hidden">
                    <div class="flex border-b border-outline-variant/10 px-md">
                        <button class="px-md py-sm font-h2 text-body-md text-primary border-b-2 border-primary">咨询记录</button>
                        <button class="px-md py-sm font-h2 text-body-md text-on-surface-variant/60 cursor-not-allowed" disabled>搜索记录</button>
                        <button class="px-md py-sm font-h2 text-body-md text-on-surface-variant/60 cursor-not-allowed" disabled>导航记录</button>
                    </div>
                    <div class="p-md space-y-sm">
                        ${statusNotice}
                        ${recordsHtml}
                    </div>
                    <div class="p-md text-center border-t border-outline-variant/10 text-body-md text-on-surface-variant">
                        当前展示 ${escapeHtml(String(state.items.length))} 条最新咨询会话
                    </div>
                </div>
            `;

            recordsSection.querySelectorAll("[data-record-competition-id]").forEach((button) => {
                button.addEventListener("click", () => {
                    const competitionId = parsePositiveInteger(button.getAttribute("data-record-competition-id"));
                    if (!competitionId) {
                        return;
                    }
                    rememberCompetitionId(competitionId);
                    trackNavigationRecord("ai-consult", "open_record_consult", competitionId);
                    window.location.href = aiConsultUrl(competitionId);
                });
            });
        }

        function renderAsideSection() {
            const totalSessions = Number(state.overview.total_sessions || 0);
            const palette = ["bg-primary", "bg-secondary-container", "bg-tertiary", "bg-surface-container-highest"];

            const categoryRows = state.hotCategories.length
                ? state.hotCategories.slice(0, 4).map((item, index) => {
                    const percentage = totalSessions > 0
                        ? Math.max(5, Math.round((Number(item.session_count || 0) / totalSessions) * 100))
                        : 0;

                    return `
                        <div class="space-y-xs">
                            <div class="flex items-center justify-between">
                                <div class="flex items-center gap-2 min-w-0">
                                    <div class="w-3 h-3 rounded-full ${palette[index % palette.length]}"></div>
                                    <span class="text-body-md text-on-surface truncate">${escapeHtml(item.category_name || "未分类")}</span>
                                </div>
                                <span class="text-body-md font-semibold shrink-0">${escapeHtml(String(item.session_count || 0))} 次</span>
                            </div>
                            <div class="h-2 rounded-full bg-surface-container-high overflow-hidden">
                                <div class="h-full ${palette[index % palette.length]}" style="width:${percentage}%"></div>
                            </div>
                        </div>
                    `;
                }).join("")
                : `<p class="text-body-md text-on-surface-variant">暂无热门分类数据，产生几条咨询记录后这里会自动更新。</p>`;

            asideSection.innerHTML = `
                <div class="bg-surface-container-lowest p-md rounded-xl shadow-sm border border-outline-variant/10">
                    <h3 class="font-h2 text-h2 text-on-surface mb-md">热门咨询分类</h3>
                    <div class="space-y-md">
                        ${categoryRows}
                    </div>
                </div>
                <div class="bg-primary text-on-primary p-md rounded-xl shadow-lg relative overflow-hidden group">
                    <div class="relative z-10">
                        <h4 class="font-h1 text-h1 mb-2">继续沉淀咨询数据</h4>
                        <p class="text-body-md opacity-90 mb-md">咨询越多，这里的会话数、分类热度和竞赛覆盖就越真实，适合答辩时展示系统闭环。</p>
                        <div class="flex flex-wrap gap-sm">
                            <a href="${routes.home}" class="inline-flex items-center gap-xs bg-white text-primary px-sm py-2 rounded-lg font-h2 text-body-md shadow-sm hover:bg-primary-fixed transition-colors">
                                去首页
                            </a>
                            <a href="${routes.aiConsult}" class="inline-flex items-center gap-xs border border-white/40 px-sm py-2 rounded-lg font-h2 text-body-md hover:bg-white/10 transition-colors">
                                去咨询页
                            </a>
                        </div>
                    </div>
                    <span class="material-symbols-outlined absolute -right-4 -bottom-4 text-[120px] opacity-10 rotate-12 group-hover:scale-110 transition-transform">insights</span>
                </div>
            `;
        }

        function renderAll() {
            renderStatsSection();
            renderRecordsSection();
            renderAsideSection();
        }

        async function loadRecords() {
            state.loading = true;
            state.errorMessage = "";
            renderAll();

            try {
                const payload = await fetchApiJson("/api/consultations/records");
                const data = payload && payload.data ? payload.data : null;
                if (!data) {
                    throw new Error("没有获取到记录统计数据。");
                }

                state.overview = data.overview || state.overview;
                state.items = Array.isArray(data.items) ? data.items : [];
                state.hotCategories = Array.isArray(data.hot_categories) ? data.hot_categories : [];
            } catch (error) {
                state.errorMessage = `加载记录统计失败：${error.message}`;
            } finally {
                state.loading = false;
                renderAll();
                scrollToHashTarget();
            }
        }

        void loadRecords();
    }

    function wireRecordsPageEnhanced() {
        const statsSection = document.querySelector("section.grid.grid-cols-1.md\\:grid-cols-4");
        const recordsSection = document.querySelector("section.lg\\:col-span-8.space-y-md");
        const asideSection = document.querySelector("aside.lg\\:col-span-4.space-y-md");
        const footerLinks = document.querySelectorAll("footer a[href='#']");

        assignFooterLinks(footerLinks, [routes.launch, routes.home, routes.aiConsult]);

        if (!statsSection || !recordsSection || !asideSection) {
            scrollToHashTarget();
            return;
        }

        const state = {
            loading: true,
            errorMessage: "",
            activeTab: "consult",
            consultations: {
                overview: {
                    total_sessions: 0,
                    total_messages: 0,
                    competition_total: 0,
                    hot_category_total: 0
                },
                items: [],
                hotCategories: []
            },
            activities: {
                overview: {
                    total_searches: 0,
                    total_navigations: 0
                },
                searchRecords: [],
                navigationRecords: []
            }
        };

        function resolveActiveTabFromHash() {
            if (window.location.hash === "#search-records") {
                return "search";
            }

            if (window.location.hash === "#navigation-records") {
                return "navigation";
            }

            return "consult";
        }

        function tabHash(tabKey) {
            if (tabKey === "search") {
                return "#search-records";
            }

            if (tabKey === "navigation") {
                return "#navigation-records";
            }

            return "#consult-records";
        }

        function syncActiveTab() {
            state.activeTab = resolveActiveTabFromHash();
        }

        function formatDateTime(value) {
            if (!value) {
                return "--";
            }

            return String(value).slice(0, 16).replace("T", " ");
        }

        function pageLabel(page) {
            switch (page) {
                case "home":
                    return "首页";
                case "competition-detail":
                    return "竞赛详情";
                case "ai-consult":
                    return "AI 咨询";
                case "records-statistics":
                    return "记录统计";
                default:
                    return page || "未知页面";
            }
        }

        function actionLabel(action) {
            switch (action) {
                case "open_competition_detail":
                    return "打开竞赛详情";
                case "open_ai_consult":
                    return "进入 AI 咨询";
                case "back_to_competition_detail":
                    return "返回竞赛详情";
                case "open_record_consult":
                    return "从记录页继续咨询";
                case "back_to_competition_list":
                    return "返回竞赛列表";
                case "open_navigation_record":
                    return "从导航记录再次访问";
                default:
                    return action || "页面跳转";
            }
        }

        function searchSourceLabel(source) {
            switch (source) {
                case "keyword_input":
                    return "关键词输入";
                case "command_open_list":
                    return "命令打开列表";
                case "command_ai_search":
                    return "命令搜索 AI";
                case "command_keyword":
                    return "命令关键词";
                case "major_filter":
                    return "专业筛选";
                case "status_filter":
                    return "报名状态筛选";
                case "level_filter":
                    return "级别筛选";
                default:
                    return source || "普通搜索";
            }
        }

        function searchSummary(item) {
            const parts = [];
            if (item.keyword) {
                parts.push(`关键词：${item.keyword}`);
            }
            if (item.major) {
                parts.push(`专业：${item.major}`);
            }
            if (item.competition_level) {
                parts.push(`级别：${item.competition_level}`);
            }
            if (item.signup_status) {
                parts.push(`状态：${item.signup_status}`);
            }

            return parts.length ? parts.join(" / ") : "无筛选条件";
        }

        function aggregateRows(items, keyBuilder, labelBuilder, valueBuilder) {
            const bucket = new Map();

            items.forEach((item) => {
                const key = keyBuilder(item);
                if (!key) {
                    return;
                }

                if (!bucket.has(key)) {
                    bucket.set(key, {
                        label: labelBuilder(item, key),
                        value: 0
                    });
                }

                bucket.get(key).value += valueBuilder(item);
            });

            return Array.from(bucket.values()).sort((left, right) => right.value - left.value).slice(0, 4);
        }

        function renderStatsSection() {
            const consultationOverview = state.consultations.overview || {};
            const activityOverview = state.activities.overview || {};
            const cards = [
                {
                    label: "咨询会话数",
                    icon: "chat",
                    value: consultationOverview.total_sessions || 0,
                    hint: state.loading ? "正在同步最新记录..." : "AI 咨询已形成真实会话"
                },
                {
                    label: "累计消息数",
                    icon: "forum",
                    value: consultationOverview.total_messages || 0,
                    hint: "统计学生提问与 AI 回复"
                },
                {
                    label: "搜索记录数",
                    icon: "search",
                    value: activityOverview.total_searches || 0,
                    hint: "首页筛选与命令搜索都会沉淀"
                },
                {
                    label: "导航记录数",
                    icon: "route",
                    value: activityOverview.total_navigations || 0,
                    hint: "页面跳转路径可用于答辩展示"
                }
            ];

            statsSection.id = "hot-statistics";
            statsSection.innerHTML = cards.map((card) => `
                <div class="bg-surface-container-lowest p-md rounded-xl shadow-sm border border-outline-variant/10">
                    <div class="flex items-center justify-between mb-sm">
                        <span class="text-label-caps font-label-caps text-on-surface-variant uppercase">${escapeHtml(card.label)}</span>
                        <span class="material-symbols-outlined text-primary">${card.icon}</span>
                    </div>
                    <p class="text-display font-display text-on-surface">${escapeHtml(String(card.value))}</p>
                    <p class="text-body-md text-on-surface-variant mt-xs">${escapeHtml(card.hint)}</p>
                </div>
            `).join("");
        }

        function renderConsultationItems() {
            const items = state.consultations.items || [];
            if (!items.length) {
                return `
                    <div class="rounded-xl border border-dashed border-outline-variant/40 bg-surface-container-low px-md py-lg text-center">
                        <p class="text-h2 text-on-surface mb-sm">还没有咨询记录</p>
                        <p class="text-body-md text-on-surface-variant mb-md">先去竞赛详情页或 AI 咨询页提几个问题，这里就会自动生成记录。</p>
                        <a href="${routes.home}" class="inline-flex items-center gap-xs px-md py-sm rounded-lg bg-primary text-on-primary font-h2 text-body-md hover:opacity-90 transition-opacity">
                            去首页开始咨询
                            <span class="material-symbols-outlined">arrow_forward</span>
                        </a>
                    </div>
                `;
            }

            return items.map((item) => `
                <button type="button" data-record-competition-id="${escapeHtml(String(item.competition_id || 0))}" class="w-full flex items-start gap-md p-sm hover:bg-surface-container-low rounded-lg transition-all group text-left">
                    <div class="bg-primary-container/10 p-2 rounded-lg">
                        <span class="material-symbols-outlined text-primary">smart_toy</span>
                    </div>
                    <div class="flex-1 min-w-0">
                        <div class="flex justify-between items-start gap-sm">
                            <div>
                                <h4 class="font-h2 text-body-md text-on-surface">${escapeHtml(item.latest_question || `${item.competition_name || "竞赛"} 咨询记录`)}</h4>
                                <p class="text-label-caps text-on-surface-variant mt-1">${escapeHtml(item.competition_name || "未命名竞赛")} · ${escapeHtml(item.category_name || "未分类")}</p>
                            </div>
                            <span class="text-label-caps text-on-surface-variant shrink-0">${escapeHtml(formatDateTime(item.update_time))}</span>
                        </div>
                        <p class="text-body-md text-on-surface-variant mt-2 line-clamp-2">${escapeHtml(trimSnippet(item.latest_answer || item.latest_question || "暂无咨询内容。", 120))}</p>
                        <p class="text-label-caps text-primary mt-2">消息数 ${escapeHtml(String(item.message_count || 0))}</p>
                    </div>
                    <span class="material-symbols-outlined text-on-surface-variant opacity-0 group-hover:opacity-100 transition-opacity shrink-0">chevron_right</span>
                </button>
            `).join("");
        }

        function renderSearchItems() {
            const items = state.activities.searchRecords || [];
            if (!items.length) {
                return `
                    <div class="rounded-xl border border-dashed border-outline-variant/40 bg-surface-container-low px-md py-lg text-center">
                        <p class="text-h2 text-on-surface mb-sm">还没有搜索记录</p>
                        <p class="text-body-md text-on-surface-variant">在首页输入关键词、使用命令搜索或切换筛选后，这里会自动出现记录。</p>
                    </div>
                `;
            }

            return items.map((item) => `
                <div class="flex items-start gap-md p-sm rounded-lg border border-outline-variant/20 bg-surface-container-lowest">
                    <div class="bg-primary-container/10 p-2 rounded-lg">
                        <span class="material-symbols-outlined text-primary">search</span>
                    </div>
                    <div class="flex-1 min-w-0">
                        <div class="flex justify-between items-start gap-sm">
                            <div>
                                <h4 class="font-h2 text-body-md text-on-surface">${escapeHtml(searchSourceLabel(item.trigger_source))}</h4>
                                <p class="text-label-caps text-on-surface-variant mt-1">${escapeHtml(searchSummary(item))}</p>
                            </div>
                            <span class="text-label-caps text-on-surface-variant shrink-0">${escapeHtml(formatDateTime(item.create_time))}</span>
                        </div>
                        <p class="text-body-md text-on-surface-variant mt-2">命中结果 ${escapeHtml(String(item.result_count || 0))} 条</p>
                    </div>
                </div>
            `).join("");
        }

        function renderNavigationItems() {
            const items = state.activities.navigationRecords || [];
            if (!items.length) {
                return `
                    <div class="rounded-xl border border-dashed border-outline-variant/40 bg-surface-container-low px-md py-lg text-center">
                        <p class="text-h2 text-on-surface mb-sm">还没有导航记录</p>
                        <p class="text-body-md text-on-surface-variant">当你从首页进入详情页、从详情页进入 AI 咨询页时，这里会自动记录跳转轨迹。</p>
                    </div>
                `;
            }

            return items.map((item) => {
                const targetHref = item.to_page === "ai-consult" && item.competition_id
                    ? aiConsultUrl(item.competition_id)
                    : item.to_page === "competition-detail" && item.competition_id
                        ? competitionDetailUrl(item.competition_id)
                        : item.to_page === "home"
                            ? routes.competitionList
                            : "";

                return `
                    <button
                        type="button"
                        class="w-full flex items-start gap-md p-sm rounded-lg border border-outline-variant/20 bg-surface-container-lowest text-left ${targetHref ? "hover:bg-surface-container-low transition-colors" : "cursor-default"}"
                        ${targetHref ? `data-navigation-href="${escapeHtml(targetHref)}" data-navigation-competition-id="${escapeHtml(String(item.competition_id || 0))}" data-navigation-to-page="${escapeHtml(item.to_page || "")}"` : ""}
                    >
                        <div class="bg-primary-container/10 p-2 rounded-lg">
                            <span class="material-symbols-outlined text-primary">route</span>
                        </div>
                        <div class="flex-1 min-w-0">
                            <div class="flex justify-between items-start gap-sm">
                                <div>
                                    <h4 class="font-h2 text-body-md text-on-surface">${escapeHtml(actionLabel(item.action_name))}</h4>
                                    <p class="text-label-caps text-on-surface-variant mt-1">${escapeHtml(pageLabel(item.from_page))} → ${escapeHtml(pageLabel(item.to_page))}</p>
                                </div>
                                <span class="text-label-caps text-on-surface-variant shrink-0">${escapeHtml(formatDateTime(item.create_time))}</span>
                            </div>
                            <p class="text-body-md text-on-surface-variant mt-2">${escapeHtml(item.competition_name || "通用页面跳转")}</p>
                        </div>
                        ${targetHref ? `<span class="material-symbols-outlined text-on-surface-variant shrink-0">chevron_right</span>` : ""}
                    </button>
                `;
            }).join("");
        }

        function renderRecordsSection() {
            const statusNotice = state.loading
                ? `<div class="rounded-xl border border-primary/20 bg-primary/5 px-md py-sm text-body-md text-primary">正在加载记录统计...</div>`
                : state.errorMessage
                    ? `<div class="rounded-xl border border-error/30 bg-error-container/40 px-md py-sm text-body-md text-on-error-container">${escapeHtml(state.errorMessage)}</div>`
                    : "";

            const tabs = [
                { key: "consult", label: "咨询记录" },
                { key: "search", label: "搜索记录" },
                { key: "navigation", label: "导航记录" }
            ];

            let contentHtml = "";
            let footerText = "";
            if (state.activeTab === "search") {
                contentHtml = renderSearchItems();
                footerText = `当前展示 ${state.activities.searchRecords.length} 条最新搜索记录`;
            } else if (state.activeTab === "navigation") {
                contentHtml = renderNavigationItems();
                footerText = `当前展示 ${state.activities.navigationRecords.length} 条最新导航记录`;
            } else {
                contentHtml = renderConsultationItems();
                footerText = `当前展示 ${state.consultations.items.length} 条最新咨询会话`;
            }

            recordsSection.id = state.activeTab === "search"
                ? "search-records"
                : state.activeTab === "navigation"
                    ? "navigation-records"
                    : "consult-records";

            recordsSection.innerHTML = `
                <div class="bg-surface-container-lowest rounded-xl shadow-sm border border-outline-variant/10 overflow-hidden">
                    <div class="flex border-b border-outline-variant/10 px-md">
                        ${tabs.map((tab) => `
                            <button
                                type="button"
                                data-record-tab="${tab.key}"
                                class="px-md py-sm font-h2 text-body-md transition-colors ${state.activeTab === tab.key ? "text-primary border-b-2 border-primary" : "text-on-surface-variant hover:text-primary"}"
                            >
                                ${tab.label}
                            </button>
                        `).join("")}
                    </div>
                    <div class="p-md space-y-sm">
                        ${statusNotice}
                        ${contentHtml}
                    </div>
                    <div class="p-md text-center border-t border-outline-variant/10 text-body-md text-on-surface-variant">
                        ${escapeHtml(footerText)}
                    </div>
                </div>
            `;

            recordsSection.querySelectorAll("[data-record-tab]").forEach((button) => {
                button.addEventListener("click", () => {
                    const tabKey = button.getAttribute("data-record-tab");
                    if (!tabKey) {
                        return;
                    }
                    window.location.hash = tabHash(tabKey);
                });
            });

            recordsSection.querySelectorAll("[data-record-competition-id]").forEach((button) => {
                button.addEventListener("click", () => {
                    const competitionId = parsePositiveInteger(button.getAttribute("data-record-competition-id"));
                    if (!competitionId) {
                        return;
                    }
                    rememberCompetitionId(competitionId);
                    trackNavigationRecord("ai-consult", "open_record_consult", competitionId);
                    window.location.href = aiConsultUrl(competitionId);
                });
            });

            recordsSection.querySelectorAll("[data-navigation-href]").forEach((button) => {
                button.addEventListener("click", () => {
                    const href = button.getAttribute("data-navigation-href");
                    if (!href) {
                        return;
                    }

                    const competitionId = parsePositiveInteger(button.getAttribute("data-navigation-competition-id"));
                    const toPage = button.getAttribute("data-navigation-to-page") || "";
                    if (competitionId) {
                        rememberCompetitionId(competitionId);
                    }

                    trackNavigationRecord(toPage || "home", "open_navigation_record", competitionId || 0);
                    window.location.href = href;
                });
            });
        }

        function renderAsideSection() {
            const palette = ["bg-primary", "bg-secondary-container", "bg-tertiary", "bg-surface-container-highest"];
            let title = "热门咨询分类";
            let description = "咨询越多，这里的分布越真实，适合答辩时展示系统闭环。";
            let rows = [];

            if (state.activeTab === "search") {
                title = "搜索来源分布";
                description = "可以直观看到学生更偏向命令搜索还是筛选搜索。";
                rows = aggregateRows(
                    state.activities.searchRecords,
                    (item) => item.trigger_source || "unknown",
                    (item) => searchSourceLabel(item.trigger_source),
                    () => 1
                );
            } else if (state.activeTab === "navigation") {
                title = "导航去向分布";
                description = "能够展示学生从哪些页面流向详情页和咨询页。";
                rows = aggregateRows(
                    state.activities.navigationRecords,
                    (item) => item.to_page || "unknown",
                    (item, key) => pageLabel(key),
                    () => 1
                );
            } else {
                const totalSessions = Number(state.consultations.overview.total_sessions || 0);
                rows = (state.consultations.hotCategories || []).slice(0, 4).map((item) => ({
                    label: item.category_name || "未分类",
                    rawValue: item.session_count || 0,
                    percentage: totalSessions > 0 ? Math.round((Number(item.session_count || 0) / totalSessions) * 100) : 0
                }));
            }

            const rowsHtml = rows.length
                ? rows.map((item, index) => {
                    const rawValue = item.rawValue !== undefined ? item.rawValue : item.value;
                    const percentage = item.percentage !== undefined ? item.percentage : item.value;
                    return `
                        <div class="space-y-xs">
                            <div class="flex items-center justify-between">
                                <div class="flex items-center gap-2 min-w-0">
                                    <div class="w-3 h-3 rounded-full ${palette[index % palette.length]}"></div>
                                    <span class="text-body-md text-on-surface truncate">${escapeHtml(item.label)}</span>
                                </div>
                                <span class="text-body-md font-semibold shrink-0">${escapeHtml(String(rawValue))}</span>
                            </div>
                            <div class="h-2 rounded-full bg-surface-container-high overflow-hidden">
                                <div class="h-full ${palette[index % palette.length]}" style="width:${Math.max(5, Math.min(100, percentage || 0))}%"></div>
                            </div>
                        </div>
                    `;
                }).join("")
                : `<p class="text-body-md text-on-surface-variant">当前还没有足够的数据分布，继续使用系统后这里会自动更新。</p>`;

            asideSection.innerHTML = `
                <div class="bg-surface-container-lowest p-md rounded-xl shadow-sm border border-outline-variant/10">
                    <h3 class="font-h2 text-h2 text-on-surface mb-md">${escapeHtml(title)}</h3>
                    <div class="space-y-md">
                        ${rowsHtml}
                    </div>
                </div>
                <div class="bg-primary text-on-primary p-md rounded-xl shadow-lg relative overflow-hidden group">
                    <div class="relative z-10">
                        <h4 class="font-h1 text-h1 mb-2">继续沉淀使用数据</h4>
                        <p class="text-body-md opacity-90 mb-md">${escapeHtml(description)}</p>
                        <div class="flex flex-wrap gap-sm">
                            <a href="${routes.home}" class="inline-flex items-center gap-xs bg-white text-primary px-sm py-2 rounded-lg font-h2 text-body-md shadow-sm hover:bg-primary-fixed transition-colors">
                                去首页
                            </a>
                            <a href="${routes.aiConsult}" class="inline-flex items-center gap-xs border border-white/40 px-sm py-2 rounded-lg font-h2 text-body-md hover:bg-white/10 transition-colors">
                                去咨询页
                            </a>
                        </div>
                    </div>
                    <span class="material-symbols-outlined absolute -right-4 -bottom-4 text-[120px] opacity-10 rotate-12 group-hover:scale-110 transition-transform">insights</span>
                </div>
            `;
        }

        function renderAll() {
            renderStatsSection();
            renderRecordsSection();
            renderAsideSection();
        }

        async function loadRecords() {
            state.loading = true;
            state.errorMessage = "";
            syncActiveTab();
            renderAll();

            try {
                const payload = await fetchApiJson("/api/records/dashboard");
                const data = payload && payload.data ? payload.data : null;
                if (!data) {
                    throw new Error("没有获取到记录统计数据。");
                }

                const consultationData = data.consultations || {};
                const activityData = data.activities || {};

                state.consultations.overview = consultationData.overview || state.consultations.overview;
                state.consultations.items = Array.isArray(consultationData.items) ? consultationData.items : [];
                state.consultations.hotCategories = Array.isArray(consultationData.hot_categories) ? consultationData.hot_categories : [];
                state.activities.overview = activityData.overview || state.activities.overview;
                state.activities.searchRecords = Array.isArray(activityData.search_records) ? activityData.search_records : [];
                state.activities.navigationRecords = Array.isArray(activityData.navigation_records) ? activityData.navigation_records : [];
            } catch (error) {
                state.errorMessage = `加载记录统计失败：${error.message}`;
            } finally {
                state.loading = false;
                renderAll();
                scrollToHashTarget();
            }
        }

        window.addEventListener("hashchange", () => {
            syncActiveTab();
            renderAll();
            scrollToHashTarget();
        });

        syncActiveTab();
        void loadRecords();
    }

    function wireAdminPage() {
        const sidebarLinks = document.querySelectorAll("aside nav a");
        if (sidebarLinks.length >= 5) {
            sidebarLinks[0].setAttribute("href", routes.launch);
            sidebarLinks[1].setAttribute("href", routes.admin);
            sidebarLinks[2].setAttribute("href", routes.recordsConsult);
            sidebarLinks[3].setAttribute("href", routes.records);
            sidebarLinks[4].setAttribute("href", routes.home);
        }

        const workbench = document.querySelector("main .flex-1.overflow-y-auto");
        if (workbench) {
            workbench.id = "competition-workbench";
        }

        const headerActions = document.querySelector("main header .flex.items-center.space-x-md");
        if (headerActions && !headerActions.querySelector("[data-shell='admin-shortcuts']")) {
            const shortcutGroup = document.createElement("div");
            shortcutGroup.dataset.shell = "admin-shortcuts";
            shortcutGroup.className = "flex items-center gap-sm";
            shortcutGroup.innerHTML = `
                <a class="text-body-md text-on-surface-variant hover:text-primary transition-colors" href="${routes.home}">前台首页</a>
                <a class="text-body-md text-on-surface-variant hover:text-primary transition-colors" href="${routes.records}">记录统计</a>
                <button type="button" data-admin-logout="true" class="text-body-md text-on-surface-variant hover:text-primary transition-colors">退出登录</button>
            `;
            headerActions.prepend(shortcutGroup);
        }

        const logoutButton = headerActions ? headerActions.querySelector("[data-admin-logout='true']") : null;
        if (logoutButton && logoutButton.dataset.bound !== "true") {
            logoutButton.dataset.bound = "true";
            logoutButton.addEventListener("click", async () => {
                try {
                    if (getAuthToken()) {
                        await postApiForm("/api/auth/logout", {});
                    }
                } catch (error) {
                    // Ignore logout errors and clear local session anyway.
                }

                clearAuthSession();
                window.location.href = buildLaunchUrl({ login: "admin" });
            });
        }

        const addButtons = [
            document.querySelector("aside .px-md.pt-md button"),
            document.querySelector("main header button.bg-primary")
        ].filter(Boolean);

        addButtons.forEach((button) => {
            button.classList.add("cursor-pointer");
            button.addEventListener("click", () => {
                if (workbench) {
                    workbench.scrollTo({ top: 0, behavior: "smooth" });
                }
            });
        });

        if (!workbench) {
            return;
        }

        function renderAccessDenied(message) {
            workbench.innerHTML = `
                <section class="p-xl">
                    <div class="mx-auto max-w-3xl rounded-3xl border border-outline-variant/30 bg-surface-container-lowest p-xl shadow-sm">
                        <div class="flex items-center gap-sm mb-md">
                            <span class="material-symbols-outlined text-error text-[32px]">lock</span>
                            <div>
                                <h2 class="font-h1 text-h1 text-on-surface">需要管理员登录</h2>
                                <p class="text-body-md text-on-surface-variant mt-xs">${escapeHtml(message)}</p>
                            </div>
                        </div>
                        <div class="flex flex-wrap gap-sm">
                            <a href="${buildLaunchUrl({ redirect: "pages/admin-competition-management.html", login: "admin" })}" class="inline-flex items-center gap-xs rounded-xl bg-primary px-md py-sm text-body-md font-semibold text-on-primary hover:opacity-90 transition-opacity">
                                前往登录入口
                                <span class="material-symbols-outlined text-[18px]">arrow_forward</span>
                            </a>
                            <a href="${routes.home}" class="inline-flex items-center gap-xs rounded-xl border border-outline-variant px-md py-sm text-body-md font-semibold text-on-surface-variant hover:border-primary hover:text-primary transition-colors">
                                返回前台首页
                            </a>
                        </div>
                    </div>
                </section>
            `;
        }

        const searchInput = document.querySelector("main header input[type='text']");
        const primaryButton = document.querySelector("main header button.bg-primary");
        const tabButtons = Array.from(document.querySelectorAll("main > div.px-xl.pt-lg button"));

        if (tabButtons.length < 2) {
            return;
        }

        const state = {
            view: "competitions",
            searchKeyword: "",
            loading: false,
            submitting: false,
            notice: "",
            noticeIsError: false,
            categories: [],
            competitions: [],
            editingCategoryId: null,
            editingCompetitionId: null,
            categoryForm: createEmptyCategoryForm(),
            competitionForm: createEmptyCompetitionForm()
        };

        const competitionLevelOptions = ["International", "National A", "National", "Provincial", "School"];
        const signupStatusOptions = ["Draft", "Warmup", "Open", "Closed", "Ended"];
        const tabKeys = ["competitions", "categories", "review"];

        function createEmptyCategoryForm() {
            return {
                category_name: "",
                category_desc: "",
                sort_no: "10"
            };
        }

        function createEmptyCompetitionForm() {
            return {
                category_id: "",
                competition_name: "",
                organizer: "",
                competition_level: "National",
                signup_status: "Open",
                suitable_major: "",
                suitable_grade: "",
                signup_start: "",
                signup_end: "",
                competition_start: "",
                competition_end: "",
                official_url: "",
                competition_desc: "",
                participation_rules: "",
                display_status: true
            };
        }

        function toFormBody(formData = {}) {
            const params = new URLSearchParams();
            Object.entries(formData).forEach(([key, value]) => {
                if (value === undefined || value === null) {
                    return;
                }

                if (typeof value === "boolean") {
                    params.set(key, value ? "true" : "false");
                    return;
                }

                params.set(key, String(value));
            });
            return params.toString();
        }

        async function requestAdminApiJson(path, options = {}) {
            const {
                method = "GET",
                params = {},
                formData = null
            } = options;

            const headers = {
                Accept: "application/json"
            };
            const requestOptions = {
                method
            };

            if (formData) {
                headers["Content-Type"] = "application/x-www-form-urlencoded; charset=UTF-8";
                requestOptions.body = toFormBody(formData);
            }

            requestOptions.headers = applyAuthHeaders(headers);

            const response = await fetch(buildApiUrl(path, params), requestOptions);
            let payload = null;

            try {
                payload = await response.json();
            } catch (error) {
                throw new Error("接口返回的不是有效 JSON。");
            }

            if (!response.ok) {
                throw new Error(payload && payload.message ? payload.message : `请求失败：${response.status}`);
            }

            if (payload && typeof payload.code === "number" && payload.code !== 0) {
                throw new Error(payload.message || "接口返回异常。");
            }

            return payload;
        }

        function setNotice(message, isError = false) {
            state.notice = message;
            state.noticeIsError = isError;
        }

        function clearNotice() {
            setNotice("", false);
        }

        function normalizeDateTimeForInput(value) {
            return value ? String(value).replace(" ", "T").slice(0, 16) : "";
        }

        function formatAdminDateTime(value) {
            return value ? String(value).replace("T", " ").slice(0, 16) : "--";
        }

        function getAvailableCategoryId(preferredId = null) {
            const preferred = preferredId === null || preferredId === undefined ? "" : String(preferredId);
            if (preferred && state.categories.some((category) => String(category.category_id) === preferred)) {
                return preferred;
            }

            const firstBusinessCategory = state.categories.find((category) => Number(category.category_id) > 0);
            if (firstBusinessCategory) {
                return String(firstBusinessCategory.category_id);
            }

            const fallbackCategory = state.categories.find((category) => Number(category.category_id) === 0);
            return fallbackCategory ? String(fallbackCategory.category_id) : "";
        }

        function ensureCompetitionFormCategory() {
            state.competitionForm.category_id = getAvailableCategoryId(state.competitionForm.category_id);
        }

        function syncHeaderChrome() {
            const isReview = state.view === "review";
            const addLabel = state.view === "categories" ? "新增分类" : "新增竞赛";
            const searchPlaceholder = state.view === "categories" ? "搜索分类..." : "搜索竞赛...";

            if (searchInput) {
                searchInput.placeholder = isReview ? "审核中心暂未开放" : searchPlaceholder;
                searchInput.disabled = isReview;
                searchInput.value = isReview ? "" : state.searchKeyword;
            }

            [primaryButton, ...addButtons].filter(Boolean).forEach((button) => {
                button.disabled = false;
                const labelNode = button.querySelector("span:last-child");
                if (labelNode) {
                    labelNode.textContent = isReview ? "返回竞赛页" : addLabel;
                } else {
                    button.textContent = isReview ? "返回竞赛页" : addLabel;
                }
            });
        }

        function updateTabStyles() {
            const activeClassName = "pb-base border-b-2 border-primary text-primary font-h2 text-body-lg";
            const inactiveClassName = "pb-base border-b-2 border-transparent text-on-surface-variant hover:text-on-surface font-body-lg";

            tabButtons.forEach((button, index) => {
                button.className = tabKeys[index] === state.view ? activeClassName : inactiveClassName;
            });
        }

        function scrollWorkbenchToTop() {
            workbench.scrollTo({ top: 0, behavior: "smooth" });
        }

        function filteredCategories() {
            const keyword = state.searchKeyword.trim().toLowerCase();
            if (!keyword) {
                return state.categories;
            }

            return state.categories.filter((category) => {
                const searchText = `${category.category_name || ""} ${category.category_desc || ""}`.toLowerCase();
                return searchText.includes(keyword);
            });
        }

        function renderStatsCards() {
            const visibleCount = state.competitions.filter((competition) => competition.display_status).length;
            const hiddenCount = Math.max(0, state.competitions.length - visibleCount);
            const businessCategoryCount = state.categories.filter((category) => Number(category.category_id) > 0).length;
            const uncategorizedCount = state.categories.find((category) => Number(category.category_id) === 0)?.competition_count || 0;

            return `
                <section class="grid grid-cols-1 md:grid-cols-4 gap-gutter">
                    <div class="bg-surface-container-lowest p-md rounded-xl shadow-sm border border-outline-variant/10">
                        <p class="text-on-surface-variant font-label-caps text-label-caps uppercase mb-xs">竞赛总数</p>
                        <h3 class="text-display font-display text-primary">${state.competitions.length}</h3>
                        <div class="mt-xs text-on-surface-variant font-body-md">后台已接入可维护数据</div>
                    </div>
                    <div class="bg-surface-container-lowest p-md rounded-xl shadow-sm border border-outline-variant/10">
                        <p class="text-on-surface-variant font-label-caps text-label-caps uppercase mb-xs">前台可见</p>
                        <h3 class="text-display font-display text-on-surface">${visibleCount}</h3>
                        <div class="mt-xs text-secondary font-body-md">学生首页可直接展示</div>
                    </div>
                    <div class="bg-surface-container-lowest p-md rounded-xl shadow-sm border border-outline-variant/10">
                        <p class="text-on-surface-variant font-label-caps text-label-caps uppercase mb-xs">隐藏或草稿</p>
                        <h3 class="text-display font-display text-on-surface">${hiddenCount}</h3>
                        <div class="mt-xs text-on-surface-variant font-body-md">方便管理员先维护再发布</div>
                    </div>
                    <div class="bg-surface-container-lowest p-md rounded-xl shadow-sm border border-outline-variant/10">
                        <p class="text-on-surface-variant font-label-caps text-label-caps uppercase mb-xs">分类数量</p>
                        <h3 class="text-display font-display text-on-surface">${businessCategoryCount}</h3>
                        <div class="mt-xs text-on-surface-variant font-body-md">未分类竞赛 ${uncategorizedCount} 项</div>
                    </div>
                </section>
            `;
        }

        function renderNotice() {
            if (!state.notice) {
                return "";
            }

            return `
                <section class="rounded-xl border px-md py-sm text-body-md ${
                    state.noticeIsError
                        ? "border-error/30 bg-error-container/40 text-on-error-container"
                        : "border-primary/20 bg-primary/5 text-primary"
                }">
                    ${escapeHtml(state.notice)}
                </section>
            `;
        }

        function renderCompetitionRows() {
            if (state.loading) {
                return `
                    <tr>
                        <td colspan="6" class="px-md py-xl text-center text-on-surface-variant text-body-md">正在加载竞赛数据...</td>
                    </tr>
                `;
            }

            if (!state.competitions.length) {
                return `
                    <tr>
                        <td colspan="6" class="px-md py-xl text-center text-on-surface-variant text-body-md">当前没有符合条件的竞赛，可以先新增一条。</td>
                    </tr>
                `;
            }

            return state.competitions.map((competition) => {
                const status = statusMeta(competition.signup_status);
                const level = levelMeta(competition.competition_level);
                return `
                    <tr class="hover:bg-surface-container transition-colors">
                        <td class="px-md py-md align-top">
                            <div class="font-h2 text-body-md text-on-surface">${escapeHtml(competition.competition_name || "--")}</div>
                            <div class="mt-1 text-body-md text-on-surface-variant">${escapeHtml(competition.organizer || "未填写主办方")}</div>
                        </td>
                        <td class="px-md py-md align-top">
                            <div class="flex flex-wrap gap-xs">
                                <span class="bg-tertiary-fixed text-on-tertiary-fixed px-sm py-1 rounded-full text-label-caps">${escapeHtml(competition.category_name || "未分类")}</span>
                                <span class="${level.className}">${escapeHtml(level.label)}</span>
                            </div>
                        </td>
                        <td class="px-md py-md align-top">
                            <span class="${status.detailClass} inline-flex items-center px-sm py-1 rounded-full font-label-caps text-label-caps">${escapeHtml(status.label)}</span>
                        </td>
                        <td class="px-md py-md align-top text-body-md ${competition.display_status ? "text-secondary" : "text-on-surface-variant"}">
                            ${competition.display_status ? "显示中" : "已隐藏"}
                        </td>
                        <td class="px-md py-md align-top text-body-md text-on-surface-variant">${escapeHtml(formatAdminDateTime(competition.update_time))}</td>
                        <td class="px-md py-md align-top text-right">
                            <div class="flex items-center justify-end gap-xs">
                                <button type="button" data-admin-action="edit-competition" data-id="${competition.competition_id}" class="text-primary hover:bg-primary/10 px-sm py-1.5 rounded-lg transition-colors">编辑</button>
                                <button type="button" data-admin-action="delete-competition" data-id="${competition.competition_id}" class="text-error hover:bg-error/10 px-sm py-1.5 rounded-lg transition-colors">删除</button>
                            </div>
                        </td>
                    </tr>
                `;
            }).join("");
        }

        function renderCategoryCards() {
            const categories = filteredCategories();

            if (state.loading && !categories.length) {
                return `<div class="rounded-xl border border-outline-variant/20 bg-surface-container-low px-md py-lg text-center text-body-md text-on-surface-variant">正在加载分类数据...</div>`;
            }

            if (!categories.length) {
                return `<div class="rounded-xl border border-outline-variant/20 bg-surface-container-low px-md py-lg text-center text-body-md text-on-surface-variant">当前没有符合条件的分类，可以先新增一个分类。</div>`;
            }

            return `
                <div class="grid grid-cols-1 md:grid-cols-2 gap-md">
                    ${categories.map((category) => `
                        <article class="bg-surface-container-low rounded-xl border border-outline-variant/15 p-md space-y-sm">
                            <div class="flex items-start justify-between gap-sm">
                                <div>
                                    <div class="flex items-center gap-xs flex-wrap">
                                        <h3 class="font-h2 text-body-lg text-on-surface">${escapeHtml(category.category_name || "--")}</h3>
                                        ${Number(category.category_id) === 0 ? `<span class="px-xs py-[2px] rounded-full bg-surface-container-high text-on-surface-variant text-label-caps">系统默认</span>` : ""}
                                    </div>
                                    <p class="mt-xs text-body-md text-on-surface-variant">${escapeHtml(category.category_desc || "暂无分类说明")}</p>
                                </div>
                                <div class="flex items-center gap-xs">
                                    ${Number(category.category_id) > 0 ? `
                                        <button type="button" data-admin-action="edit-category" data-id="${category.category_id}" class="text-primary hover:bg-primary/10 px-sm py-1.5 rounded-lg transition-colors">编辑</button>
                                        <button type="button" data-admin-action="delete-category" data-id="${category.category_id}" class="text-error hover:bg-error/10 px-sm py-1.5 rounded-lg transition-colors">删除</button>
                                    ` : ""}
                                </div>
                            </div>
                            <div class="flex flex-wrap gap-sm text-body-md text-on-surface-variant">
                                <span>排序：${escapeHtml(String(category.sort_no ?? 0))}</span>
                                <span>竞赛数量：${escapeHtml(String(category.competition_count ?? 0))}</span>
                                <span>更新于：${escapeHtml(formatAdminDateTime(category.update_time))}</span>
                            </div>
                        </article>
                    `).join("")}
                </div>
            `;
        }

        function renderCompetitionForm() {
            const categoryOptions = state.categories.map((category) => `
                <option value="${escapeHtml(String(category.category_id))}" ${String(state.competitionForm.category_id) === String(category.category_id) ? "selected" : ""}>
                    ${escapeHtml(category.category_name)}
                </option>
            `).join("");

            return `
                <section class="bg-surface-container-lowest rounded-xl shadow-sm border border-outline-variant/10 overflow-hidden">
                    <div class="px-md py-md border-b border-outline-variant/20 flex items-center justify-between">
                        <div>
                            <h2 class="font-h2 text-h2 text-on-background">${state.editingCompetitionId ? "编辑竞赛" : "新增竞赛"}</h2>
                            <p class="text-body-md text-on-surface-variant">保存后首页和详情页会读取最新的接口数据。</p>
                        </div>
                        ${state.editingCompetitionId ? `<button type="button" data-admin-action="reset-competition-form" class="text-body-md text-on-surface-variant hover:text-primary transition-colors">取消编辑</button>` : ""}
                    </div>
                    <form data-admin-form="competition" class="p-md space-y-md">
                        <div class="grid grid-cols-1 md:grid-cols-2 gap-md">
                            <label class="space-y-xs">
                                <span class="text-body-md text-on-surface">竞赛名称</span>
                                <input required name="competition_name" value="${escapeHtml(state.competitionForm.competition_name)}" class="w-full rounded-xl border border-outline-variant bg-surface px-sm py-2 text-body-md focus:border-primary focus:ring-primary/20" type="text" placeholder="例如：蓝桥杯程序设计赛"/>
                            </label>
                            <label class="space-y-xs">
                                <span class="text-body-md text-on-surface">所属分类</span>
                                <select required name="category_id" class="w-full rounded-xl border border-outline-variant bg-surface px-sm py-2 text-body-md focus:border-primary focus:ring-primary/20">
                                    ${categoryOptions}
                                </select>
                            </label>
                            <label class="space-y-xs">
                                <span class="text-body-md text-on-surface">主办方</span>
                                <input name="organizer" value="${escapeHtml(state.competitionForm.organizer)}" class="w-full rounded-xl border border-outline-variant bg-surface px-sm py-2 text-body-md focus:border-primary focus:ring-primary/20" type="text" placeholder="例如：学院、学会、组委会"/>
                            </label>
                            <label class="space-y-xs">
                                <span class="text-body-md text-on-surface">赛事级别</span>
                                <select name="competition_level" class="w-full rounded-xl border border-outline-variant bg-surface px-sm py-2 text-body-md focus:border-primary focus:ring-primary/20">
                                    ${competitionLevelOptions.map((option) => `<option value="${escapeHtml(option)}" ${state.competitionForm.competition_level === option ? "selected" : ""}>${escapeHtml(option)}</option>`).join("")}
                                </select>
                            </label>
                            <label class="space-y-xs">
                                <span class="text-body-md text-on-surface">报名状态</span>
                                <select name="signup_status" class="w-full rounded-xl border border-outline-variant bg-surface px-sm py-2 text-body-md focus:border-primary focus:ring-primary/20">
                                    ${signupStatusOptions.map((option) => `<option value="${escapeHtml(option)}" ${state.competitionForm.signup_status === option ? "selected" : ""}>${escapeHtml(option)}</option>`).join("")}
                                </select>
                            </label>
                            <label class="space-y-xs">
                                <span class="text-body-md text-on-surface">适合专业</span>
                                <input name="suitable_major" value="${escapeHtml(state.competitionForm.suitable_major)}" class="w-full rounded-xl border border-outline-variant bg-surface px-sm py-2 text-body-md focus:border-primary focus:ring-primary/20" type="text" placeholder="例如：人工智能、计算机科学"/>
                            </label>
                            <label class="space-y-xs">
                                <span class="text-body-md text-on-surface">适合年级</span>
                                <input name="suitable_grade" value="${escapeHtml(state.competitionForm.suitable_grade)}" class="w-full rounded-xl border border-outline-variant bg-surface px-sm py-2 text-body-md focus:border-primary focus:ring-primary/20" type="text" placeholder="例如：大一及以上"/>
                            </label>
                            <label class="space-y-xs">
                                <span class="text-body-md text-on-surface">官网链接</span>
                                <input name="official_url" value="${escapeHtml(state.competitionForm.official_url)}" class="w-full rounded-xl border border-outline-variant bg-surface px-sm py-2 text-body-md focus:border-primary focus:ring-primary/20" type="url" placeholder="https://example.org"/>
                            </label>
                            <label class="space-y-xs">
                                <span class="text-body-md text-on-surface">报名开始</span>
                                <input name="signup_start" value="${escapeHtml(state.competitionForm.signup_start)}" class="w-full rounded-xl border border-outline-variant bg-surface px-sm py-2 text-body-md focus:border-primary focus:ring-primary/20" type="datetime-local"/>
                            </label>
                            <label class="space-y-xs">
                                <span class="text-body-md text-on-surface">报名截止</span>
                                <input name="signup_end" value="${escapeHtml(state.competitionForm.signup_end)}" class="w-full rounded-xl border border-outline-variant bg-surface px-sm py-2 text-body-md focus:border-primary focus:ring-primary/20" type="datetime-local"/>
                            </label>
                            <label class="space-y-xs">
                                <span class="text-body-md text-on-surface">比赛开始</span>
                                <input name="competition_start" value="${escapeHtml(state.competitionForm.competition_start)}" class="w-full rounded-xl border border-outline-variant bg-surface px-sm py-2 text-body-md focus:border-primary focus:ring-primary/20" type="datetime-local"/>
                            </label>
                            <label class="space-y-xs">
                                <span class="text-body-md text-on-surface">比赛结束</span>
                                <input name="competition_end" value="${escapeHtml(state.competitionForm.competition_end)}" class="w-full rounded-xl border border-outline-variant bg-surface px-sm py-2 text-body-md focus:border-primary focus:ring-primary/20" type="datetime-local"/>
                            </label>
                        </div>
                        <label class="space-y-xs block">
                            <span class="text-body-md text-on-surface">竞赛简介</span>
                            <textarea name="competition_desc" rows="4" class="w-full rounded-xl border border-outline-variant bg-surface px-sm py-2 text-body-md focus:border-primary focus:ring-primary/20" placeholder="用于首页卡片和详情页展示">${escapeHtml(state.competitionForm.competition_desc)}</textarea>
                        </label>
                        <label class="space-y-xs block">
                            <span class="text-body-md text-on-surface">参赛规则</span>
                            <textarea name="participation_rules" rows="4" class="w-full rounded-xl border border-outline-variant bg-surface px-sm py-2 text-body-md focus:border-primary focus:ring-primary/20" placeholder="例如：支持组队、每队 3 人、需要提交作品">${escapeHtml(state.competitionForm.participation_rules)}</textarea>
                        </label>
                        <label class="flex items-center gap-sm rounded-xl bg-surface-container-low px-md py-sm text-body-md text-on-surface">
                            <input ${state.competitionForm.display_status ? "checked" : ""} name="display_status" type="checkbox" class="rounded border-outline-variant text-primary focus:ring-primary/30"/>
                            <span>保存后立即在前台显示</span>
                        </label>
                        <div class="flex items-center justify-end gap-sm">
                            <button type="button" data-admin-action="reset-competition-form" class="px-md py-2 rounded-xl border border-outline-variant text-on-surface-variant hover:text-on-surface hover:border-primary transition-colors">重置</button>
                            <button ${state.submitting ? "disabled" : ""} type="submit" class="px-gutter py-2.5 rounded-xl bg-primary text-on-primary font-h2 text-body-md hover:opacity-90 transition-opacity disabled:opacity-60">
                                ${state.submitting ? "保存中..." : state.editingCompetitionId ? "保存修改" : "创建竞赛"}
                            </button>
                        </div>
                    </form>
                </section>
            `;
        }

        function renderCategoryForm() {
            return `
                <section class="bg-surface-container-lowest rounded-xl shadow-sm border border-outline-variant/10 overflow-hidden">
                    <div class="px-md py-md border-b border-outline-variant/20 flex items-center justify-between">
                        <div>
                            <h2 class="font-h2 text-h2 text-on-background">${state.editingCategoryId ? "编辑分类" : "新增分类"}</h2>
                            <p class="text-body-md text-on-surface-variant">第一版先保留最核心的分类信息，便于演示和维护。</p>
                        </div>
                        ${state.editingCategoryId ? `<button type="button" data-admin-action="reset-category-form" class="text-body-md text-on-surface-variant hover:text-primary transition-colors">取消编辑</button>` : ""}
                    </div>
                    <form data-admin-form="category" class="p-md space-y-md">
                        <label class="space-y-xs block">
                            <span class="text-body-md text-on-surface">分类名称</span>
                            <input required name="category_name" value="${escapeHtml(state.categoryForm.category_name)}" class="w-full rounded-xl border border-outline-variant bg-surface px-sm py-2 text-body-md focus:border-primary focus:ring-primary/20" type="text" placeholder="例如：人工智能类竞赛"/>
                        </label>
                        <label class="space-y-xs block">
                            <span class="text-body-md text-on-surface">分类说明</span>
                            <textarea name="category_desc" rows="4" class="w-full rounded-xl border border-outline-variant bg-surface px-sm py-2 text-body-md focus:border-primary focus:ring-primary/20" placeholder="用于说明这个分类下适合放哪些竞赛">${escapeHtml(state.categoryForm.category_desc)}</textarea>
                        </label>
                        <label class="space-y-xs block">
                            <span class="text-body-md text-on-surface">排序值</span>
                            <input name="sort_no" value="${escapeHtml(state.categoryForm.sort_no)}" class="w-full rounded-xl border border-outline-variant bg-surface px-sm py-2 text-body-md focus:border-primary focus:ring-primary/20" type="number" min="0" step="1"/>
                        </label>
                        <div class="flex items-center justify-end gap-sm">
                            <button type="button" data-admin-action="reset-category-form" class="px-md py-2 rounded-xl border border-outline-variant text-on-surface-variant hover:text-on-surface hover:border-primary transition-colors">重置</button>
                            <button ${state.submitting ? "disabled" : ""} type="submit" class="px-gutter py-2.5 rounded-xl bg-primary text-on-primary font-h2 text-body-md hover:opacity-90 transition-opacity disabled:opacity-60">
                                ${state.submitting ? "保存中..." : state.editingCategoryId ? "保存修改" : "创建分类"}
                            </button>
                        </div>
                    </form>
                </section>
            `;
        }

        function renderCompetitionsView() {
            return `
                <section class="grid grid-cols-1 xl:grid-cols-[minmax(0,1.4fr)_minmax(420px,0.9fr)] gap-gutter">
                    <div class="bg-surface-container-lowest rounded-xl shadow-sm border border-outline-variant/10 overflow-hidden">
                        <div class="px-md py-md border-b border-outline-variant/20 flex items-center justify-between">
                            <div>
                                <h2 class="font-h2 text-h2 text-on-background">竞赛列表</h2>
                                <p class="text-body-md text-on-surface-variant">搜索会直接联动后端接口，便于演示真实管理流程。</p>
                            </div>
                            <button type="button" data-admin-action="create-competition" class="text-primary font-body-md hover:underline">新增竞赛</button>
                        </div>
                        <div class="overflow-x-auto">
                            <table class="w-full text-left border-collapse">
                                <thead>
                                    <tr class="bg-surface-container-low/50">
                                        <th class="px-md py-sm font-label-caps text-label-caps uppercase text-on-surface-variant">竞赛</th>
                                        <th class="px-md py-sm font-label-caps text-label-caps uppercase text-on-surface-variant">分类 / 级别</th>
                                        <th class="px-md py-sm font-label-caps text-label-caps uppercase text-on-surface-variant">状态</th>
                                        <th class="px-md py-sm font-label-caps text-label-caps uppercase text-on-surface-variant">显示状态</th>
                                        <th class="px-md py-sm font-label-caps text-label-caps uppercase text-on-surface-variant">更新时间</th>
                                        <th class="px-md py-sm font-label-caps text-label-caps uppercase text-on-surface-variant text-right">操作</th>
                                    </tr>
                                </thead>
                                <tbody class="divide-y divide-outline-variant/10">
                                    ${renderCompetitionRows()}
                                </tbody>
                            </table>
                        </div>
                    </div>
                    ${renderCompetitionForm()}
                </section>
            `;
        }

        function renderCategoriesView() {
            return `
                <section class="grid grid-cols-1 xl:grid-cols-[minmax(0,1.2fr)_minmax(360px,0.8fr)] gap-gutter">
                    <div class="bg-surface-container-lowest rounded-xl shadow-sm border border-outline-variant/10 overflow-hidden">
                        <div class="px-md py-md border-b border-outline-variant/20 flex items-center justify-between">
                            <div>
                                <h2 class="font-h2 text-h2 text-on-background">分类管理</h2>
                                <p class="text-body-md text-on-surface-variant">支持新增、编辑、删除分类。删除后关联竞赛会自动回退到未分类。</p>
                            </div>
                            <button type="button" data-admin-action="create-category" class="text-primary font-body-md hover:underline">新增分类</button>
                        </div>
                        <div class="p-md">
                            ${renderCategoryCards()}
                        </div>
                    </div>
                    ${renderCategoryForm()}
                </section>
            `;
        }

        function renderReviewPlaceholder() {
            return `
                <section class="bg-surface-container-lowest rounded-xl shadow-sm border border-outline-variant/10 p-xl space-y-md">
                    <h2 class="font-h2 text-h2 text-on-background">审核中心</h2>
                    <p class="text-body-lg text-on-surface-variant">第一版先聚焦“竞赛分类与竞赛信息的增删改查”。这个页签已经预留好，后续如果你需要，我们可以继续加报名审核、导入校验或发布审批。</p>
                    <div class="flex gap-sm">
                        <button type="button" data-admin-action="switch-to-competitions" class="px-gutter py-2.5 rounded-xl bg-primary text-on-primary font-h2 text-body-md hover:opacity-90 transition-opacity">返回竞赛管理</button>
                        <button type="button" data-admin-action="switch-to-categories" class="px-gutter py-2.5 rounded-xl border border-outline-variant text-on-surface hover:border-primary transition-colors">查看分类管理</button>
                    </div>
                </section>
            `;
        }

        function renderAdminPage() {
            updateTabStyles();
            syncHeaderChrome();
            ensureCompetitionFormCategory();

            workbench.innerHTML = `
                <div class="space-y-lg">
                    ${renderNotice()}
                    ${renderStatsCards()}
                    ${state.view === "categories"
                        ? renderCategoriesView()
                        : state.view === "review"
                            ? renderReviewPlaceholder()
                            : renderCompetitionsView()}
                </div>
            `;

            const competitionForm = workbench.querySelector("[data-admin-form='competition']");
            if (competitionForm) {
                competitionForm.addEventListener("submit", handleCompetitionSubmit);
            }

            const categoryForm = workbench.querySelector("[data-admin-form='category']");
            if (categoryForm) {
                categoryForm.addEventListener("submit", handleCategorySubmit);
            }

            workbench.querySelectorAll("[data-admin-action]").forEach((element) => {
                element.addEventListener("click", async () => {
                    const action = element.dataset.adminAction;
                    const targetId = parsePositiveInteger(element.dataset.id);

                    if (action === "create-competition") {
                        startCreateCompetition();
                        return;
                    }

                    if (action === "create-category") {
                        startCreateCategory();
                        return;
                    }

                    if (action === "reset-competition-form") {
                        startCreateCompetition(false);
                        return;
                    }

                    if (action === "reset-category-form") {
                        startCreateCategory(false);
                        return;
                    }

                    if (action === "switch-to-competitions") {
                        activateView("competitions");
                        return;
                    }

                    if (action === "switch-to-categories") {
                        activateView("categories");
                        return;
                    }

                    if (action === "edit-competition" && targetId) {
                        startEditCompetition(targetId);
                        return;
                    }

                    if (action === "edit-category" && targetId) {
                        startEditCategory(targetId);
                        return;
                    }

                    if (action === "delete-competition" && targetId) {
                        await deleteCompetition(targetId);
                        return;
                    }

                    if (action === "delete-category" && targetId) {
                        await deleteCategory(targetId);
                    }
                });
            });
        }

        function activateView(nextView) {
            state.view = nextView;
            state.searchKeyword = "";
            clearNotice();
            renderAdminPage();
            void loadAdminData();
        }

        function startCreateCompetition(shouldScroll = true) {
            state.view = "competitions";
            state.editingCompetitionId = null;
            state.competitionForm = createEmptyCompetitionForm();
            ensureCompetitionFormCategory();
            clearNotice();
            renderAdminPage();
            if (shouldScroll) {
                scrollWorkbenchToTop();
            }
        }

        function startEditCompetition(competitionId) {
            const competition = state.competitions.find((item) => Number(item.competition_id) === Number(competitionId));
            if (!competition) {
                setNotice("未找到要编辑的竞赛。", true);
                renderAdminPage();
                return;
            }

            state.view = "competitions";
            state.editingCompetitionId = competitionId;
            state.competitionForm = {
                category_id: getAvailableCategoryId(competition.category_id),
                competition_name: competition.competition_name || "",
                organizer: competition.organizer || "",
                competition_level: competition.competition_level || "National",
                signup_status: competition.signup_status || "Open",
                suitable_major: competition.suitable_major || "",
                suitable_grade: competition.suitable_grade || "",
                signup_start: normalizeDateTimeForInput(competition.signup_start),
                signup_end: normalizeDateTimeForInput(competition.signup_end),
                competition_start: normalizeDateTimeForInput(competition.competition_start),
                competition_end: normalizeDateTimeForInput(competition.competition_end),
                official_url: competition.official_url || "",
                competition_desc: competition.competition_desc || "",
                participation_rules: competition.participation_rules || "",
                display_status: Boolean(competition.display_status)
            };
            clearNotice();
            renderAdminPage();
            scrollWorkbenchToTop();
        }

        function startCreateCategory(shouldScroll = true) {
            state.view = "categories";
            state.editingCategoryId = null;
            state.categoryForm = createEmptyCategoryForm();
            clearNotice();
            renderAdminPage();
            if (shouldScroll) {
                scrollWorkbenchToTop();
            }
        }

        function startEditCategory(categoryId) {
            const category = state.categories.find((item) => Number(item.category_id) === Number(categoryId));
            if (!category) {
                setNotice("未找到要编辑的分类。", true);
                renderAdminPage();
                return;
            }

            state.view = "categories";
            state.editingCategoryId = categoryId;
            state.categoryForm = {
                category_name: category.category_name || "",
                category_desc: category.category_desc || "",
                sort_no: String(category.sort_no ?? 0)
            };
            clearNotice();
            renderAdminPage();
            scrollWorkbenchToTop();
        }

        async function loadAdminData() {
            state.loading = true;
            renderAdminPage();

            try {
                const [categoryPayload, competitionPayload] = await Promise.all([
                    fetchApiJson("/api/categories"),
                    fetchApiJson("/api/competitions", {
                        keyword: state.view === "competitions" ? state.searchKeyword : "",
                        include_hidden: true
                    })
                ]);

                state.categories = Array.isArray(categoryPayload?.data?.items) ? categoryPayload.data.items : [];
                state.competitions = Array.isArray(competitionPayload?.data?.items) ? competitionPayload.data.items : [];
                ensureCompetitionFormCategory();
            } catch (error) {
                setNotice(`加载后台数据失败：${error.message}。请先运行 backend/run-dev.bat。`, true);
            } finally {
                state.loading = false;
                renderAdminPage();
            }
        }

        async function handleCompetitionSubmit(event) {
            event.preventDefault();

            const formData = new FormData(event.currentTarget);
            const payload = {
                category_id: formData.get("category_id") || "0",
                competition_name: formData.get("competition_name") || "",
                organizer: formData.get("organizer") || "",
                competition_level: formData.get("competition_level") || "",
                signup_status: formData.get("signup_status") || "",
                suitable_major: formData.get("suitable_major") || "",
                suitable_grade: formData.get("suitable_grade") || "",
                signup_start: formData.get("signup_start") || "",
                signup_end: formData.get("signup_end") || "",
                competition_start: formData.get("competition_start") || "",
                competition_end: formData.get("competition_end") || "",
                official_url: formData.get("official_url") || "",
                competition_desc: formData.get("competition_desc") || "",
                participation_rules: formData.get("participation_rules") || "",
                display_status: formData.get("display_status") === "on"
            };

            state.submitting = true;
            clearNotice();
            renderAdminPage();

            try {
                if (state.editingCompetitionId) {
                    await requestAdminApiJson(`/api/competitions/${state.editingCompetitionId}`, {
                        method: "PUT",
                        formData: payload
                    });
                    setNotice("竞赛信息已更新。");
                } else {
                    await requestAdminApiJson("/api/competitions", {
                        method: "POST",
                        formData: payload
                    });
                    setNotice("竞赛信息已创建。");
                }

                state.editingCompetitionId = null;
                state.competitionForm = createEmptyCompetitionForm();
                await loadAdminData();
                ensureCompetitionFormCategory();
            } catch (error) {
                setNotice(`保存竞赛失败：${error.message}`, true);
            } finally {
                state.submitting = false;
                renderAdminPage();
            }
        }

        async function handleCategorySubmit(event) {
            event.preventDefault();

            const formData = new FormData(event.currentTarget);
            const payload = {
                category_name: formData.get("category_name") || "",
                category_desc: formData.get("category_desc") || "",
                sort_no: formData.get("sort_no") || "0"
            };

            state.submitting = true;
            clearNotice();
            renderAdminPage();

            try {
                if (state.editingCategoryId) {
                    await requestAdminApiJson(`/api/categories/${state.editingCategoryId}`, {
                        method: "PUT",
                        formData: payload
                    });
                    setNotice("分类信息已更新。");
                } else {
                    await requestAdminApiJson("/api/categories", {
                        method: "POST",
                        formData: payload
                    });
                    setNotice("分类已创建。");
                }

                state.editingCategoryId = null;
                state.categoryForm = createEmptyCategoryForm();
                await loadAdminData();
            } catch (error) {
                setNotice(`保存分类失败：${error.message}`, true);
            } finally {
                state.submitting = false;
                renderAdminPage();
            }
        }

        async function deleteCompetition(competitionId) {
            const target = state.competitions.find((competition) => Number(competition.competition_id) === Number(competitionId));
            if (!target) {
                setNotice("未找到要删除的竞赛。", true);
                renderAdminPage();
                return;
            }

            if (!window.confirm(`确认删除竞赛“${target.competition_name}”吗？`)) {
                return;
            }

            try {
                await requestAdminApiJson(`/api/competitions/${competitionId}`, {
                    method: "DELETE"
                });

                if (state.editingCompetitionId === competitionId) {
                    state.editingCompetitionId = null;
                    state.competitionForm = createEmptyCompetitionForm();
                }

                setNotice("竞赛已删除。");
                await loadAdminData();
                ensureCompetitionFormCategory();
            } catch (error) {
                setNotice(`删除竞赛失败：${error.message}`, true);
                renderAdminPage();
            }
        }

        async function deleteCategory(categoryId) {
            const target = state.categories.find((category) => Number(category.category_id) === Number(categoryId));
            if (!target) {
                setNotice("未找到要删除的分类。", true);
                renderAdminPage();
                return;
            }

            if (!window.confirm(`确认删除分类“${target.category_name}”吗？该分类下竞赛会自动回退到未分类。`)) {
                return;
            }

            try {
                await requestAdminApiJson(`/api/categories/${categoryId}`, {
                    method: "DELETE"
                });

                if (state.editingCategoryId === categoryId) {
                    state.editingCategoryId = null;
                    state.categoryForm = createEmptyCategoryForm();
                }

                setNotice("分类已删除，关联竞赛已回退到未分类。");
                await loadAdminData();
                ensureCompetitionFormCategory();
            } catch (error) {
                setNotice(`删除分类失败：${error.message}`, true);
                renderAdminPage();
            }
        }

        const debouncedSearch = debounce(() => {
            if (state.view === "review") {
                return;
            }

            clearNotice();
            if (state.view === "categories") {
                renderAdminPage();
                return;
            }

            void loadAdminData();
        }, 250);

        if (searchInput && !searchInput.dataset.adminBound) {
            searchInput.dataset.adminBound = "true";
            searchInput.addEventListener("input", () => {
                state.searchKeyword = searchInput.value.trim();
                debouncedSearch();
            });
        }

        [primaryButton, ...addButtons].filter(Boolean).forEach((button) => {
            if (button.dataset.adminBound === "true") {
                return;
            }

            button.dataset.adminBound = "true";
            button.addEventListener("click", () => {
                if (state.view === "categories") {
                    startCreateCategory();
                    return;
                }

                if (state.view === "review") {
                    activateView("competitions");
                    return;
                }

                startCreateCompetition();
            });
        });

        tabButtons.forEach((button, index) => {
            if (button.dataset.adminBound === "true") {
                return;
            }

            button.dataset.adminBound = "true";
            button.addEventListener("click", () => {
                const nextView = tabKeys[index] || "competitions";
                if (nextView === state.view) {
                    return;
                }
                activateView(nextView);
            });
        });

        async function initializeAdminPage() {
            if (getAuthRole() !== "admin" || !getAuthToken()) {
                renderAccessDenied("当前后台已开启权限控制，请先使用管理员账号登录。");
                return;
            }

            try {
                const payload = await fetchApiJson("/api/auth/me");
                const user = payload && payload.data ? payload.data.user : null;
                if (!user || user.role !== "admin") {
                    clearAuthSession();
                    renderAccessDenied("当前登录身份不是管理员，请重新登录后再进入后台。");
                    return;
                }
            } catch (error) {
                clearAuthSession();
                renderAccessDenied(`管理员身份校验失败：${error.message}`);
                return;
            }

            renderAdminPage();
            await loadAdminData();
        }

        void initializeAdminPage();
    }

    makeBrandClickable();
    wireTopNav();

    if (pageName === "home.html") {
        wireHomePage();
    }

    if (pageName === "competition-detail.html") {
        wireCompetitionDetailPage();
    }

    if (pageName === "ai-consult.html") {
        wireAiConsultPage();
    }

    if (pageName === "records-statistics.html") {
        wireRecordsPageEnhanced();
    }

    if (pageName === "admin-competition-management.html") {
        wireAdminPage();
    }
})();
