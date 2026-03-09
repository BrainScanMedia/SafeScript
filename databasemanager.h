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
};

class DatabaseManager {
public:
    static DatabaseManager& instance();
    bool open();

    // Folders
    QList<Folder> fetchFolders();
    int insertFolder(const QString& name);
    void renameFolder(int id, const QString& newName);
    void deleteFolder(int id);
    void updateFolderSortOrder(int folderID, int sortOrder);

    // Snippets
    QList<Snippet> fetchSnippets(int folderID);
    int insertSnippet(int folderID);
    void updateSnippet(const Snippet& snippet);
    void deleteSnippet(int id);

    // Settings
    void saveSetting(const QString& key, const QString& value);
    QString getSetting(const QString& key, const QString& defaultValue = "");

private:
    DatabaseManager() {}
    void createTablesIfNeeded();
    QSqlDatabase db;
};
