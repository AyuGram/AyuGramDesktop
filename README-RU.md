<div align="center">

# Drakeygram

**Независимый клиент Telegram Desktop — приватность, контроль, без стороннего трекинга**

</div>

---

## Зачем Drakeygram?

Это **самодостаточный** форк Telegram Desktop.  
Клиент **не** обращается к серверам AyuGram и сторонним config/CDN. Удалённые конфиги, внешние language pack и donate-трекинг отключены.

## Возможности

- Ghost Mode (read/online/typing/stories)
- Anti-recall и история правок сообщений
- Локальные фильтры
- Кастомизация шрифтов и внешнего вида
- Streamer mode
- Локальный Premium-вид
- Переводчик (по выбору пользователя)
- Умный forward и message shot

## Сборка

Как у официального [Telegram Desktop](https://github.com/telegramdesktop/tdesktop).  
Имя бинарника: **Drakeygram**. Desktop ID: `com.drakeygram.desktop`.

```bash
git clone --recursive https://github.com/sqdq2/Drakeygram.git
cd Drakeygram
```

## Независимость

- RCManager — без сети  
- Внешний language CDN — выключен  
- Брендинг — Drakeygram  

Папка `ayu/` в исходниках — только путь, не сетевая зависимость.

## Лицензия

GPL-3.0. Проект не связан с Telegram FZ-LLC.
