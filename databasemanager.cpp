#include "databasemanager.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QDebug>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>

DatabaseManager& DatabaseManager::instance() {
    static DatabaseManager mgr;
    return mgr;
}

bool DatabaseManager::open() {
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(appDataPath);
    dbPath = appDataPath + "/storage.sqlite3";
    return openConnection();
}

QString DatabaseManager::databasePath() const {
    return dbPath;
}

// ── Connection helpers ─────────────────────────────────────────
bool DatabaseManager::openConnection() {
    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(dbPath);

    if (!db.open()) {
        qWarning() << "Could not open database:" << db.lastError().text();
        return false;
    }

    createTablesIfNeeded();
    return true;
}

void DatabaseManager::closeConnection() {
    QString conn;
    if (db.isValid()) {
        conn = db.connectionName();
        db.close();
    }
    // Release our handle before removing so Qt doesn't warn that the
    // connection is still in use.
    db = QSqlDatabase();
    if (!conn.isEmpty())
        QSqlDatabase::removeDatabase(conn);
}

// ── Validation ─────────────────────────────────────────────────
// Opens the candidate file in a throwaway connection and confirms it
// looks like a SafeScript database (has the expected tables).
bool DatabaseManager::isValidDatabase(const QString& path, QString* errorMessage) {
    bool valid = false;
    const QString connName = "safescript_validate_connection";
    {
        QSqlDatabase test = QSqlDatabase::addDatabase("QSQLITE", connName);
        test.setDatabaseName(path);
        if (test.open()) {
            QStringList tables = test.tables();
            if (tables.contains("folders") && tables.contains("snippets")) {
                valid = true;
            } else if (errorMessage) {
                *errorMessage = "The selected file is a database but does not "
                                "contain SafeScript data (missing the folders "
                                "and snippets tables).";
            }
            test.close();
        } else if (errorMessage) {
            *errorMessage = "The selected file could not be opened as a "
                            "database: " + test.lastError().text();
        }
    }
    QSqlDatabase::removeDatabase(connName);
    return valid;
}

// ── Backup ─────────────────────────────────────────────────────
// Closes the live connection, copies the db file to destPath, then
// reopens so the app stays usable regardless of outcome.
bool DatabaseManager::backupTo(const QString& destPath, QString* errorMessage) {
    closeConnection();

    bool ok = true;
    QString err;

    if (!QFile::exists(dbPath)) {
        err = "There is no database file to back up yet.";
        ok = false;
    }

    // QFile::copy will not overwrite, so clear any existing destination.
    if (ok && QFile::exists(destPath)) {
        if (!QFile::remove(destPath)) {
            err = "Could not overwrite the existing file at the destination. "
                  "Check that you have permission to write there.";
            ok = false;
        }
    }

    if (ok && !QFile::copy(dbPath, destPath)) {
        err = "Failed to write the backup file. Check that you have permission "
              "to write to that location.";
        ok = false;
    }

    // Always reopen the live database.
    openConnection();

    if (!ok && errorMessage) *errorMessage = err;
    return ok;
}

// ── Import ─────────────────────────────────────────────────────
// Validates srcPath, then replaces the live database with it. Keeps a
// rollback copy so a failed copy can never leave the app without data.
bool DatabaseManager::importFrom(const QString& srcPath, QString* errorMessage) {
    QString err;

    if (!QFile::exists(srcPath)) {
        if (errorMessage) *errorMessage = "The selected file does not exist.";
        return false;
    }

    if (!isValidDatabase(srcPath, &err)) {
        if (errorMessage)
            *errorMessage = err.isEmpty()
                ? "The selected file is not a valid SafeScript database."
                : err;
        return false;
    }

    closeConnection();

    // Stash the current database so we can restore it if the copy fails.
    const QString rollbackPath = dbPath + ".importbak";
    QFile::remove(rollbackPath);
    const bool hadExisting = QFile::exists(dbPath);
    if (hadExisting)
        QFile::copy(dbPath, rollbackPath);

    bool ok = true;

    if (QFile::exists(dbPath) && !QFile::remove(dbPath)) {
        err = "Could not replace the current database file.";
        ok = false;
    }

    if (ok && !QFile::copy(srcPath, dbPath)) {
        err = "Failed to copy the imported database into place.";
        ok = false;
    }

    // Roll back on failure.
    if (!ok && hadExisting) {
        QFile::remove(dbPath);
        QFile::copy(rollbackPath, dbPath);
    }
    QFile::remove(rollbackPath);

    openConnection();

    if (!ok && errorMessage) *errorMessage = err;
    return ok;
}

// ── Schema ─────────────────────────────────────────────────────
void DatabaseManager::createTablesIfNeeded() {
    QSqlQuery q;
    q.exec("CREATE TABLE IF NOT EXISTS folders ("
           "RecID INTEGER PRIMARY KEY AUTOINCREMENT,"
           "SortBy INTEGER,"
           "Folder TEXT)");

    q.exec("CREATE TABLE IF NOT EXISTS snippets ("
           "RecID INTEGER PRIMARY KEY AUTOINCREMENT,"
           "FolderID INTEGER,"
           "SnippetTitle TEXT,"
           "SnippetDesc TEXT,"
           "SnippetCode TEXT,"
           "SnippetNote TEXT)");

    q.exec("CREATE TABLE IF NOT EXISTS settings ("
           "Key TEXT PRIMARY KEY,"
           "Value TEXT)");
}

QList<Folder> DatabaseManager::fetchFolders() {
    QList<Folder> folders;
    QSqlQuery q("SELECT RecID, SortBy, Folder FROM folders ORDER BY SortBy ASC");
    while (q.next()) {
        folders.append({ q.value(0).toInt(), q.value(1).toInt(), q.value(2).toString() });
    }
    return folders;
}

int DatabaseManager::insertFolder(const QString& name) {
    QSqlQuery q;
    q.prepare("INSERT INTO folders (SortBy, Folder) VALUES (0, ?)");
    q.addBindValue(name);
    q.exec();
    return q.lastInsertId().toInt();
}

void DatabaseManager::renameFolder(int id, const QString& newName) {
    QSqlQuery q;
    q.prepare("UPDATE folders SET Folder = ? WHERE RecID = ?");
    q.addBindValue(newName);
    q.addBindValue(id);
    q.exec();
}

void DatabaseManager::deleteFolder(int id) {
    QSqlQuery q;
    q.prepare("DELETE FROM snippets WHERE FolderID = ?");
    q.addBindValue(id);
    q.exec();
    q.prepare("DELETE FROM folders WHERE RecID = ?");
    q.addBindValue(id);
    q.exec();
}

void DatabaseManager::updateFolderSortOrder(int folderID, int sortOrder) {
    QSqlQuery q;
    q.prepare("UPDATE folders SET SortBy = ? WHERE RecID = ?");
    q.addBindValue(sortOrder);
    q.addBindValue(folderID);
    q.exec();
}

QList<Snippet> DatabaseManager::fetchSnippets(int folderID) {
    QList<Snippet> list;
    QSqlQuery q;
    q.prepare("SELECT RecID, FolderID, SnippetTitle, SnippetDesc, SnippetCode, SnippetNote "
              "FROM snippets WHERE FolderID = ? ORDER BY SnippetTitle ASC");
    q.addBindValue(folderID);
    q.exec();
    while (q.next()) {
        list.append({
            q.value(0).toInt(), q.value(1).toInt(),
            q.value(2).toString(), q.value(3).toString(),
            q.value(4).toString(), q.value(5).toString()
        });
    }
    return list;
}

int DatabaseManager::insertSnippet(int folderID) {
    QSqlQuery q;
    q.prepare("INSERT INTO snippets (FolderID, SnippetTitle, SnippetDesc, SnippetCode, SnippetNote) "
              "VALUES (?, '', '', '', '')");
    q.addBindValue(folderID);
    q.exec();
    return q.lastInsertId().toInt();
}

void DatabaseManager::updateSnippet(const Snippet& s) {
    QSqlQuery q;
    q.prepare("UPDATE snippets SET SnippetTitle=?, SnippetDesc=?, SnippetCode=?, SnippetNote=? "
              "WHERE RecID=?");
    q.addBindValue(s.title);
    q.addBindValue(s.description);
    q.addBindValue(s.code);
    q.addBindValue(s.note);
    q.addBindValue(s.id);
    q.exec();
}

void DatabaseManager::deleteSnippet(int id) {
    QSqlQuery q;
    q.prepare("DELETE FROM snippets WHERE RecID = ?");
    q.addBindValue(id);
    q.exec();
}

void DatabaseManager::saveSetting(const QString& key, const QString& value) {
    QSqlQuery q;
    q.prepare("REPLACE INTO settings (Key, Value) VALUES (?, ?)");
    q.addBindValue(key);
    q.addBindValue(value);
    q.exec();
}

QString DatabaseManager::getSetting(const QString& key, const QString& defaultValue) {
    QSqlQuery q;
    q.prepare("SELECT Value FROM settings WHERE Key = ? LIMIT 1");
    q.addBindValue(key);
    q.exec();
    if (q.next()) return q.value(0).toString();
    return defaultValue;
}
