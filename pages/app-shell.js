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
        lastCompetitionId: "lastCompetitionId"
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
            headers: {
                Accept: "application/json"
            }
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
            rawItems: []
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
                competition_level: activeLevelIndex >= 0 ? levelValuesByIndex[activeLevelIndex] || "" : ""
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
                    window.location.href = competitionDetailUrl(competitionId);
                });
            });
        }

        async function loadCompetitions(options = {}) {
            const { feedbackMessage = "", preserveNotice = false } = options;
            const filters = readHomeFilters();

            if (!preserveNotice) {
                showResultsNotice(`正在从 ${apiBase} 加载竞赛数据...`);
            }

            try {
                const payload = await fetchApiJson("/api/competitions", filters);
                const items = payload && payload.data && Array.isArray(payload.data.items) ? payload.data.items : [];
                state.rawItems = items;
                renderCompetitionList(items);
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
            void loadCompetitions();
        }, 300);

        if (commandButton) {
            commandButton.addEventListener("click", () => {
                const command = commandInput ? commandInput.value.trim() : "";
                if (!command) {
                    clearHomeFeedback();
                    showHomeFeedback("可以尝试输入：打开竞赛列表页，或搜索人工智能类竞赛。");
                    return;
                }

                if (command.includes("打开") && command.includes("竞赛列表")) {
                    if (filterKeywordInput) {
                        filterKeywordInput.value = "";
                    }
                    if (majorSelect) {
                        majorSelect.selectedIndex = 0;
                    }
                    levelCheckboxes.forEach((checkbox) => {
                        checkbox.checked = false;
                    });
                    setStatusButtonStyles(0);
                    window.location.hash = "competition-results";
                    void loadCompetitions({ feedbackMessage: "已打开竞赛列表。" });
                    return;
                }

                if ((command.includes("搜索") || command.includes("查找")) && command.includes("人工智能")) {
                    if (filterKeywordInput) {
                        filterKeywordInput.value = "artificial intelligence";
                    }
                    if (majorSelect) {
                        majorSelect.selectedIndex = 0;
                    }
                    levelCheckboxes.forEach((checkbox) => {
                        checkbox.checked = false;
                    });
                    setStatusButtonStyles(0);
                    window.location.hash = "ai-competition-card";
                    void loadCompetitions({ feedbackMessage: "已为你搜索人工智能类竞赛。" });
                    return;
                }

                if (filterKeywordInput) {
                    filterKeywordInput.value = command;
                }
                window.location.hash = "competition-results";
                void loadCompetitions({ feedbackMessage: `已按关键词“${command}”执行搜索。` });
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
                debouncedReload();
            });
        }

        if (majorSelect) {
            majorSelect.addEventListener("change", () => {
                clearHomeFeedback();
                window.location.hash = "competition-results";
                void loadCompetitions();
            });
        }

        statusButtons.forEach((button, index) => {
            button.addEventListener("click", () => {
                setStatusButtonStyles(index);
                clearHomeFeedback();
                window.location.hash = "competition-results";
                void loadCompetitions();
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
                window.location.hash = "competition-results";
                void loadCompetitions();
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
        const detailButton = document.querySelector("aside button.w-full");
        if (detailButton) {
            detailButton.classList.add("cursor-pointer");
            detailButton.addEventListener("click", () => {
                const competitionId = parsePositiveInteger(getQueryParam("competitionId")) || readRememberedCompetitionId();
                window.location.href = competitionDetailUrl(competitionId || 3);
            });
        }

        const exampleButtons = document.querySelectorAll(".flex.flex-wrap.gap-xs.mb-md button");
        const textarea = document.querySelector("textarea");
        exampleButtons.forEach((button) => {
            button.addEventListener("click", () => {
                if (textarea) {
                    textarea.value = button.textContent.trim();
                    textarea.focus();
                }
            });
        });
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

        scrollToHashTarget();
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
            `;
            headerActions.prepend(shortcutGroup);
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
                method,
                headers
            };

            if (formData) {
                headers["Content-Type"] = "application/x-www-form-urlencoded; charset=UTF-8";
                requestOptions.body = toFormBody(formData);
            }

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

        renderAdminPage();
        void loadAdminData();
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
        wireRecordsPage();
    }

    if (pageName === "admin-competition-management.html") {
        wireAdminPage();
    }
})();
