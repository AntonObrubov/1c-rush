const API_BASE = '/api';

const Auth = {
    isAuthenticated() { return !!localStorage.getItem('user_id'); },
    async register(email, password) {
        const res = await fetch(`${API_BASE}/register`, {
            method: 'POST', headers: {'Content-Type':'application/json'},
            body: JSON.stringify({email, password})
        });
        const data = await res.json();
        if (!data.success) throw new Error(data.error);
        return data;
    },
    async login(email, password) {
        const res = await fetch(`${API_BASE}/login`, {
            method: 'POST', headers: {'Content-Type':'application/json'},
            body: JSON.stringify({email, password})
        });
        const data = await res.json();
        if (!data.success) throw new Error(data.error);
        localStorage.setItem('user_id', data.user_id);
        localStorage.setItem('user_email', data.email);
        if (data.display_name) localStorage.setItem('user_display_name', data.display_name);
        if (data.avatar_base64) localStorage.setItem('user_avatar', data.avatar_base64);
        return data;
    },
    logout() {
        ['user_id','user_email','user_display_name','user_avatar'].forEach(k => localStorage.removeItem(k));
    }
};

function getDisplayName() {
    return localStorage.getItem('user_display_name') || (Auth.isAuthenticated() ? localStorage.getItem('user_email')?.split('@')[0] : 'Гость');
}

function updateAuthUI() {
    const authArea = document.getElementById('authArea');
    if (!authArea) return;
    if (Auth.isAuthenticated()) {
        const avatar = localStorage.getItem('user_avatar');
        const avatarHtml = avatar ? `<img src="${avatar}" alt="avatar">` : `<img src="img/user.png" alt="avatar">`;
        authArea.innerHTML = `<div class="user-badge"><div class="user-avatar">${avatarHtml}</div><span class="user-name">${getDisplayName()}</span><button id="logoutBtnHeader" class="logout-btn">Выйти</button></div>`;
        document.getElementById('logoutBtnHeader').onclick = () => { Auth.logout(); location.reload(); };
    } else {
        authArea.innerHTML = `<a href="/login.html">Вход</a> / <a href="/register.html">Регистрация</a>`;
    }
}

async function loadIndexPage() {
    const userId = localStorage.getItem('user_id');
    if (userId) {
        const res = await fetch(`${API_BASE}/profile?user_id=${userId}`);
        const data = await res.json();
        if (data.success) {
            document.getElementById('greetingMessage').innerHTML = `С возвращением, <strong>${data.display_name || data.email}</strong>!`;
            document.getElementById('daysCount').textContent = data.days_count;
            document.getElementById('tasksSolved').textContent = data.tasks_solved;
            document.getElementById('statsBlock').style.display = '';
            return;
        }
    }
    document.getElementById('greetingMessage').innerHTML = `Добро пожаловать! <a href="/login.html">Войдите</a>`;
    document.getElementById('statsBlock').style.display = 'none';
}

async function loadProfilePage() {
    const userId = localStorage.getItem('user_id');
    if (!userId) { window.location.href = '/login.html'; return; }
    const res = await fetch(`${API_BASE}/profile?user_id=${userId}`);
    const data = await res.json();
    if (!data.success) { alert(data.error); return; }
    document.getElementById('profileEmail').textContent = data.email;
    document.getElementById('profileSince').textContent = new Date(data.created_at).toLocaleDateString('ru-RU');
    document.getElementById('displayNameInput').value = data.display_name || data.email.split('@')[0];
    document.getElementById('profileTasksSolved').textContent = data.tasks_solved;
    document.getElementById('profileDaysCount').textContent = data.days_count;
    const total = parseInt(localStorage.getItem('total_tasks_count') || '10');
    const percent = total ? Math.min(100, Math.round((data.tasks_solved / total)*100)) : 0;
    document.getElementById('progressBar').style.width = percent+'%';
    document.getElementById('progressPercent').textContent = percent+'%';

    document.getElementById('saveNameBtn').onclick = async () => {
        const newName = document.getElementById('displayNameInput').value.trim();
        if (!newName) return;
        await fetch(`${API_BASE}/profile`, {
            method: 'PUT', headers: {'Content-Type':'application/json'},
            body: JSON.stringify({
                user_id: parseInt(userId),
                display_name: newName
            })
        });
        localStorage.setItem('user_display_name', newName);
        updateAuthUI();
        showToast('Имя сохранено');
    };
    document.getElementById('logoutBtnProfile').onclick = () => { Auth.logout(); window.location.href = '/index.html'; };

    const avatarImg = document.getElementById('avatarImg');
    const avatarUpload = document.getElementById('avatarUpload');
    const savedAvatar = localStorage.getItem('user_avatar');
    if (avatarImg) avatarImg.src = savedAvatar || 'img/user.png';
    if (avatarUpload) {
        avatarUpload.addEventListener('change', async (e) => {
            const file = e.target.files[0];
            if (!file || !['image/jpeg','image/png'].includes(file.type)) return;
            const reader = new FileReader();
            reader.onload = async (ev) => {
                const base64 = ev.target.result;
                await fetch(`${API_BASE}/profile`, {
                    method: 'PUT', headers: {'Content-Type':'application/json'},
                    body: JSON.stringify({
                        user_id: parseInt(userId),
                        avatar_base64: base64
                    })
                });
                localStorage.setItem('user_avatar', base64);
                if (avatarImg) avatarImg.src = base64;
                updateAuthUI();
                showToast('Аватар обновлён');
            };
            reader.readAsDataURL(file);
        });
    }
}

window.refreshStats = async function() {
    const userId = localStorage.getItem('user_id');
    if (!userId) return;
    const res = await fetch(`${API_BASE}/profile?user_id=${userId}`);
    const data = await res.json();
    if (!data.success) return;
    const daysEl = document.getElementById('daysCount');
    const tasksEl = document.getElementById('tasksSolved');
    if (daysEl) daysEl.textContent = data.days_count;
    if (tasksEl) tasksEl.textContent = data.tasks_solved;
    const profTasks = document.getElementById('profileTasksSolved');
    const profDays = document.getElementById('profileDaysCount');
    const progressBar = document.getElementById('progressBar');
    const progressPercent = document.getElementById('progressPercent');
    if (profTasks) profTasks.textContent = data.tasks_solved;
    if (profDays) profDays.textContent = data.days_count;
    if (progressBar) {
        const total = parseInt(localStorage.getItem('total_tasks_count') || '10');
        const percent = total ? Math.min(100, Math.round((data.tasks_solved / total)*100)) : 0;
        progressBar.style.width = percent+'%';
    }
    if (progressPercent) {
        const total = parseInt(localStorage.getItem('total_tasks_count') || '10');
        const percent = total ? Math.min(100, Math.round((data.tasks_solved / total)*100)) : 0;
        progressPercent.textContent = percent+'%';
    }
    const greeting = document.getElementById('greetingMessage');
    if (greeting && document.body.dataset.page === 'index') {
        greeting.innerHTML = `С возвращением, <strong>${data.display_name || data.email}</strong>!`;
    }
};

window.incrementTasksSolved = async function(taskIndex) {
    const userId = localStorage.getItem('user_id');
    if (!userId) return;
    await fetch(`${API_BASE}/task-solved/save`, {
        method: 'POST', headers: {'Content-Type':'application/json'},
        body: JSON.stringify({
            user_id: parseInt(userId),
            task_index: taskIndex
        })
    });
    if (typeof window.refreshStats === 'function') {
        await window.refreshStats();
    }
};

function initLoginPage() {
    const form = document.getElementById('loginForm');
    const errorEl = document.getElementById('loginError');
    if (!form) return;
    form.addEventListener('submit', async (e) => {
        e.preventDefault();
        const email = document.getElementById('email').value.trim();
        const password = document.getElementById('password').value;
        try {
            await Auth.login(email, password);
            window.location.href = '/index.html';
        } catch (err) {
            if (errorEl) errorEl.textContent = err.message;
        }
    });
}

function initRegisterPage() {
    const form = document.getElementById('registerForm');
    const errorEl = document.getElementById('registerError');
    if (!form) return;
    form.addEventListener('submit', async (e) => {
        e.preventDefault();
        const email = document.getElementById('email').value.trim();
        const password = document.getElementById('password').value;
        try {
            await Auth.register(email, password);
            showToast('Регистрация успешна! Сейчас войдите.');
            setTimeout(() => window.location.href = '/login.html', 1500);
        } catch (err) {
            if (errorEl) errorEl.textContent = err.message;
        }
    });
}

function showToast(text, color='#4ade80') {
    const toast = document.createElement('div');
    toast.className = 'custom-toast'; toast.textContent = text; toast.style.borderColor = color;
    document.body.appendChild(toast);
    setTimeout(() => toast.remove(), 2500);
}

function onDOMReady() {
    updateAuthUI();
    const page = document.body.dataset.page;
    if (page === 'index') loadIndexPage();
    else if (page === 'profile') loadProfilePage();
    else if (page === 'login') initLoginPage();
    else if (page === 'register') initRegisterPage();
}

document.addEventListener('DOMContentLoaded', onDOMReady);
