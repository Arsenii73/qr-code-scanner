#Якщо macOS блокує запуск

Після завантаження проєкту macOS може показати повідомлення, що Apple не може перевірити qr-scanner. Для отриманої від авторів копії програми можна окремо дозволити запуск.

У вікні попередження натисніть «Готово».

Треба відкрити  → Системні параметри → Приватність і безпека.

Знайдіть повідомлення про блокування qr-scanner та натисніть «Відкрити попри це».



# Real-Time QR Code Scanner

Навчальний проєкт мовою **C, стандарт C17**, який зчитує QR-коди та товарні штрихкоди за допомогою камери комп’ютера.

**Автори** — Арсеній Філончук та Владислав Шинкаренко.
# Мета проєкту

Створити програму на C, яка зчитує коди з екрана телефона, паперу та етикеток товарів у реальному часі.

Проєкт демонструє роботу з бібліотеками через C API, обробку зображень, керування пам’яттю, події графічного інтерфейсу та файлові операції.



## Можливості

- Сканування QR, EAN-13, EAN-8, UPC-A, UPC-E та Code 128.
- Попередній перегляд відео й відображення результату.
- Копіювання даних у буфер обміну.
- Пауза та відновлення сканування.
- Збереження історії у `scan_history.tsv`.
- Зчитування кодів із BMP-зображень.

## Технології

| Компонент | Призначення                                           |
| --------- | ----------------------------------------------------- |
| C17       | Логіка програми, обробка зображень і робота з файлами |
| SDL3      | Камера, графічний інтерфейс і керування               |
| ZBar      | Пошук та декодування кодів                            |

Програма отримує кадр, перетворює його у відтінки сірого та передає в ZBar. Якщо початкова спроба не знаходить код, програма додатково обробляє зображення й повторює сканування.

## Встановлення та запуск

Перевірене середовище — **macOS на Apple Silicon**. Для збірки потрібні Command Line Tools і Homebrew. [Встановлення Homebrew](https://brew.sh/)

Відкрийте термінал у папці з `Makefile` та виконайте:

```sh
brew install sdl3 zbar pkgconf
make -B
./qr-scanner
```

Під час першого запуску дозвольте доступ до камери.

Якщо macOS блокує отриману від авторів збірку, підтвердьте її відкриття через розділ «Приватність і безпека».

## Керування

| Кнопка або клавіша          | Дія                                |
| --------------------------- | ---------------------------------- |
| Copy result або `C`         | Копіювання результату              |
| Pause / Resume або `Пробіл` | Пауза та відновлення розпізнавання |
| Close або `Esc`             | Завершення роботи                  |

Під час паузи камера продовжує показувати відео.

## Перевірка без камери

```sh
make demo
```

Очікуваний результат:

```text
[URL] https://example.org/qr-demo
```

Перевірка товарного штрихкоду:

```sh
./qr-scanner --image demo/barcode-example.bmp --no-history
```

Очікуваний результат:

```text
[EAN-13] 5901234123457
```

Довідка та список камер:

```sh
./qr-scanner --help
./qr-scanner --list-cameras
```

## Структура коду

- `src/main.c` — запуск, камера та інтерфейс.
- `src/decoder.c` — обробка кадрів і взаємодія із ZBar.
- `src/results.c` — класифікація даних, копіювання та історія.
- `demo/` — зображення для перевірки.
- `Makefile` — збірка проєкту.

## Обмеження

Якість зчитування залежить від фокуса, освітлення та видимості всього коду. Режим обробки файлів підтримує BMP.

Панель результатів показує символи ASCII. Початковий український текст доступний через копіювання та в терміналі.

Історія містить повний вміст кодів. Щоб вимкнути її запис, запустіть:

```sh
./qr-scanner --no-history
```
ПЕРЕКЛАД
# If macOS blocks the application

After downloading the project, macOS may display a message stating that Apple cannot verify `qr-scanner`. If you received your copy directly from the project authors, you can manually allow it to run.

1. Click **Done** in the warning dialog.
2. Open ** → System Settings → Privacy & Security**.
3. Find the message about `qr-scanner` being blocked and click **Open Anyway**.

# Real-Time QR Code Scanner

An educational project written in **C using the C17 standard** that reads QR codes and product barcodes using a computer camera.

**Authors** — Arsenii Filonchuk and Vladyslav Shynkarenko.

## Project goal

Develop a C application that reads codes from phone screens, paper, and product labels in real time.

The project demonstrates integration with libraries through C APIs, image processing, memory management, graphical interface events, and file operations.

## Features

- Scanning QR, EAN-13, EAN-8, UPC-A, UPC-E, and Code 128.
- Live camera preview and scan result display.
- Copying decoded data to the clipboard.
- Pausing and resuming scanning.
- Saving scan history to `scan_history.tsv`.
- Reading codes from BMP images.

## Technologies

| Component | Purpose |
| --- | --- |
| C17 | Application logic, image processing, and file operations |
| SDL3 | Camera access, graphical interface, and controls |
| ZBar | Code detection and decoding |

The application captures a frame, converts it to grayscale, and passes it to ZBar. If the initial attempt does not detect a code, the application applies additional image processing and tries scanning again.

## Installation and launch

The tested environment is **macOS on Apple Silicon**. Building the project requires Xcode Command Line Tools and Homebrew. [Install Homebrew](https://brew.sh/)

Open a terminal in the directory containing `Makefile` and run the following commands.

```sh
brew install sdl3 zbar pkgconf
make -B
./qr-scanner
```

Allow camera access when prompted during the first launch.

If macOS blocks the build received from the project authors, allow it to open through **Privacy & Security**.

## Controls

| Button or key | Action |
| --- | --- |
| Copy result or `C` | Copy the result |
| Pause / Resume or `Space` | Pause or resume recognition |
| Close or `Esc` | Exit the application |

The camera continues displaying video while scanning is paused.

## Testing without a camera

```sh
make demo
```

Expected output

```text
[URL] https://example.org/qr-demo
```

Test the sample product barcode.

```sh
./qr-scanner --image demo/barcode-example.bmp --no-history
```

Expected output

```text
[EAN-13] 5901234123457
```

Display help and list available cameras.

```sh
./qr-scanner --help
./qr-scanner --list-cameras
```

## Code structure

- `src/main.c` — application startup, camera handling, and graphical interface.
- `src/decoder.c` — frame processing and integration with ZBar.
- `src/results.c` — data classification, copying, and scan history.
- `demo/` — sample images for testing.
- `Makefile` — project build instructions.

## Limitations

Recognition quality depends on focus, lighting, and visibility of the entire code. File scanning supports BMP images.

The result panel displays ASCII characters. The original Ukrainian text is available through clipboard copying and in the terminal.

Scan history stores the full contents of decoded codes. To disable history recording, run the following command.

```sh
./qr-scanner --no-history
```
