#include "TextDatabase.h"

void TextDatabase::Initialize()
{
    // 1. SETUP SYSTEM STRINGS
    m_systemStrings["StatusOnline"] = "Online - Unstable";
    m_systemStrings["DirectoryHeader"] = "NAME             SIZE";
    m_systemStrings["TUIMenuBar"] = "F1 HELP | F2 NOTE | F3 EDIT | F4 DEL | F5 RUN | F9 REFR | F10 QUIT";

    // 2. SETUP DIRECTORY LIST (Urutan Menu)
    m_directoryList = {
        "BeyondBreak...    3256",
        "README.txt         275",
        "settings.ini        83",
        "Misc/            <DIR>",
        "Exit.exe          4096"
    };

    // 3. SETUP FILE METADATA (Isi Konten)

    // --- Beyond Breaker ---
    m_fileDatabase["BeyondBreak...    3256"] = {
        "APPLICATION INFO", // Title
        {
            "App : BeyondBreaker.exe",
            "Ver : Alpha 1.0",
            "Dir : C:\\GAMES\\BEYOND_BREAKER\\BIN",
            "",
            "Desc:",
            "Project Game Programming tahun ke-2.",
            "Menghancurkan batas dimensi OS.",
            "Tekan ENTER untuk memulai."
        }
    };

    // --- README.TXT (ASCII Art) ---
    m_fileDatabase["README.txt         275"] = {
        "FILE CONTENT: README.TXT",
        {
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
        }
    };

    // --- Settings.ini ---
    m_fileDatabase["settings.ini        83"] = {
        "CONFIGURATION",
        {
            "[Video]",
            "Resolution=1920x1080",
            "Fullscreen=True",
            "VSync=On",
            "",
            "[Audio]",
            "MasterVolume=80",
            "BGM=100"
        }
    };

    // --- Misc Directory ---
    m_fileDatabase["Misc/            <DIR>"] = {
        "DIRECTORY INFO",
        {
            "Folder: Misc",
            "Access: RESTRICTED",
            "",
            "Contains system garbage and temp files.",
            "Do not delete."
        }
    };

    // --- Exit.exe ---
    m_fileDatabase["Exit.exe          4096"] = {
        "SYSTEM CALL",
        {
            "Terminating Session...",
            "Are you sure you want to quit?",
            " ",
            "> Clicking this again will close the app."
        }
    };

    m_systemLogs = {
        "Initializing Kernel...",
        "Mounting Volume (C:)",
        "Loading Drivers...",
        "Check Integrity: 100%",
        "Memory Heap: Alloc OK",
        "Security Level: NULL",
        "Bypass Firewall: ON",
        "Establishing Link...",
        "Handshake: Accepted",
        "Render Pipeline: ON",
        "Shader Cache: Built",
        "Reading Sector 0x7G",
        "Writing Buffer...",
        "Decrypting Keyfile",
        "Password: *********",
        "Root Access: GRANTED",
        "Fetching Remote Data",
        "Parsing Config...",
        "Texture Atlas: Load",
        "Audio Engine: Ready",
        "Physics: Stabilized",
        "Warning: Low Voltage",
        "Overclocking: 110%",
        "Compiling Assets...",
        "Garbage Collection",
        "Stack Trace: Clean",
        "User: ADMIN_01",
        "Uploading Virus...",
        "System Update: SKIP"
    }; }

const FileMetadata* TextDatabase::GetMetadata(const std::string& fileName) const
{
    // Cek apakah key ada di map
    auto it = m_fileDatabase.find(fileName);
    if (it != m_fileDatabase.end())
    {
        return &it->second; // Return pointer ke data
    }
    return nullptr; // Tidak ketemu
}

std::string TextDatabase::GetSystemString(const std::string& key)
{
    if (m_systemStrings.count(key)) return m_systemStrings[key];
    return "[[MISSING TEXT]]";
}