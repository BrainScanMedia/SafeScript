#pragma once
#include <QString>
#include <QList>
#include <QtSql/QSqlDatabase>

struct Folder {
    int id;
    int sortBy;
    QString name;
};

struct Snippet {
    int id;
    int folderID;
    QString title;
    QString description;
    QString code;
    QString note;
    bool    favorite = false;
    QString created;    // ISO 8601 local time; empty for legacy rows
    QString modified;   // ISO 8601 local time; empty for legacy rows
};

class DatabaseManager {
public:
    static DatabaseManager& instance();
    bool open();

    // Path to the live database file currently in use
    QString databasePath() const;

    // Copy the current database to destPath (backup).
    // Returns false and fills errorMessage on failure.
    bool backupTo(const QString& destPath, QString* errorMessage = nullptr);

    // Replace the current database with the file at srcPath (import).
    // Validates the source first and rolls back on failure.
    // Returns false and fills errorMessage on failure.
    bool importFrom(const QString& srcPath, QString* errorMessage = nullptr);

    // Folders
    QList<Folder> fetchFolders();
    int insertFolder(const QString& name);
    void renameFolder(int id, const QString& newName);
    void deleteFolder(int id);
    void updateFolderSortOrder(int folderID, int sortOrder);

    // Snippets
    QList<Snippet> fetchSnippets(int folderID);
    int insertSnippet(int folderID);
    int insertSnippet(int folderID, const QString& title, const QString& code);
    void updateSnippet(const Snippet& snippet);
    void deleteSnippet(int id);
    int  cloneSnippet(int id);                       // duplicates within the same folder; returns new id
    void moveSnippet(int id, int newFolderID);
    void setSnippetFavorite(int id, bool favorite);

    // Settings
    void saveSetting(const QString& key, const QString& value);
    QString getSetting(const QString& key, const QString& defaultValue = "");

private:
    DatabaseManager() {}
    void createTablesIfNeeded();
    void runMigrations();               // adds columns to pre-existing databases as the schema grows
    bool tableHasColumn(const QString& table, const QString& column);

    bool openConnection();
    void closeConnection();
    bool isValidDatabase(const QString& path, QString* errorMessage = nullptr);

    QSqlDatabase db;
    QString dbPath;
};
