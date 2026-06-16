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

function animateCodeInEditor(editor, code, onComplete) {
    let timeout = null;
    let i = 0;
    editor.setValue('');
    function typeNext() {
        if (i < code.length) {
            editor.setValue(code.substring(0, i+1));
            i++;
            timeout = setTimeout(typeNext, 40);
        } else {
            timeout = setTimeout(() => eraseCode(editor, code, onComplete), 1500);
        }
    }
    function eraseCode(cm, originalCode, callback) {
        let text = cm.getValue();
        let j = text.length;
        function eraseStep() {
            if (j > 0) {
                cm.setValue(text.substring(0, j-1));
                j--;
                timeout = setTimeout(eraseStep, 30);
            } else if (callback) callback();
        }
        eraseStep();
    }
    typeNext();
    return () => { if (timeout) clearTimeout(timeout); };
}

function startAnimationLoop(editor, examplesArray) {
    let currentIndex = 0;
    let active = true;
    let timeout = null;
    function nextExample() {
        if (!active) return;
        const code = examplesArray[currentIndex];
        animateCodeInEditor(editor, code, () => {
            currentIndex = (currentIndex + 1) % examplesArray.length;
            timeout = setTimeout(nextExample, 500);
        });
    }
    nextExample();
    return () => { active = false; if (timeout) clearTimeout(timeout); };
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
        img.src = '../img/mark.png';
        setTimeout(() => { img.src = originalSrc; }, 1500);
    }
}

document.addEventListener('DOMContentLoaded', function() {
    document.querySelectorAll('.editor-block').forEach(block => {
        const textarea = block.querySelector('textarea');
        if (!textarea) return;

        const isAnimated = block.classList.contains('animated');
        const editor = CodeMirror.fromTextArea(textarea, {
            mode: "1c",
            theme: "monokai",
            readOnly: !isAnimated,
            lineNumbers: true,
            lineWrapping: true,
            matchBrackets: true,
            viewportMargin: isAnimated ? 10 : Infinity,
            scrollbarStyle: isAnimated ? "native" : "null"
        });

        if (!isAnimated) {
            editor.setSize(null, 'auto');
            var scrollEl = editor.getScrollerElement();
            if (scrollEl) {
                scrollEl.style.height = 'auto';
                scrollEl.style.overflowY = 'visible';
            }
        }

        if (isAnimated && block.dataset.examples) {
            try {
                const examples = JSON.parse(block.dataset.examples);
                if (examples.length) startAnimationLoop(editor, examples);
            } catch(e) { console.warn("Ошибка data-examples", e); }
        }

        let header = block.querySelector('.code-header');
        if (!header) {
            header = document.createElement('div');
            header.className = 'code-header';
            header.innerHTML = '<span>1c</span>';
            block.insertBefore(header, block.firstChild);
        }

        const copyBtn = block.querySelector('.copy-btn');
        if (copyBtn) {
            header.appendChild(copyBtn);
            copyBtn.addEventListener('click', function(e) {
                e.stopPropagation();
                const cmInstance = block.querySelector('.CodeMirror');
                let code = '';
                if (cmInstance && cmInstance.CodeMirror) {
                    const editor = cmInstance.CodeMirror;
                    if (editor && typeof editor.getValue === 'function') {
                        code = editor.getValue();
                    }
                }
                if (!code) code = textarea.value;
                copyText(code, this);
            });
        }
    });

    document.querySelectorAll('.screenshot-demo .toggle-screenshot').forEach(btn => {
        const content = btn.parentElement.querySelector('.screenshot-content');
        if (!content) return;
        btn.addEventListener('click', function() {
            if (content.style.maxHeight && content.style.maxHeight !== '0px') {
                content.style.maxHeight = '0';
                content.style.marginTop = '0';
                btn.textContent = 'Показать результат';
            } else {
                content.style.maxHeight = content.scrollHeight + 'px';
                content.style.marginTop = '10px';
                btn.textContent = 'Скрыть результат';
            }
        });
    });

    document.querySelectorAll('.sidebar .dropdown-toggle').forEach(toggle => {
        const titleSpan = toggle.querySelector('span:first-child');
        const menuId = titleSpan ? titleSpan.innerText.trim() : 'default';
        const isOpen = localStorage.getItem(`dropdown_${menuId}`) === 'true';
        if (isOpen) {
            toggle.classList.add('open');
            const submenu = toggle.nextElementSibling;
            if (submenu && submenu.classList.contains('submenu')) {
                submenu.classList.add('open');
            }
        }
        toggle.addEventListener('click', function(e) {
            e.stopPropagation();
            this.classList.toggle('open');
            const submenu = this.nextElementSibling;
            if (submenu && submenu.classList.contains('submenu')) {
                submenu.classList.toggle('open');
            }
            const nowOpen = this.classList.contains('open');
            localStorage.setItem(`dropdown_${menuId}`, nowOpen);
        });
    });
});
