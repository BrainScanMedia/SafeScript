#include "databasemanager.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QDebug>
#include <QDateTime>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>

// Current local time as an ISO-8601 string, used for Created/Modified stamps.
static QString nowIso() {
    return QDateTime::currentDateTime().toString(Qt::ISODate);
}

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

    // Fresh databases get the full column set here; existing ones are brought
    // up to date by runMigrations() below.
    q.exec("CREATE TABLE IF NOT EXISTS snippets ("
           "RecID INTEGER PRIMARY KEY AUTOINCREMENT,"
           "FolderID INTEGER,"
           "SnippetTitle TEXT,"
           "SnippetDesc TEXT,"
           "SnippetCode TEXT,"
           "SnippetNote TEXT,"
           "Favorite INTEGER DEFAULT 0,"
           "Created TEXT,"
           "Modified TEXT)");

    q.exec("CREATE TABLE IF NOT EXISTS settings ("
           "Key TEXT PRIMARY KEY,"
           "Value TEXT)");

    runMigrations();
}

// Returns true if the given table already has the given column.
bool DatabaseManager::tableHasColumn(const QString& table, const QString& column) {
    QSqlQuery q;
    q.exec(QString("PRAGMA table_info(%1)").arg(table));
    while (q.next()) {
        if (q.value(1).toString().compare(column, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

// Idempotent, additive-only migrations. Safe to run on every startup: each step
// checks whether it is already applied. New columns are added with ALTER TABLE
// so databases created by older versions upgrade in place without data loss.
void DatabaseManager::runMigrations() {
    QSqlQuery q;
    if (!tableHasColumn("snippets", "Favorite"))
        q.exec("ALTER TABLE snippets ADD COLUMN Favorite INTEGER DEFAULT 0");
    if (!tableHasColumn("snippets", "Created"))
        q.exec("ALTER TABLE snippets ADD COLUMN Created TEXT");
    if (!tableHasColumn("snippets", "Modified"))
        q.exec("ALTER TABLE snippets ADD COLUMN Modified TEXT");

    // Record the schema version for future reference.
    saveSetting("SchemaVersion", "2");
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
    // Favorites float to the top; ties break alphabetically, case-insensitively.
    q.prepare("SELECT RecID, FolderID, SnippetTitle, SnippetDesc, SnippetCode, SnippetNote, "
              "COALESCE(Favorite, 0), Created, Modified "
              "FROM snippets WHERE FolderID = ? "
              "ORDER BY COALESCE(Favorite, 0) DESC, SnippetTitle COLLATE NOCASE ASC");
    q.addBindValue(folderID);
    q.exec();
    while (q.next()) {
        Snippet s;
        s.id          = q.value(0).toInt();
        s.folderID    = q.value(1).toInt();
        s.title       = q.value(2).toString();
        s.description = q.value(3).toString();
        s.code        = q.value(4).toString();
        s.note        = q.value(5).toString();
        s.favorite    = q.value(6).toInt() != 0;
        s.created     = q.value(7).toString();
        s.modified    = q.value(8).toString();
        list.append(s);
    }
    return list;
}

int DatabaseManager::insertSnippet(int folderID) {
    const QString now = nowIso();
    QSqlQuery q;
    q.prepare("INSERT INTO snippets "
              "(FolderID, SnippetTitle, SnippetDesc, SnippetCode, SnippetNote, Favorite, Created, Modified) "
              "VALUES (?, '', '', '', '', 0, ?, ?)");
    q.addBindValue(folderID);
    q.addBindValue(now);
    q.addBindValue(now);
    q.exec();
    return q.lastInsertId().toInt();
}

int DatabaseManager::insertSnippet(int folderID, const QString& title, const QString& code) {
    const QString now = nowIso();
    QSqlQuery q;
    q.prepare("INSERT INTO snippets "
              "(FolderID, SnippetTitle, SnippetDesc, SnippetCode, SnippetNote, Favorite, Created, Modified) "
              "VALUES (?, ?, '', ?, '', 0, ?, ?)");
    q.addBindValue(folderID);
    q.addBindValue(title);
    q.addBindValue(code);
    q.addBindValue(now);
    q.addBindValue(now);
    q.exec();
    return q.lastInsertId().toInt();
}

void DatabaseManager::updateSnippet(const Snippet& s) {
    QSqlQuery q;
    q.prepare("UPDATE snippets SET SnippetTitle=?, SnippetDesc=?, SnippetCode=?, SnippetNote=?, "
              "Modified=? WHERE RecID=?");
    q.addBindValue(s.title);
    q.addBindValue(s.description);
    q.addBindValue(s.code);
    q.addBindValue(s.note);
    q.addBindValue(nowIso());
    q.addBindValue(s.id);
    q.exec();
}

void DatabaseManager::deleteSnippet(int id) {
    QSqlQuery q;
    q.prepare("DELETE FROM snippets WHERE RecID = ?");
    q.addBindValue(id);
    q.exec();
}

// Duplicates a snippet inside its own folder. The copy is never a favorite and
// gets fresh timestamps; its title has " (copy)" appended.
int DatabaseManager::cloneSnippet(int id) {
    QSqlQuery q;
    q.prepare("SELECT FolderID, SnippetTitle, SnippetDesc, SnippetCode, SnippetNote "
              "FROM snippets WHERE RecID = ?");
    q.addBindValue(id);
    q.exec();
    if (!q.next()) return -1;

    int folderID       = q.value(0).toInt();
    QString title      = q.value(1).toString();
    QString desc       = q.value(2).toString();
    QString code       = q.value(3).toString();
    QString note       = q.value(4).toString();
    const QString now  = nowIso();

    QSqlQuery ins;
    ins.prepare("INSERT INTO snippets "
                "(FolderID, SnippetTitle, SnippetDesc, SnippetCode, SnippetNote, Favorite, Created, Modified) "
                "VALUES (?, ?, ?, ?, ?, 0, ?, ?)");
    ins.addBindValue(folderID);
    ins.addBindValue(title.isEmpty() ? QString("Untitled (copy)") : title + " (copy)");
    ins.addBindValue(desc);
    ins.addBindValue(code);
    ins.addBindValue(note);
    ins.addBindValue(now);
    ins.addBindValue(now);
    ins.exec();
    return ins.lastInsertId().toInt();
}

void DatabaseManager::moveSnippet(int id, int newFolderID) {
    QSqlQuery q;
    q.prepare("UPDATE snippets SET FolderID = ? WHERE RecID = ?");
    q.addBindValue(newFolderID);
    q.addBindValue(id);
    q.exec();
}

void DatabaseManager::setSnippetFavorite(int id, bool favorite) {
    QSqlQuery q;
    q.prepare("UPDATE snippets SET Favorite = ? WHERE RecID = ?");
    q.addBindValue(favorite ? 1 : 0);
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
