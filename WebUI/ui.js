(function () {
    
    const I18N = {
        '0': {
            'nav.home': '主页',
            'nav.settings': '设置',
            'app.subtitle': '快速、简洁的文件复制',
            'form.srcLabel': '源文件夹',
            'form.srcPlaceholder': '选择源文件夹...',
            'form.dstLabel': '目标文件夹',
            'form.dstPlaceholder': '选择目标文件夹...',
            'form.strategyLabel': '策略',
            'form.verify': '校验完整性',
            'form.speedLimit': '速度限制',
            'form.language': '语言',
            'strategy.auto': '智能检测',
            'strategy.small': '小文件优化',
            'strategy.large': '大文件优化',
            'strategy.balanced': '平衡模式',
            'btn.browse': '浏览',
            'btn.start': '开始复制',
            'btn.pause': '暂停',
            'btn.resume': '继续',
            'btn.stop': '终止',
            'btn.save': '保存',
            'btn.cancel': '取消',
            'btn.ok': '确定',
            'status.ready': '准备就绪',
            'status.paused': '已暂停',
            'hint.unlimited': '0 = 不限速',
            'lang.zhCN': '简体中文',
            'lang.zhTW': '繁體中文',
            'lang.en': 'English',
            'lang.ko': '한국어',
            'lang.ja': '日本語',
            'dialog.stopped': '已终止',
            'dialog.stoppedMsg': '复制已终止。',
            'dialog.done': '全部复制完成',
            'dialog.doneMsg': '成功复制 {n} 个文件。',
            'dialog.failed': '完成（有失败）',
            'dialog.failedMsg': '共 {n} 个文件失败。',
            'settings.title': '设置',
            'err.verify': '校验失败',
            'err.fileNotFound': '找不到文件',
            'err.pathNotFound': '找不到路径',
            'err.accessDenied': '访问被拒绝',
            'err.inUse': '文件被占用',
            'err.diskFull': '磁盘空间不足',
            'err.pathTooLong': '路径过长',
            'err.startInvalid': '启动失败：源或目标无效',
            'err.code': '错误代码 {n}',
        },
        '1': {
            'nav.home': '主頁',
            'nav.settings': '設定',
            'app.subtitle': '快速、簡潔的檔案複製',
            'form.srcLabel': '來源資料夾',
            'form.srcPlaceholder': '選擇來源資料夾...',
            'form.dstLabel': '目標資料夾',
            'form.dstPlaceholder': '選擇目標資料夾...',
            'form.strategyLabel': '策略',
            'form.verify': '校驗完整性',
            'form.speedLimit': '速度限制',
            'form.language': '語言',
            'strategy.auto': '智慧偵測',
            'strategy.small': '小檔案最佳化',
            'strategy.large': '大檔案最佳化',
            'strategy.balanced': '平衡模式',
            'btn.browse': '瀏覽',
            'btn.start': '開始複製',
            'btn.pause': '暫停',
            'btn.resume': '繼續',
            'btn.stop': '終止',
            'btn.save': '儲存',
            'btn.cancel': '取消',
            'btn.ok': '確定',
            'status.ready': '準備就緒',
            'status.paused': '已暫停',
            'hint.unlimited': '0 = 不限速',
            'lang.zhCN': '简体中文',
            'lang.zhTW': '繁體中文',
            'lang.en': 'English',
            'lang.ko': '한국어',
            'lang.ja': '日本語',
            'dialog.stopped': '已終止',
            'dialog.stoppedMsg': '複製已終止。',
            'dialog.done': '全部複製完成',
            'dialog.doneMsg': '成功複製 {n} 個檔案。',
            'dialog.failed': '完成（有失敗）',
            'dialog.failedMsg': '共 {n} 個檔案失敗。',
            'settings.title': '設定',
            'err.verify': '校驗失敗',
            'err.fileNotFound': '找不到檔案',
            'err.pathNotFound': '找不到路徑',
            'err.accessDenied': '存取被拒',
            'err.inUse': '檔案被佔用',
            'err.diskFull': '磁碟空間不足',
            'err.pathTooLong': '路徑過長',
            'err.startInvalid': '啟動失敗：來源或目標無效',
            'err.code': '錯誤代碼 {n}',
        },
        '2': {
            'nav.home': 'Home',
            'nav.settings': 'Settings',
            'app.subtitle': 'Fast, simple file copy',
            'form.srcLabel': 'Source folder',
            'form.srcPlaceholder': 'Choose source folder...',
            'form.dstLabel': 'Destination folder',
            'form.dstPlaceholder': 'Choose destination folder...',
            'form.strategyLabel': 'Strategy',
            'form.verify': 'Verify integrity',
            'form.speedLimit': 'Speed limit',
            'form.language': 'Language',
            'strategy.auto': 'Auto detect',
            'strategy.small': 'Small files',
            'strategy.large': 'Large files',
            'strategy.balanced': 'Balanced',
            'btn.browse': 'Browse',
            'btn.start': 'Start',
            'btn.pause': 'Pause',
            'btn.resume': 'Resume',
            'btn.stop': 'Stop',
            'btn.save': 'Save',
            'btn.cancel': 'Cancel',
            'btn.ok': 'OK',
            'status.ready': 'Ready',
            'status.paused': 'Paused',
            'hint.unlimited': '0 = unlimited',
            'lang.zhCN': '简体中文',
            'lang.zhTW': '繁體中文',
            'lang.en': 'English',
            'lang.ko': '한국어',
            'lang.ja': '日本語',
            'dialog.stopped': 'Stopped',
            'dialog.stoppedMsg': 'Copy stopped.',
            'dialog.done': 'Copy complete',
            'dialog.doneMsg': 'Successfully copied {n} files.',
            'dialog.failed': 'Completed with errors',
            'dialog.failedMsg': '{n} file(s) failed.',
            'settings.title': 'Settings',
            'err.verify': 'Verification failed',
            'err.fileNotFound': 'File not found',
            'err.pathNotFound': 'Path not found',
            'err.accessDenied': 'Access denied',
            'err.inUse': 'File in use',
            'err.diskFull': 'Disk full',
            'err.pathTooLong': 'Path too long',
            'err.startInvalid': 'Start failed: invalid source or target',
            'err.code': 'Error code {n}',
        },
        // ★ TODO: 韩语、日语翻译待补。先用中文占位，保证切换时能看到变化。
        '3': {
            'nav.home': '홈',
            'nav.settings': '설정',
            'app.subtitle': '빠르고 간단한 파일 복사',
            'form.srcLabel': '원본 폴더',
            'form.srcPlaceholder': '원본 폴더 선택...',
            'form.dstLabel': '대상 폴더',
            'form.dstPlaceholder': '대상 폴더 선택...',
            'form.strategyLabel': '전략',
            'form.verify': '무결성 검사',
            'form.speedLimit': '속도 제한',
            'form.language': '언어',
            'strategy.auto': '자동 감지',
            'strategy.small': '작은 파일',
            'strategy.large': '큰 파일',
            'strategy.balanced': '균형',
            'btn.browse': '찾아보기',
            'btn.start': '시작',
            'btn.pause': '일시정지',
            'btn.resume': '계속',
            'btn.stop': '중지',
            'btn.save': '저장',
            'btn.cancel': '취소',
            'btn.ok': '확인',
            'status.ready': '준비 완료',
            'status.paused': '일시중지됨',
            'hint.unlimited': '0 = 무제한',
            'lang.zhCN': '简体中文',
            'lang.zhTW': '繁體中文',
            'lang.en': 'English',
            'lang.ko': '한국어',
            'lang.ja': '日本語',
            'dialog.stopped': '중지됨',
            'dialog.stoppedMsg': '복사가 중지되었습니다.',
            'dialog.done': '복사 완료',
            'dialog.doneMsg': '{n}개 파일을 성공적으로 복사했습니다.',
            'dialog.failed': '오류와 함께 완료',
            'dialog.failedMsg': '{n}개 파일이 실패했습니다.',
            'settings.title': '설정',
            'err.verify': '검증 실패',
            'err.fileNotFound': '파일을 찾을 수 없음',
            'err.pathNotFound': '경로를 찾을 수 없음',
            'err.accessDenied': '액세스 거부',
            'err.inUse': '파일 사용 중',
            'err.diskFull': '디스크 공간 부족',
            'err.pathTooLong': '경로가 너무 김',
            'err.startInvalid': '시작 실패: 원본 또는 대상이 유효하지 않음',
            'err.code': '오류 코드 {n}',
        },
        '4': {
            'nav.home': 'ホーム',
            'nav.settings': '設定',
            'app.subtitle': '高速でシンプルなファイルコピー',
            'form.srcLabel': 'コピー元',
            'form.srcPlaceholder': 'コピー元フォルダを選択...',
            'form.dstLabel': 'コピー先',
            'form.dstPlaceholder': 'コピー先フォルダを選択...',
            'form.strategyLabel': '戦略',
            'form.verify': '整合性を検証',
            'form.speedLimit': '速度制限',
            'form.language': '言語',
            'strategy.auto': '自動検出',
            'strategy.small': '小ファイル',
            'strategy.large': '大ファイル',
            'strategy.balanced': 'バランス',
            'btn.browse': '参照',
            'btn.start': '開始',
            'btn.pause': '一時停止',
            'btn.resume': '再開',
            'btn.stop': '停止',
            'btn.save': '保存',
            'btn.cancel': 'キャンセル',
            'btn.ok': 'OK',
            'status.ready': '準備完了',
            'status.paused': '一時停止中',
            'hint.unlimited': '0 = 無制限',
            'lang.zhCN': '简体中文',
            'lang.zhTW': '繁體中文',
            'lang.en': 'English',
            'lang.ko': '한국어',
            'lang.ja': '日本語',
            'dialog.stopped': '停止しました',
            'dialog.stoppedMsg': 'コピーを停止しました。',
            'dialog.done': 'コピー完了',
            'dialog.doneMsg': '{n} 個のファイルをコピーしました。',
            'dialog.failed': 'エラーあり',
            'dialog.failedMsg': '{n} 個のファイルが失敗しました。',
            'settings.title': '設定',
            'err.verify': '検証失敗',
            'err.fileNotFound': 'ファイルが見つかりません',
            'err.pathNotFound': 'パスが見つかりません',
            'err.accessDenied': 'アクセスが拒否されました',
            'err.inUse': 'ファイルが使用中です',
            'err.diskFull': 'ディスク容量不足',
            'err.pathTooLong': 'パスが長すぎます',
            'err.startInvalid': '開始失敗：ソースまたは保存先が無効です',
            'err.code': 'エラーコード {n}',
        },
    };

    let currentLang = '0';
    let lastConfig = null;   // 最近一次内核 config，"取消"时回退用
    let lastStatus = { isCopying: false, isPaused: false };

    // 取翻译（支持 {n} 占位替换）
    function t(key, repl) {
        const dict = I18N[currentLang] || I18N['0'];
        let s = dict[key] != null ? dict[key] : key;
        if (repl) {
            for (const k in repl) s = s.replace('{' + k + '}', repl[k]);
        }
        return s;
    }

    // 把整页应用成指定语言
    function applyLang(idx) {
        const key = String(idx);
        if (!I18N[key]) return;
        currentLang = key;

        document.querySelectorAll('[data-i18n]').forEach(el => {
            const k = el.dataset.i18n;
            const dict = I18N[currentLang];
            if (dict[k] != null) el.textContent = dict[k];
        });
        document.querySelectorAll('[data-i18n-placeholder]').forEach(el => {
            const k = el.dataset.i18nPlaceholder;
            const dict = I18N[currentLang];
            if (dict[k] != null) el.placeholder = dict[k];
        });

        // 暂停按钮在复制中要随暂停状态显示"暂停/继续"
        const btnPause = document.getElementById('btnPause');
        if (btnPause && !btnPause.disabled) {
            btnPause.textContent = lastStatus.isPaused ? t('btn.resume') : t('btn.pause');
        }
        // 空闲时状态栏同步为对应语言
        if (!lastStatus.isCopying) {
            const st = document.getElementById('statusText');
            if (st) st.textContent = t('status.ready');
        }

        // 更新 html lang 属性（可选）
        const htmlLangMap = { '0': 'zh-CN', '1': 'zh-TW', '2': 'en', '3': 'ko', '4': 'ja' };
        document.documentElement.lang = htmlLangMap[currentLang] || 'zh-CN';
    }

    // 内核 failList 格式："错误码|路径\n"，每行一条；
    // 999=校验失败，1000=宿主启动失败，其余为 Windows 系统错误码。
    function errText(code) {
        const map = {
            999: 'err.verify', 2: 'err.fileNotFound', 3: 'err.pathNotFound',
            5: 'err.accessDenied', 32: 'err.inUse', 112: 'err.diskFull',
            206: 'err.pathTooLong', 1000: 'err.startInvalid'
        };
        const key = map[code];
        return key ? t(key) : t('err.code', { n: code });
    }

    function formatFailList(raw) {
        if (!raw) return '';
        const lines = raw.split('\n').map(s => s.trim()).filter(Boolean);
        const parts = lines.map(line => {
            const bar = line.indexOf('|');
            let code = 0, path = '';
            if (bar >= 0) {
                code = parseInt(line.slice(0, bar), 10) || 0;
                path = line.slice(bar + 1);
            } else {
                code = parseInt(line, 10) || 0;
            }
            const msg = errText(code);
            return path ? (path + ' — ' + msg) : msg;
        });
        return parts.length ? ('\n\n' + parts.join('\n')) : '';
    }

    // ---------- 页面切换 ----------
    const pages = {
        home: document.querySelector('.page-home'),
        settings: document.querySelector('.page-settings'),
    };
    const navItems = document.querySelectorAll('.nav-item');

    navItems.forEach(item => {
        item.addEventListener('click', () => {
            const page = item.dataset.page;
            navItems.forEach(n => n.classList.toggle('active', n === item));
            Object.entries(pages).forEach(([k, el]) => {
                if (el) el.style.display = (k === page) ? 'flex' : 'none';
            });
            window.dispatchEvent(new Event('resize'));
        });
    });

    // ---------- 命令包装 ----------
    function send(cmd, extra = {}) {
        if (window.copyTool && window.copyTool.send) {
            window.copyTool.send(cmd, extra);
        } else {
            console.log('[ui] send (no bridge):', cmd, extra);
        }
    }

    // ---------- 浏览 ----------
    document.getElementById('btnBrowseSrc')?.addEventListener('click', () => {
        send('browse', { target: 'src' });
    });
    document.getElementById('btnBrowseDst')?.addEventListener('click', () => {
        send('browse', { target: 'dst' });
    });

    // ---------- 输入框 ----------
    document.getElementById('srcPath')?.addEventListener('input', (e) => {
        send('setSrcPath', { value: e.target.value });
    });
    document.getElementById('dstPath')?.addEventListener('input', (e) => {
        send('setDstPath', { value: e.target.value });
    });

    // ---------- 开始 / 暂停 / 终止 ----------
    document.getElementById('btnStart')?.addEventListener('click', () => {
        // 手输路径也要生效：点击开始前把两个文本框当前值同步给内核
        const sp = document.getElementById('srcPath').value.trim();
        const dp = document.getElementById('dstPath').value.trim();
        send('setSrcPath', { value: sp });
        send('setDstPath', { value: dp });
        send('start');
    });
    document.getElementById('btnPause')?.addEventListener('click', () => {
        send('pause');
    });
    document.getElementById('btnStop')?.addEventListener('click', () => {
        send('stop');
    });

    // ---------- 策略 ----------
    document.getElementById('strategySelect')?.addEventListener('change', (e) => {
        send('setStrategy', { value: parseInt(e.target.value, 10) });
    });

    // ---------- 校验（仅主页一个入口；设置页的同名控件已删除） ----------
    document.getElementById('verifyCheck')?.addEventListener('change', (e) => {
        send('setVerify', { value: e.target.checked ? 1 : 0 });
    });

    // ---------- ★ 语言切换：立即预览 + 保存时告知 C++ ----------
    const langSelect = document.getElementById('langSelect');
    if (langSelect) {
        // 切换时立即预览（用户能看到界面立刻变化）
        langSelect.addEventListener('change', (e) => {
            applyLang(parseInt(e.target.value, 10));
        });
        // 初始化：用当前值应用一次
        applyLang(parseInt(langSelect.value, 10));
    }

    // ---------- 设置页按钮 ----------
    document.getElementById('btnSave')?.addEventListener('click', () => {
        const speed = parseInt(document.getElementById('speedInput').value, 10) || 0;
        const unit = parseInt(document.getElementById('unitSelect').value, 10);
        const lang = parseInt(document.getElementById('langSelect').value, 10);
        send('setSpeedLimit', { value: speed, unit });
        send('setLang', { value: lang });
        applyLang(lang); // 双保险：保存时再应用一次
        // 同步本地"已保存值"缓存，保证随后"取消"能回退到本次保存的值。
        // 校验开关只在主页，用主页那个勾选框的值做缓存。
        const kb = speed === 0 ? 0 :
            (unit === 1 ? speed * 1024 : (unit === 2 ? speed * 1024 * 1024 : speed));
        const verifyHome = document.getElementById('verifyCheck');
        lastConfig = Object.assign({}, lastConfig, {
            lang, speed: kb,
            verify: verifyHome && verifyHome.checked ? 1 : 0
        });
        document.querySelector('.nav-item[data-page="home"]')?.click();
    });

    document.getElementById('btnCancel')?.addEventListener('click', () => {
        // 取消预览：语言/速度/校验全部回退到内核当前值。
        // 若启动时 config 尚未到达（竞态），向内核拉一次，onConfig 会完成回退。
        if (lastConfig) {
            if (langSelect) langSelect.value = String(lastConfig.lang != null ? lastConfig.lang : 0);
            if (lastConfig.speed != null) {
                const sp = document.getElementById('speedInput');
                const un = document.getElementById('unitSelect');
                if (sp && un) {
                    if (lastConfig.speed > 0) { sp.value = String(lastConfig.speed); un.value = '0'; }
                    else { sp.value = '0'; un.value = '1'; }
                }
            }
            // 校验开关只在主页
            const verify = !!lastConfig.verify;
            const vh = document.getElementById('verifyCheck');
            if (vh) vh.checked = verify;
            applyLang(lastConfig.lang != null ? lastConfig.lang : 0);
        } else {
            send('getConfig');
        }
        document.querySelector('.nav-item[data-page="home"]')?.click();
    });

    // ---------- 弹窗 ----------
    const dialogOverlay = document.getElementById('dialogOverlay');
    const dialogIcon = document.getElementById('dialogIcon');
    const dialogTitle = document.getElementById('dialogTitle');
    const dialogMessage = document.getElementById('dialogMessage');

    document.getElementById('dialogOk')?.addEventListener('click', () => {
        dialogOverlay.style.display = 'none';
    });

    function showDialog(title, message, kind = 'info') {
        dialogIcon.textContent = kind === 'error' ? '!' : (kind === 'warn' ? '!' : 'i');
        dialogIcon.className = 'dialog-icon ' + (kind === 'error' ? 'error' : (kind === 'warn' ? 'warn' : ''));
        dialogTitle.textContent = title || 'CopyTool';
        dialogMessage.textContent = message || '';
        dialogOverlay.style.display = 'flex';
    }

    // ---------- 接收 C++ 状态 ----------
    function onStatus(msg) {
        const pct = Math.max(0, Math.min(100, msg.progressPct || 0));
        const copying = !!msg.isCopying;
        const paused = !!msg.isPaused;
        document.getElementById('progressFill').style.width = pct + '%';
        document.getElementById('statusPercent').textContent = pct + '%';
        // 空闲时内核 statusText 是旧语言缓存（SetLanguage 不会刷新它），
        // 空闲状态统一用前端本地化的"准备就绪"；复制中的内核状态文本
        // 会按当前语言实时生成，直接使用。
        const stEl = document.getElementById('statusText');
        if (copying && paused) {
            // 内核 GetStatus 在复制中会用进度文本覆盖"已暂停"状态，
            // 由前端统一显示本地化的暂停态。
            stEl.textContent = t('status.paused') + ' ' + pct + '%';
        } else if (copying) {
            stEl.textContent = msg.statusText || t('status.ready');
        } else {
            stEl.textContent = t('status.ready');
        }

        lastStatus.isCopying = copying;
        lastStatus.isPaused = paused;
        const btnStart = document.getElementById('btnStart');
        const btnPause = document.getElementById('btnPause');
        const btnStop  = document.getElementById('btnStop');
        if (btnStart) btnStart.disabled = copying;
        if (btnPause) {
            btnPause.disabled = !copying;
            btnPause.textContent = paused ? t('btn.resume') : t('btn.pause');
        }
        if (btnStop) btnStop.disabled = !copying;
    }

    function onConfig(msg) {
        lastConfig = msg;
        if (msg.src != null) document.getElementById('srcPath').value = msg.src;
        if (msg.dst != null) document.getElementById('dstPath').value = msg.dst;
        if (msg.strategy != null) document.getElementById('strategySelect').value = String(msg.strategy);
        // 校验开关只在主页
        const verify = !!msg.verify;
        document.getElementById('verifyCheck').checked = verify;
        if (msg.speed != null) {
            const sp = document.getElementById('speedInput');
            const un = document.getElementById('unitSelect');
            if (sp && un) {
                if (msg.speed > 0) {
                    sp.value = String(msg.speed);
                    un.value = '0';
                } else {
                    sp.value = '0';
                    un.value = '1';
                }
            }
        }
        // ★ 回填语言并立即应用
        if (msg.lang != null && langSelect) {
            langSelect.value = String(msg.lang);
            applyLang(parseInt(msg.lang, 10));
        }
    }

    function onFinished(msg) {
        console.log('[onFinished] msg:', JSON.stringify(msg));
        if (msg.wasStopped) {
            showDialog(t('dialog.stopped'), t('dialog.stoppedMsg'), 'warn');
        } else if (msg.failedFiles > 0 || msg.verifyFailed > 0) {
            const total = (msg.failedFiles || 0) + (msg.verifyFailed || 0);
            const detail = formatFailList(msg.failList);
            showDialog(t('dialog.failed'), t('dialog.failedMsg', { n: total }) + detail, 'error');
        } else {
            showDialog(t('dialog.done'), t('dialog.doneMsg', { n: msg.totalFiles || 0 }), 'info');
        }
    }

    window.copyToolUI = { onStatus, onConfig, onFinished, showDialog };

    console.log('[ui] ready');
})();