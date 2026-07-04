cmake_minimum_required(VERSION 3.21)

project(password_manager
    VERSION 0.2.0
    DESCRIPTION "Standalone offline password manager for Windows"
    LANGUAGES CXX RC)

# ── C++ standard ─────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build the Qt GUI? Turn OFF to build/test just the headless core
# (crypto), e.g. to verify M1 without installing Qt:
#   cmake --preset default -DBUILD_APP=OFF
option(BUILD_APP "Build the Qt GUI application" ON)

# ── Security hardening (MSVC) ────────────────────────────────
# Control Flow Guard + ASLR/DEP/high-entropy ASLR. Defence-in-depth, and a
# hardened modern PE also looks less suspicious to AV / SmartScreen heuristics.
# Applies to every target defined below (lib, tests, app).
if(MSVC)
    add_compile_options(/guard:cf)
    add_link_options(/guard:cf /DYNAMICBASE /HIGHENTROPYVA /NXCOMPAT)
endif()

# ── Core library (no Qt): crypto + storage + vault ──────────
find_package(unofficial-sodium CONFIG REQUIRED)
find_package(sqlcipher CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
# TOTP needs HMAC-SHA1; OpenSSL is already in the dependency tree (and the
# shipped DLL set) as SQLCipher's crypto backend, so this adds no new runtime.
find_package(OpenSSL REQUIRED)

add_library(pm_core STATIC
    src/core/crypto/SecureBuffer.cpp
    src/core/crypto/Crypto.cpp
    src/core/crypto/Totp.cpp
    src/core/model/Entry.cpp
    src/core/storage/Database.cpp
    src/core/storage/Migrations.cpp
    src/core/storage/EntryRepository.cpp
    src/core/storage/FolderRepository.cpp
    src/core/storage/TagRepository.cpp
    src/core/storage/AttachmentRepository.cpp
    src/core/storage/SettingsRepository.cpp
    src/core/vault/VaultMeta.cpp
    src/core/vault/VaultSession.cpp
    src/core/vault/Backup.cpp
)
target_include_directories(pm_core PUBLIC src)
# sqlcipher and nlohmann_json are implementation details of the storage/vault
# layers (the public headers don't expose them), so link them PRIVATE.
target_link_libraries(pm_core
    PUBLIC unofficial-sodium::sodium
    PRIVATE sqlcipher::sqlcipher nlohmann_json::nlohmann_json OpenSSL::Crypto)
if(MSVC)
    target_compile_options(pm_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(pm_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ── GUI application (Qt) ─────────────────────────────────────
if(BUILD_APP)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    set(CMAKE_AUTOUIC ON)
    find_package(Qt6 REQUIRED COMPONENTS Widgets)

    add_executable(password_manager
        src/main.cpp
        resources/app.qrc          # Qt resource → window/taskbar icon (AUTORCC)
        resources/fonts.qrc        # Qt resource → bundled fonts (AUTORCC)
        resources/app.rc           # Windows resource → .exe icon in Explorer
        src/app/Theme.h
        src/app/Clipboard.cpp
        src/app/Clipboard.h
        src/app/CopyButton.cpp
        src/app/CopyButton.h
        src/app/FieldRow.cpp
        src/app/FieldRow.h
        src/app/TotpRow.cpp
        src/app/TotpRow.h
        src/app/EntryDetailView.cpp
        src/app/EntryDetailView.h
        src/app/EntryEditorDialog.cpp
        src/app/EntryEditorDialog.h
        src/app/MainWindow.cpp
        src/app/MainWindow.h
        src/app/NavRail.cpp
        src/app/NavRail.h
        src/app/UnlockView.cpp
        src/app/UnlockView.h
        src/app/VaultPage.cpp
        src/app/VaultPage.h
        src/app/GeneratorPage.cpp
        src/app/GeneratorPage.h
        src/app/SettingsPage.cpp
        src/app/SettingsPage.h
    )
    target_link_libraries(password_manager PRIVATE pm_core Qt6::Widgets)
    if(MSVC)
        target_compile_options(password_manager PRIVATE /W4 /permissive-)
    endif()

    # On Windows: GUI app (no console) in Release, keep console in Debug.
    if(WIN32)
        set_target_properties(password_manager PROPERTIES
            WIN32_EXECUTABLE $<NOT:$<CONFIG:Debug>>)

        # Copy the required Qt runtime DLLs next to the .exe after each build so
        # the app runs straight from the build tree (and is easy to package later).
        find_program(WINDEPLOYQT_EXE windeployqt HINTS "$ENV{QT_DIR}/bin")
        if(WINDEPLOYQT_EXE)
            add_custom_command(TARGET password_manager POST_BUILD
                COMMAND "${WINDEPLOYQT_EXE}" --no-translations --no-compiler-runtime
                        "$<TARGET_FILE:password_manager>"
                COMMENT "Deploying Qt runtime DLLs next to password_manager.exe")
        endif()
    endif()
endif()

# ── Tests (Catch2) ───────────────────────────────────────────
include(CTest)
if(BUILD_TESTING)
    find_package(Catch2 3 REQUIRED)
    add_executable(pm_tests
        tests/crypto_tests.cpp
        tests/totp_tests.cpp
        tests/storage_tests.cpp
        tests/migrations_tests.cpp
        tests/entry_repository_tests.cpp
        tests/folder_repository_tests.cpp
        tests/tag_repository_tests.cpp
        tests/attachment_repository_tests.cpp
        tests/settings_repository_tests.cpp
        tests/vault_tests.cpp
        tests/backup_tests.cpp
        tests/security_tests.cpp
    )
    # The storage tests drive the sqlite3 C API directly to set up / verify data,
    # so pm_tests links sqlcipher in addition to pm_core.
    target_link_libraries(pm_tests
        PRIVATE pm_core Catch2::Catch2WithMain sqlcipher::sqlcipher)
    add_test(NAME unit COMMAND pm_tests)
endif()
