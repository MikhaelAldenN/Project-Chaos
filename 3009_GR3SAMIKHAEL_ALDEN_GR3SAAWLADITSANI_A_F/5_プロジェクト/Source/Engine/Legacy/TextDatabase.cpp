#include "TextDatabase.h"

// [IMPLEMENTASI BARU YANG SIMPEL]
void TextDatabase::RegisterEntry(const std::string& folder, const std::string& displayLabel, const std::string& title, const std::vector<std::string>& content)
{
    // 1. Masukkan string mentah-mentah ke list Menu UI
    //    Kamu bebas atur spasi di sini.
    m_fileSystem[folder].push_back(displayLabel);

    // 2. Ambil kata pertama sebagai Key untuk Database
    //    Contoh: "README.txt      200" -> Key: "README.txt"
    //    Sistem akan otomatis memotong di spasi pertama.
    std::string cleanKey = displayLabel;
    size_t spacePos = displayLabel.find(' ');

    if (spacePos != std::string::npos) {
        cleanKey = displayLabel.substr(0, spacePos);
    }

    // 3. Simpan Metadata
    m_fileDatabase[cleanKey] = { title, content };
}

void TextDatabase::Initialize()
{
    m_fileSystem.clear();
    m_fileDatabase.clear();
    m_systemStrings.clear();
    // 1. SYSTEM STRINGS
    m_systemStrings["StatusOnline"] = "Online - Unstable";
    m_systemStrings["DirectoryHeader"] = "NAME             SIZE";
    m_systemStrings["TUIMenuBar"] = "F1 HELP | F2 NOTE | F3 EDIT | F4 DEL | F5 RUN | F9 REFR | F10 QUIT";

    // =========================================================
    // ISI FOLDER: ROOT
    // =========================================================

    // Format: RegisterEntry("FOLDER", "TEKS TAMPILAN BEBAS", "JUDUL KANAN", {ISI});



    // File: BeyondBreaker...
    RegisterEntry("ROOT", "BEYONDBREAKER.exe 3256", "APPLICATION INFO", {
        "App : BeyondBreaker.exe",
        "Ver : Alpha 1.0",
        "Dir : C:\\GAMES\\BEYOND_BREAKER\\BIN",
        "",
        "[ DESCRIPTION ]",
        " This program is a dimension-breaking tool",
        " disguised as a game application.",
        " It requires direct user intervention to",
        " bypass the operating system's security.",
        "",
        "",
        "",
        "",
        "",
        "      +--------------------------+",
        "      |  > PRESS ENTER TO RUN <  |",
        "      +--------------------------+",
        "",
        "",
        "",
        "",
        "",
        "     0x008F: A9 4C 00 FF 8B EC 5D C3",  
        "     0x0090: 90 90 90 ERROR... NULL"    
        ""
        });

    // File: README.TXT
    RegisterEntry("ROOT", "ReadMe.txt         275", "FILE CONTENT: README.TXT", {
        "    ____  _______   _____  _   _ ____       ",
        "   | __ )| ____\\ \\ / / _ \\| \\ | |  _ \\      ",
        "   |  _ \\|  _|  \\ V / | | |  \\| | | | |     ",
        "   | |_) | |___  | || |_| | |\\  | |_| |     ",
        " __|____/|_____|_|_| \\___/|_| \\_|____/____  ",
        "| __ )|  _ \\| ____|  / \\  | |/ / ____|  _ \\ ",
        "|  _ \\| |_) |  _|   / _ \\ | ' /|  _| | |_) |",
        "| |_) |  _ <| |___ / ___ \\| . \\| |___|  _ < ",
        "|____/|_| \\_\\_____/_/   \\_\\_|\\_\\_____|_| \\_\\",
        "",
        "              --a Game by--",
        "      KONPAIRUTOOREBAKAMI PRODUCTION",
        "",
        "===========================================",
        " RELEASE INFO  : PUBLIC ALPHA BUILD v1.0",
        " TARGET SYSTEM : BRICK-DOS / KERNEL 32",
        " CRACKED BY    : KONPAIRU-TO-OREBAKAMI",
        "===========================================",
        " ",
        "[ DESCRIPTION ]",
        " warning: this program is unstable!",
        " Beyond Breaker is not just a game. It is a ",
        " tooldesigned to shatter the 4th wall of ",
        " this OS."
        });

    // File: Settings.ini
    RegisterEntry("ROOT", "Settings.ini        83", "CONFIGURATION", {
        "[Video]",
        "Resolution=1920x1080",
        "Fullscreen=True",
        "VSync=On",
        "",
        "[Audio]",
        "MasterVolume=80",
        "BGM=100"
        });

    // File: Exit.exe
    RegisterEntry("ROOT", "Exit.exe          4096", "SYSTEM CALL", {
        "Terminating Session...",
        "Are you sure you want to quit?",
        " ",
        "> Clicking this again will close the app."
        });
}

// [GETTER TETAP SAMA]
const std::vector<std::string>& TextDatabase::GetFiles(const std::string& folderName)
{
    if (m_fileSystem.find(folderName) != m_fileSystem.end()) {
        return m_fileSystem[folderName];
    }
    return m_fileSystem["ROOT"];
}

const FileMetadata* TextDatabase::GetMetadata(const std::string& fileName) const
{
    auto it = m_fileDatabase.find(fileName);
    if (it != m_fileDatabase.end())
    {
        return &it->second;
    }
    return nullptr;
}

std::string TextDatabase::GetSystemString(const std::string& key)
{
    if (m_systemStrings.count(key)) return m_systemStrings[key];
    return "[[MISSING TEXT]]";
}