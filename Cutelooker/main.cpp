#include "MainWindow.h"

#include <clocale>
#include <cstdio>
#include <QApplication>

int main(int argc, char* argv[])
{
#ifdef _UCRT
    // Enable UTF-8 support in the MSVC runtime
    // https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/setlocale-wsetlocale?view=msvc-160#utf-8-support
    setlocale(LC_ALL, ".utf8");
    setlocale(LC_NUMERIC, "C");
#endif // _UCRT

    // Disable buffering because stdout/stderr are only used for debugging
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);

    // Hopefully enable high DPI support
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    // Create application
    QApplication app(argc, argv);
    QApplication::setOrganizationName("Denuvo");
    QApplication::setApplicationName("Cutelooker");

    // Create main window
    MainWindow w;

    // Show main window
    w.show();

    // Run application
    return QApplication::exec();
}

// Support /SUBSYSTEM:WINDOWS
#ifdef _WIN32

#include <Windows.h>
#include <shellapi.h>

int WINAPI CALLBACK WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nShowCmd)
{
    // https://utf8everywhere.org/
    int argc = 0;
    auto argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::vector<QByteArray> argv_utf8;
    for (int i = 0; i < argc; i++)
        argv_utf8.push_back(QString::fromUtf16((const char16_t*)argv[i]).toUtf8());
    LocalFree(argv);
    std::vector<char*> argv_main;
    for (auto& arg : argv_utf8)
        argv_main.push_back(arg.data());
    return main(argc, argv_main.data());
}

#endif // _WIN32