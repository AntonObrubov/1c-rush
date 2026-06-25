# 1C-Rush

Учебный проект по дисциплине «Проектный практикум».  

Проект представляет собой backend-сервер для веб-приложения 1C-Rush с SQLite и разделением frontend/backend архитектуры.
Сам Front-end представляет из себя базовые знания в области 1С:Предприятие 8.3 

## Команда

| Роль | Участник |
|------|----------|
| Backend, API архитектура, Git, деплой | Антон Обрубов |
| Frontend (веб-приложение) | Антон Обрубов |
| Дизайн | Антон Обрубов, Данила Снастин |
| Тестирование / Документация | Антон Обрубов |

## Технологический стек

- C++17  
- cpp-httplib
- picosha2.h
- SQLite (файл tickets.db)  
- nlohmann/json  
- HTML + JavaScript  
- Git + GitHub  
- MSYS2 MinGW g++ (Windows)

### Клонирование репозитория

```bash
git clone https://github.com/AntonObrubov/1c-rush.git
cd 1c-rush/backend
```

## Старт сервера (локально)

### Требования!

- Установка [MSYS2](https://www.msys2.org/?utm_source=chatgpt.com) с пакетом `mingw-w64-ucrt-x86_64-gcc`
- g++ (C++17)
- [sqlite3.o](http://sqlite.org) (уже скомпилирован или получается командой `gcc -c sqlite3.c`)

```bash
g++ -std=c++17 main.cpp sqlite3.o -lws2_32 -lpthread -o server.exe
./server.exe
```

Сервер запускается на порту 8080, автоматически создаёт tickets.db.
Откройте в браузере [http://localhost:8080/](http://localhost:8080/)

## Старт сервера (через VPS)

Через аварийная консоль подключаемся к нашему VPS

1. Обновление и установка нужных пакетов
   
```bash
apt update && apt install -y g++ git ufw
```

2. Обновление и установка нужных пакетов
   
```bash
cd /opt
git clone https://github.com/AntonObrubov/ticket-server.git
cd ticket-server/backend
```

3. Сборка SQLite и сервера
   
```bash
gcc -c sqlite3.c -o sqlite3.o
g++ -std=c++17 main.cpp sqlite3.o -lpthread -o server
```

4. Запуск сервера
   
```bash
nohup ./server > server.log 2>&1 &
```

5. Вход по http

Открой в своём браузере http://00.000.000.000:8080/
Вместо 00.000.000.000 пропишите свой IP VPS

6. Отключение сервера (если необходимо)
   
6.1 Найди PID
  
```bash
ps aux | grep server
```

6.2 Выключить сервер (вместо 0000 прописываем ваш PID)
  
```bash
kill 0000
```


## Структура проекта
```
1c-rush
├── backend
│   ├── main.cpp
│   ├── picosha2.h
│   ├── server.exe
│   ├── sqlite3.c
│   ├── sqlite3.h
│   └── sqlite3.o
│
├── static
│   ├── css
│   │   └── main.css
│   ├── img
│   ├── js
│   │   └── app.js
│   ├── index.html
│   ├── login.html
│   ├── profile.html
│   ├── register.html
│   ├── practical-tasks
│   │   ├── css
│   │   │   └── style.css
│   │   ├── img
│   │   ├── js
│   │   │   └── script.js
│   │   ├── tasks
│   │   │   └── tasks.js
│   │   └── practical.html
│   └── textbook
│       ├── css
│       │   └── styles.css
│       ├── img
│       ├── js
│       │   └── script.js
│       └── pages
│           ├── array.html
│           ├── booleans.html
│           ├── conditions.html
│           ├── dates.html
│           ├── functions-procedure.html
│           ├── loops.html
│           ├── numbers.html
│           ├── strings.html
│           ├── type-conversion.html
│           └── variables.html
└── .gitignore
```
