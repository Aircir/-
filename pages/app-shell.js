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

    const pageName = window.location.pathname.split("/").pop();

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
        const headerActions = document.querySelector("header .flex.items-center.gap-sm");
        if (headerActions && !headerActions.querySelector("[data-shell='admin-entry']")) {
            const adminEntry = document.createElement("a");
            adminEntry.href = routes.admin;
            adminEntry.dataset.shell = "admin-entry";
            adminEntry.className = "rounded-lg border border-outline-variant px-sm py-xs text-body-md text-on-surface-variant hover:border-primary hover:text-primary transition-colors";
            adminEntry.textContent = "管理入口";
            headerActions.prepend(adminEntry);
        }

        const resultsSection = document.querySelector("main [class*='md:col-span-9']");
        if (resultsSection) {
            resultsSection.id = "competition-results";
        }

        const competitionGrid = document.querySelector("main [class*='md:col-span-9'] [class*='lg:grid-cols-2']");
        const competitionCards = competitionGrid ? Array.from(competitionGrid.children).filter((node) => node.tagName === "DIV") : [];
        if (competitionCards[0]) {
            competitionCards[0].id = "icpc-card";
        }
        if (competitionCards[1]) {
            competitionCards[1].id = "lanqiao-card";
        }
        if (competitionCards[2]) {
            competitionCards[2].id = "ai-competition-card";
            competitionCards[2].classList.add("transition-all");
        }

        competitionCards.forEach((card) => {
            const buttons = card.querySelectorAll("button");
            const actionButton = buttons[buttons.length - 1];
            if (!actionButton) {
                return;
            }

            actionButton.classList.add("cursor-pointer");
            actionButton.addEventListener("click", () => {
                window.location.href = routes.competitionDetail;
            });
        });

        const commandInput = document.querySelector("section.max-w-3xl input");
        const commandButton = document.querySelector("section.max-w-3xl button");
        const commandSection = document.querySelector("section.max-w-3xl");
        let feedback = document.getElementById("nl-feedback");

        if (commandSection && !feedback) {
            feedback = document.createElement("p");
            feedback.id = "nl-feedback";
            feedback.className = "mx-sm mt-xs hidden text-body-md text-primary";
            commandSection.appendChild(feedback);
        }

        function clearHomeFeedback() {
            if (feedback) {
                feedback.classList.add("hidden");
                feedback.textContent = "";
            }

            const aiCard = document.getElementById("ai-competition-card");
            if (aiCard) {
                aiCard.classList.remove("ring-2", "ring-secondary", "ring-offset-2", "ring-offset-background", "scale-[1.01]");
            }
        }

        function showHomeFeedback(message) {
            if (!feedback) {
                return;
            }

            feedback.textContent = message;
            feedback.classList.remove("hidden");
        }

        function applyHomeHashAction() {
            clearHomeFeedback();

            if (window.location.hash === "#competition-results" && resultsSection) {
                showHomeFeedback("已打开竞赛列表，你可以继续筛选和查看竞赛详情。");
                resultsSection.scrollIntoView({ behavior: "smooth", block: "start" });
                return;
            }

            if (window.location.hash === "#ai-competition-card") {
                const aiCard = document.getElementById("ai-competition-card");
                if (!aiCard) {
                    return;
                }

                aiCard.classList.add("ring-2", "ring-secondary", "ring-offset-2", "ring-offset-background", "scale-[1.01]");
                showHomeFeedback("已为你定位到人工智能类竞赛示例，可以直接点击详情进入下一步。");
                aiCard.scrollIntoView({ behavior: "smooth", block: "center" });
            }
        }

        function handleCommand() {
            const command = commandInput ? commandInput.value.trim() : "";
            if (!command) {
                clearHomeFeedback();
                showHomeFeedback("可尝试输入：打开竞赛列表页，或搜索人工智能类竞赛。");
                return;
            }

            if (command.includes("打开") && command.includes("竞赛列表")) {
                window.location.hash = "competition-results";
                applyHomeHashAction();
                return;
            }

            if ((command.includes("搜索") || command.includes("查找")) && command.includes("人工智能")) {
                window.location.hash = "ai-competition-card";
                applyHomeHashAction();
                return;
            }

            clearHomeFeedback();
            showHomeFeedback("当前前端演示版已支持 2 条指令：打开竞赛列表页、搜索人工智能类竞赛。");
        }

        if (commandButton) {
            commandButton.addEventListener("click", handleCommand);
        }

        if (commandInput) {
            commandInput.addEventListener("keydown", (event) => {
                if (event.key === "Enter") {
                    event.preventDefault();
                    handleCommand();
                }
            });
        }

        const footerLinks = document.querySelectorAll("footer a[href='#']");
        assignFooterLinks(footerLinks, [
            routes.competitionList,
            routes.aiConsult,
            routes.recordsHot,
            routes.aiConsult,
            routes.recordsConsult,
            routes.records
        ]);

        window.addEventListener("hashchange", applyHomeHashAction);
        applyHomeHashAction();
    }

    function wireCompetitionDetailPage() {
        const backButton = document.querySelector("main .mb-sm button");
        if (backButton) {
            backButton.classList.add("cursor-pointer");
            backButton.addEventListener("click", () => {
                window.location.href = routes.competitionList;
            });
        }

        const consultButton = document.querySelector("section.bg-surface-container-lowest button");
        if (consultButton) {
            consultButton.classList.add("cursor-pointer");
            consultButton.addEventListener("click", () => {
                window.location.href = routes.aiConsult;
            });
        }

        const officialLink = document.querySelector("a.flex.items-center.justify-between.bg-surface-container-lowest");
        if (officialLink && officialLink.getAttribute("href") === "#") {
            officialLink.setAttribute("href", "https://www.mcm.edu.cn/");
            officialLink.setAttribute("target", "_blank");
            officialLink.setAttribute("rel", "noreferrer");
        }

        const footerLinks = document.querySelectorAll("footer a[href='#']");
        assignFooterLinks(footerLinks, [routes.launch, routes.records, routes.aiConsult]);
    }

    function wireAiConsultPage() {
        const detailButton = document.querySelector("aside button.w-full");
        if (detailButton) {
            detailButton.classList.add("cursor-pointer");
            detailButton.addEventListener("click", () => {
                window.location.href = routes.competitionDetail;
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
