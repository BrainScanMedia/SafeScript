#include <QApplication>
#include <QSharedMemory>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QIcon>
#include "mainwindow.h"
#include "databasemanager.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("SafeScript");
    app.setOrganizationName("BrainScanMedia");
    app.setStyle("Fusion");
    app.setWindowIcon(QIcon(":/safescript.png"));

    // Single instance check — skip gracefully if shared memory fails (e.g. Flatpak)
    QSharedMemory sharedMemory("SafeScript_UniqueInstanceKey");
    bool sharedMemoryAvailable = sharedMemory.create(1);
    if (!sharedMemoryAvailable) {
        // If it already exists, another instance is running
        if (sharedMemory.error() == QSharedMemory::AlreadyExists) {
            QDialog dlg;
            dlg.setWindowTitle("SafeScript Already Running");
            dlg.setFixedSize(300, 100);
            QVBoxLayout* layout = new QVBoxLayout(&dlg);
            QLabel* msg = new QLabel("SafeScript is already open.\nPlease check your taskbar.");
            msg->setAlignment(Qt::AlignLeft);
            layout->addWidget(msg);
            QPushButton* ok = new QPushButton("OK");
            ok->setFixedWidth(80);
            QObject::connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);
            QHBoxLayout* btnRow = new QHBoxLayout;
            btnRow->addStretch();
            btnRow->addWidget(ok);
            layout->addLayout(btnRow);
            dlg.exec();
            return 0;
        }
        // Otherwise shared memory just isn't supported (Flatpak sandbox), continue anyway
    }

    if (!DatabaseManager::instance().open()) return 1;
    MainWindow w;
    w.show();
    return app.exec();
}
