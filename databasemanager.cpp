#include "databasemanager.h"
#include <QStandardPaths>
#include <QDir>
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
    QString dbPath = appDataPath + "/storage.sqlite3";

    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(dbPath);

    if (!db.open()) {
        qWarning() << "Could not open database:" << db.lastError().text();
        return false;
    }

    createTablesIfNeeded();
    return true;
}

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
