#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>

// Kita pindahkan struct ini dari SceneTitle ke sini agar jadi Global
struct FileMetadata {
    std::string title;            // Judul Panel Kanan
    std::vector<std::string> lines; // Isi Teks (ASCII Art / Deskripsi)
};

class TextDatabase
{
public:
    // Singleton Pattern (Sama seperti Graphics/Input)
    static TextDatabase& Instance() { static TextDatabase i; return i; }

    // Setup awal (Load semua teks ke memori)
    void Initialize();

    // Ambil data berdasarkan nama file (Key)
    const FileMetadata* GetMetadata(const std::string& fileName) const;

    // Ambil daftar nama file untuk Menu Directory
    //const std::vector<std::string>& GetDirectoryList() const { return m_directoryList; }
    const std::vector<std::string>& GetFiles(const std::string& folderName);

    // Ambil string sistem (misal: Header atau Status)
    std::string GetSystemString(const std::string& key);

    const std::vector<std::string>& GetSystemLogs() const { return m_systemLogs; }

private:
    TextDatabase() = default;
    ~TextDatabase() = default;

    // Penyimpanan Data
    //std::vector<std::string> m_directoryList;
    std::map<std::string, std::vector<std::string>> m_fileSystem;

    std::map<std::string, FileMetadata> m_fileDatabase;
    std::map<std::string, std::string> m_systemStrings;
    std::vector<std::string> m_systemLogs;
};