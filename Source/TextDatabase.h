#pragma once
#include <string>
#include <vector>
#include <map>

struct FileMetadata {
    std::string title;
    std::vector<std::string> lines;
};

class TextDatabase
{
public:
    static TextDatabase& Instance() { static TextDatabase i; return i; }

    void Initialize();
    const FileMetadata* GetMetadata(const std::string& fileName) const;
    const std::vector<std::string>& GetFiles(const std::string& folderName);
    std::string GetSystemString(const std::string& key);
    const std::vector<std::string>& GetSystemLogs() const { return m_systemLogs; }

private:
    TextDatabase() = default;
    ~TextDatabase() = default;

    // [UBAH] Hanya satu fungsi untuk SEMUA jenis entry (File, Folder, Back, dll)
    // Parameter:
    // folder       : Masuk ke folder mana? (misal "ROOT")
    // displayLabel : Teks lengkap di menu (misal "README.txt      200 KB")
    // title        : Judul di panel kanan
    // content      : Isi teks di panel kanan
    void RegisterEntry(const std::string& folder, const std::string& displayLabel, const std::string& title, const std::vector<std::string>& content);

    std::map<std::string, std::vector<std::string>> m_fileSystem;
    std::map<std::string, FileMetadata> m_fileDatabase;
    std::map<std::string, std::string> m_systemStrings;
    std::vector<std::string> m_systemLogs;
};