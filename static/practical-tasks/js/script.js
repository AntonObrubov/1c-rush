CodeMirror.defineMode("1c", function() {
    const keywordsPurple = [
        "Если","Тогда","ИначеЕсли","Иначе","КонецЕсли",
        "Цикл","КонецЦикла","Для","Из","По","Пока","Прервать","Продолжить",
        "Процедура","КонецПроцедуры","Функция","КонецФункции",
        "Возврат","Экспорт","Попытка","Исключение","ВызватьИсключение","КонецПопытки",
        "Перем","Новый","NULL","ИСТИНА","ЛОЖЬ","Неопределено",
        "И","ИЛИ","НЕ"
    ];
    const keywordsBlue = [
        "ДляКаждого","Шаг","В","МЕЖДУ","ПОДОБНО","КАК",
        "Дата","Строка","Число","Булево","Массив","Структура",
        "Соответствие","ТаблицаЗначений",
        "Выбрать","ГДЕ","УПОРЯДОЧИТЬ","СГРУППИРОВАТЬ","ИМЕЮЩИЕ",
        "ОБЪЕДИНИТЬ","ВСЕ","ПЕРВЫЕ","ВНУТРЕННЕЕ","ЛЕВОЕ","ПРАВОЕ",
        "ПОЛНОЕ","СОЕДИНЕНИЕ","ЗНАЧЕНИЕ"
    ];
    const purpleSet = new Set(keywordsPurple.map(w => w.toLowerCase()));
    const blueSet = new Set(keywordsBlue.map(w => w.toLowerCase()));

    return {
        token: function(stream) {
            if (stream.eatSpace()) return null;
            if (stream.match("//")) { stream.skipToEnd(); return "comment"; }
            if (stream.match("/*")) {
                let ch;
                while ((ch = stream.next()) != null) {
                    if (ch === "*" && stream.peek() === "/") { stream.next(); break; }
                }
                return "comment";
            }
            if (stream.peek() === '"') {
                stream.next();
                let escaped = false;
                while (!stream.eol()) {
                    let ch = stream.next();
                    if (ch === '"' && !escaped) break;
                    if (ch === '\\') escaped = !escaped;
                    else escaped = false;
                }
                return "string-double";
            }
            if (stream.peek() === "'") {
                stream.next();
                let escaped = false;
                while (!stream.eol()) {
                    let ch = stream.next();
                    if (ch === "'" && !escaped) break;
                    if (ch === '\\') escaped = !escaped;
                    else escaped = false;
                }
                return "string-single";
            }
            if (stream.match(/^-?\d+(\.\d+)?/)) return "number";
            if (stream.match(/^#\d{4}-\d{2}-\d{2}#/)) return "atom";

            if (stream.match(/[A-Za-zА-Яа-я_][A-Za-zА-Яа-я0-9_]*/)) {
                const word = stream.current();
                const lowerWord = word.toLowerCase();
                if (purpleSet.has(lowerWord)) return "keyword-purple";
                if (blueSet.has(lowerWord)) return "keyword-blue";
                return "variable";
            }

            if (stream.match(/[=+\-*/%<>!&|~^?]/)) return "operator";
            if (stream.match(/[(){}[\].,;:]/)) return "operator";

            stream.next();
            return null;
        },
        lineComment: "//"
    };
});
CodeMirror.defineMIME("text/x-1c", "1c");

function shuffleArray(arr) {
    for (let i = arr.length - 1; i > 0; i--) {
        const j = Math.floor(Math.random() * (i + 1));
        [arr[i], arr[j]] = [arr[j], arr[i]];
    }
    return arr;
}

const tasksWithId = TASKS.map((task, idx) => ({ ...task, originalIndex: idx }));
let shuffledTasks = shuffleArray([...tasksWithId]);
const totalTasks = TASKS.length;

let currentIndex = 0;
let editor = null;
let hintEditor = null;
let lastCheckWrong = false;

let solvedTasks = new Set();

function isAuthorized() {
    return !!localStorage.getItem('user_id');
}

function getApiBase() {
    return window.API_BASE || '/api';
}

async function loadSolvedTasks() {
    solvedTasks.clear();
    if (!isAuthorized()) return;
    const userId = localStorage.getItem('user_id');
    try {
        const res = await fetch(`${getApiBase()}/solved-tasks?user_id=${userId}`);
        const data = await res.json();
        if (data.success && data.tasks) {
            data.tasks.forEach(origIdx => solvedTasks.add(origIdx));
        }
    } catch (e) {
        console.warn('Не удалось загрузить решённые задачи:', e);
    }
}

function isTaskSolved() {
    const task = shuffledTasks[currentIndex];
    return task ? solvedTasks.has(task.originalIndex) : false;
}

let taskStatusDiv = null;

function updateTaskStatus() {
    if (!taskStatusDiv) return;
    if (isTaskSolved()) {
        taskStatusDiv.innerHTML = '<span style="color:#4ade80;">✔ Задание решено</span>';
    } else {
        taskStatusDiv.innerHTML = '';
    }
}

const prevBtn = document.getElementById('prevBtn');
const nextBtn = document.getElementById('nextBtn');
const checkBtn = document.getElementById('checkBtn');
const resetBtn = document.getElementById('resetBtn');
const fullHintBtn = document.getElementById('fullHintBtn');
const resultDiv = document.getElementById('resultArea');
const fullHintArea = document.getElementById('fullHintArea');
const fullHintContent = document.getElementById('fullHintContent');
const currentIndexSpan = document.getElementById('currentIndexSpan');
const totalTasksSpan = document.getElementById('totalTasksSpan');
const taskTitle = document.getElementById('taskTitle');

function normalizeCode(str) {
    return str.replace(/\s+/g, '').toLowerCase();
}

function setResult(message, type) {
    resultDiv.innerHTML = `<span>${message}</span>`;
    resultDiv.className = 'result-box';
    if (type) resultDiv.classList.add(type);
}

function updateNavButtons() {
    prevBtn.disabled = (currentIndex === 0);
    nextBtn.disabled = (currentIndex === shuffledTasks.length - 1);
    currentIndexSpan.textContent = currentIndex + 1;
    totalTasksSpan.textContent = shuffledTasks.length;
}

function loadTask(index) {
    const task = shuffledTasks[index];
    if (!task) return;
    editor.setValue(task.originalCode);
    fullHintArea.style.display = "none";
    taskTitle.textContent = `Задание: ${task.name}`;
    setResult('Найдите ошибки в коде и исправьте их', 'result-info');
    lastCheckWrong = false;
    updateNavButtons();
    updateTaskStatus();
    editor.focus();
}

function checkCurrentCode() {
    const userCode = editor.getValue();
    const task = shuffledTasks[currentIndex];
    const normalizedUser = normalizeCode(userCode);
    const normalizedCorrect = normalizeCode(task.fullHint);
    if (normalizedUser === normalizedCorrect) {
        setResult('Задание выполнено успешно!', 'result-success');
        lastCheckWrong = false;

        if (!isTaskSolved()) {
            solvedTasks.add(task.originalIndex);
            updateTaskStatus();
            if (typeof window.incrementTasksSolved === 'function') {
                try {
                    window.incrementTasksSolved(task.originalIndex);
                } catch (e) {
                    console.warn('Ошибка при вызове incrementTasksSolved:', e);
                }
            }
        }
    } else {
        if (lastCheckWrong) blinkError();
        setResult('Ответ неверный. Попробуйте снова или воспользуйтесь сбросом / показать решение', 'result-error');
        lastCheckWrong = true;
    }
}

function blinkError() {
    let count = 0;
    const interval = setInterval(() => {
        resultDiv.classList.toggle('result-error-blink');
        count++;
        if (count >= 6) {
            clearInterval(interval);
            resultDiv.classList.remove('result-error-blink');
        }
    }, 150);
}

function resetTask() {
    const task = shuffledTasks[currentIndex];
    editor.setValue(task.originalCode);
    setResult('Код был восстановлен до базового состояния', 'result-info');
    fullHintArea.style.display = "none";
    lastCheckWrong = false;
    editor.focus();
}

function fullHint() {
    const task = shuffledTasks[currentIndex];
    fullHintArea.style.display = "block";
    if (!hintEditor) {
        const hintDiv = document.createElement('div');
        hintDiv.id = 'staticHintEditor';
        fullHintContent.innerHTML = '';
        fullHintContent.appendChild(hintDiv);
        hintEditor = CodeMirror(hintDiv, {
            value: task.fullHint,
            mode: "1c",
            theme: "monokai",
            readOnly: true,
            lineNumbers: false,
            lineWrapping: true
        });
        hintEditor.setSize("100%", "auto");
    } else {
        hintEditor.setValue(task.fullHint);
    }
    setTimeout(() => hintEditor.refresh(), 10);
}

function copyText(text, buttonElement) {
    if (navigator.clipboard && window.isSecureContext) {
        navigator.clipboard.writeText(text).then(() => {
            if (buttonElement) showCopied(buttonElement);
        }).catch(() => {
            fallbackCopy(text, buttonElement);
        });
    } else {
        fallbackCopy(text, buttonElement);
    }
}

function fallbackCopy(text, buttonElement) {
    const textarea = document.createElement('textarea');
    textarea.value = text;
    textarea.style.position = 'fixed';
    textarea.style.top = '-10000px';
    textarea.style.left = '-10000px';
    document.body.appendChild(textarea);
    textarea.select();
    try {
        const success = document.execCommand('copy');
        if (success && buttonElement) showCopied(buttonElement);
        else if (!success) console.warn('Copy failed');
    } catch (e) {
        console.warn('Copy error:', e);
    }
    document.body.removeChild(textarea);
}

function showCopied(buttonElement) {
    const img = buttonElement.querySelector('img');
    if (img) {
        const originalSrc = img.src;
        img.src = 'img/mark.png';
        setTimeout(() => { img.src = originalSrc; }, 1500);
    }
}

function copyHintCode() {
    const task = shuffledTasks[currentIndex];
    const btn = document.querySelector('.hint-copy-btn');
    if (btn) copyText(task.fullHint, btn);
}

function initCopyButtons() {
    document.querySelectorAll('.editor-block .copy-btn').forEach(btn => {
        btn.addEventListener('click', function(e) {
            e.stopPropagation();
            const block = this.closest('.editor-block');
            if (!block) return;
            const textarea = block.querySelector('textarea');
            if (!textarea) return;
            let code = '';
            const cmInstance = block.querySelector('.CodeMirror');
            if (cmInstance) {
                const editor = cmInstance.CodeMirror;
                if (editor && typeof editor.getValue === 'function') {
                    code = editor.getValue();
                }
            }
            if (!code) code = textarea.value;
            copyText(code, this);
        });
    });
}

function nextTask() {
    if (currentIndex < shuffledTasks.length - 1) {
        currentIndex++;
        loadTask(currentIndex);
    }
}

function prevTask() {
    if (currentIndex > 0) {
        currentIndex--;
        loadTask(currentIndex);
    }
}

window.onload = async function() {
    localStorage.setItem('total_tasks_count', totalTasks);
    await loadSolvedTasks();

    const textarea = document.getElementById("codeEditor");
    editor = CodeMirror.fromTextArea(textarea, {
        lineNumbers: true,
        mode: "1c",
        theme: "monokai",
        styleActiveLine: true,
        matchBrackets: true,
        indentUnit: 4,
        lineWrapping: true,
        autofocus: true,
        extraKeys: { "Tab": cm => cm.replaceSelection("    ", "end") }
    });

    totalTasksSpan.textContent = shuffledTasks.length;

    const progressDiv = document.querySelector('.progress');
    taskStatusDiv = document.createElement('div');
    taskStatusDiv.className = 'task-status';
    taskStatusDiv.style.margin = '10px 0';
    taskStatusDiv.style.fontWeight = 'bold';
    progressDiv.parentNode.insertBefore(taskStatusDiv, progressDiv.nextSibling);

    loadTask(0);

    prevBtn.addEventListener("click", prevTask);
    nextBtn.addEventListener("click", nextTask);
    checkBtn.addEventListener("click", checkCurrentCode);
    resetBtn.addEventListener("click", resetTask);
    fullHintBtn.addEventListener("click", fullHint);

    const hintTitle = document.querySelector('.hint-title');
    if (hintTitle) {
        const copyBtn = document.createElement('button');
        copyBtn.className = 'copy-btn hint-copy-btn';
        copyBtn.title = 'Скопировать правильный код';
        copyBtn.innerHTML = '<img src="img/copy.png" alt="Копировать" width="20" height="20"> Copy';
        copyBtn.addEventListener('click', copyHintCode);
        hintTitle.appendChild(copyBtn);
    }

    initCopyButtons();
};
