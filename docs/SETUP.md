# Настройка окружения

Для работы на курсе вам потребуется машина с установленной операционной системой Linux.

Можно использовать Windows, но для компиляции и запуска потребуется WSL (Windows Subsystem for Linux). В этом случае сначала установите WSL, а дальшейшие инструкции выполняйте уже в Linux окружении.

Рекомендуется использовать Ubuntu версии **24.04**. Тестирующая система проверяет задачи на этой
версии убунты, и если вы будете использовать эту же версию локально, то вы избежите
проблем со старыми версиями компиляторов.

## Установите зависимости
  Нужны: компилятор gcc (версии минимум 13) или clang (версии минимум 19), cmake и tbb.
  Опционально: gdb - отладчик, ninja - альтернатива make, clang-format и clang-tidy - утилиты для проверки на соответствие кода стайлгайду.
   * **Ubuntu**:
      - **g++-13**
         ```bash
         $ sudo apt install g++-13
         ```
      - **clang++-19**
         ```bash
         $ sudo apt install clang-19
         ```
      - **Остальное**
         ```
         $ sudo apt install git gdb cmake ninja-build clang-format-19 clang-tidy-19 libtbb-dev libclang-19-dev libgoogle-glog-dev
         ```
   * **Windows**: установите [WSL](https://learn.microsoft.com/en-us/windows/wsl/install) и следуйте инструкции для Ubuntu.

## Регистрация в системе

1. Зарегистрируйтесь в системе: https://cpp.manytask.org/. Код регистрации доступен [на странице курса](https://lk.yandexdataschool.ru/courses/2025-spring/7.1328-obuchenie-iazyku-cpp-chast-2/). Если вы ранее регистрировались в этой системе для прохождения других курсов, то сразу можете жать кнопку **Login**.
2. Сгенерируйте ssh ключ, если у вас его еще нет.
   ```bash
   $ ssh-keygen -N "" -f ~/.ssh/id_rsa
   ```
3. Скопируйте содержимое файла id_rsa.pub (`cat ~/.ssh/id_rsa.pub`) в https://gitlab.manytask.org/-/profile/keys
4. Проверьте, что ssh ключ работает. Выполните команду `ssh git@gitlab.manytask.org`. Вы должны увидеть такое приветствие:
   ```bash
   $ ssh git@gitlab.manytask.org
   PTY allocation request failed on channel 0
   Welcome to GitLab, Fedor Korotkiy!
   Connection to gitlab.manytask.org closed.
   ```

## Настройка локального репозитория
   1. На странице https://cpp.manytask.org/ нажмите на **MY REPO**
   2. Нажмите на синию кнопку **Clone** и скопируйте SSH адрес
   3. Используя консоль склонируйте и настройте репозиторий с задачами:
   ```bash
   $ git clone "<Полученный SSH адрес>"

   # Переходим в директорию склонированного репозитория
   $ cd "<Склонированный репозиторий>"

   # Настраиваем пользователя
   $ git config --local user.name "<логин с cpp.manytask.org>"
   $ git config --local user.email "<email с cpp.manytask.org>"

   # Настраиваем возможность получать обновления из публичного репозитория с задачами
   $ git remote add upstream git@gitlab.manytask.org:cpp/public-2025-spring.git
   ```

   В течении всего курса публичный репозиторий будет обновлятся, в нём будут появлятся новый задачи, а также возможно будут обновляться тесты и условия существующих задач. Подтянуть изменения из публичного рупозитория можно с помощью команды `git pull upstream main`

## Посылка тестовой задачи в систему

1. Настройте IDE. Если вы используйте Windows, то вам также надо настроить WSL:
   * CLion
      - \[Windows\] [Настройка WSL](https://www.jetbrains.com/help/clion/how-to-use-wsl-development-environment-in-clion.html)
      - Настройку можно посмотреть в [записи семинара](https://disk.yandex.ru/i/8waWBV-L-FOKOw)
   * VS Code
      - \[Windows\] [Настройка WSL](https://code.visualstudio.com/docs/cpp/config-wsl)
      - Настройка описана в [инструкции](https://docs.google.com/document/d/1K0t05Bmqb3he3gW4ORQXfkVfFouS4FRT)

2. Решите первую задачу [range-sum](../range-sum).

3. Сдаваемый в систему код должен соответствовать нашему стайлгайду. Для локальной проверки перейдите в билд-директорию и выполните
    ```(bash)
    ../run_linter.sh <имя задачи>
    ```

4. Сдайте тестовое задание в систему:
```bash
# Находясь в корне репозитория
$ git add range-sum/range_sum.cpp
$ git commit -m "Add range-sum task"
$ git push origin main
```

5. Пронаблюдайте за процессом тестирования на странице CI/CD -> Pipelines своего репозитория.
gitlab показывает вывод консоли во время тестирования.

6. Проверьте, что оценка появилась в [таблице с результатами](https://docs.google.com/spreadsheets/d/12gsqmF_fxO2XrMzP1J5V9oako1nVZi8iv4MMymu5D5E)
