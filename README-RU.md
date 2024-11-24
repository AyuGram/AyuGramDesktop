# AyuGram

![AyuGram Лого](.github/AyuGram.png) ![AyuChan](.github/AyuChan.png)

[ [English](README.md)  | Русский ]

## Функции и Фишки

- Полный режим призрака (настраиваемый)
- История удалений и изменений сообщений
- Кастомизация шрифта
- Режим Стримера
- Локальный телеграм премиум
- Превью медиа и быстрая реакция при сильном нажатии на тачпад (macOS)
- Улучшенный вид

И многое другое. Посмотрите нашу [Документацию](https://docs.ayugram.one/desktop/) для более подробной информации.

<h3>
  <details>
    <summary>Скриншоты настроек</summary>
    <img src='.github/demos/demo1.png' width='268'>
    <img src='.github/demos/demo2.png' width='268'>
    <img src='.github/demos/demo3.png' width='268'>
    <img src='.github/demos/demo4.png' width='268'>
  </details>
</h3>

## Установка

### Windows

#### Официальный вариант

Вы можете скачать готовый бинарный файл со [вкладки Releases](https://github.com/AyuGram/AyuGramDesktop/releases) или из
[Телеграм чата](https://t.me/ayugramchat/12788).

#### Winget

```bash
winget install RadolynLabs.AyuGramDesktop
```

#### Scoop

```bash
scoop bucket add extras
scoop install ayugram
```

#### Сборка вручную

Следуйте [официальному руководству](https://github.com/AyuGram/AyuGramDesktop/blob/dev/docs/building-win-x64.md), если
вы хотите собрать AyuGram сами.

### Arch Linux

Вы можете установить `ayugram-desktop` или `ayugram-desktop-bin`
из [AUR](https://aur.archlinux.org/packages?O=0&K=ayugram).

### NixOS

Попробуйте [этот репозиторий](https://github.com/ayugram-port/ayugram-desktop).

### Любой другой Линукс дистрибутив

Следуйте [официальному руководству](https://github.com/AyuGram/AyuGramDesktop/blob/dev/docs/building-linux.md).

### Примечания для Windows

Убедитесь что у вас присутствуют эти зависимости:

- C++ MFC latest (x86 & x64)
- C++ ATL latest (x86 & x64)
- последний Windows 11 SDK

## Пожертвования

Вам нравится использовать **AyuGram**? Оставьте нам чаевые!

[Здесь доступные варианты.](https://docs.ayugram.one/donate/)

## Использованные материалы

### Телеграм клиенты

- [Telegram Desktop](https://github.com/telegramdesktop/tdesktop)
- [Kotatogram](https://github.com/kotatogram/kotatogram-desktop)
- [64Gram](https://github.com/TDesktop-x64/tdesktop)
- [Forkgram](https://github.com/forkgram/tdesktop)

### Использованные библиотеки

- [JSON for Modern C++](https://github.com/nlohmann/json)
- [SQLite](https://github.com/sqlite/sqlite)
- [sqlite_orm](https://github.com/fnc12/sqlite_orm)

### Иконки

- [Solar Icon Set](https://www.figma.com/community/file/1166831539721848736)
