#include "editor.hpp"
#include <iostream>
#include <fstream>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <cstdlib>
#include <filesystem>

namespace fs = std::filesystem;

// Terminal implementation
static struct termios orig_termios;

void Terminal::enterRawMode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void Terminal::exitRawMode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void Terminal::clearScreen() {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
}

void Terminal::moveCursor(int row, int col) {
    std::string buf = "\x1b[" + std::to_string(row + 1) + ";" + std::to_string(col + 1) + "H";
    write(STDOUT_FILENO, buf.c_str(), buf.size());
}

void Terminal::hideCursor() {
    write(STDOUT_FILENO, "\x1b[?25l", 6);
}

void Terminal::showCursor() {
    write(STDOUT_FILENO, "\x1b[?25h", 6);
}

std::pair<int, int> Terminal::getWindowSize() {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        return {24, 80};
    }
    return {ws.ws_row, ws.ws_col};
}

// SyntaxHighlighter implementation
void SyntaxHighlighter::addRule(const std::string& pattern, const std::string& color) {
    rules.push_back({std::regex(pattern), color});
}

std::string SyntaxHighlighter::highlight(const std::string& line) {
    std::string result = line;
    for (const auto& rule : rules) {
        std::smatch match;
        std::string temp = result;
        result.clear();
        std::string::const_iterator searchStart(temp.cbegin());
        
        while (std::regex_search(searchStart, temp.cend(), match, rule.pattern)) {
            result.append(searchStart, searchStart + match.position());
            result.append(rule.color);
            result.append(match.str());
            result.append("\x1b[0m");
            searchStart = searchStart + match.position() + match.length();
        }
        result.append(searchStart, temp.cend());
    }
    return result;
}

// Buffer implementation
Buffer::Buffer(const std::string& path) : filepath(path) {
    load();
}

void Buffer::insertChar(int row, int col, char c) {
    if (row >= lines.size()) return;
    lines[row].insert(col, 1, c);
    modified = true;
}

void Buffer::deleteChar(int row, int col) {
    if (row >= lines.size() || col == 0) return;
    lines[row].erase(col - 1, 1);
    modified = true;
}

void Buffer::insertLine(int row) {
    if (row > lines.size()) return;
    lines.insert(lines.begin() + row + 1, "");
    modified = true;
}

void Buffer::deleteLine(int row) {
    if (lines.size() <= 1) return;
    lines.erase(lines.begin() + row);
    modified = true;
}

std::string Buffer::getLine(int row) const {
    if (row >= lines.size()) return "";
    return lines[row];
}

void Buffer::save() {
    std::ofstream file(filepath);
    for (const auto& line : lines) {
        file << line << '\n';
    }
    modified = false;
}

void Buffer::load() {
    lines.clear();
    std::ifstream file(filepath);
    if (!file.is_open()) {
        lines.push_back("");
        return;
    }
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    if (lines.empty()) lines.push_back("");
}

// PluginManager implementation
void PluginManager::loadPlugin(std::shared_ptr<Plugin> plugin) {
    plugins[plugin->getName()] = plugin;
    plugin->onLoad();
}

void PluginManager::unloadPlugin(const std::string& name) {
    plugins.erase(name);
}

void PluginManager::notifyKeyPress(int key) {
    for (auto& [name, plugin] : plugins) {
        plugin->onKeyPress(key);
    }
}

void PluginManager::notifyBufferChange() {
    for (auto& [name, plugin] : plugins) {
        plugin->onBufferChange();
    }
}

// FileExplorer implementation
void FileExplorer::scanDirectory(const std::string& path) {
    files.clear();
    try {
        for (const auto& entry : fs::directory_iterator(path)) {
            files.push_back(entry.path().filename().string());
        }
    } catch (...) {}
}

void FileExplorer::render(int startRow, int height) {
    for (int i = 0; i < height && i < files.size(); i++) {
        Terminal::moveCursor(startRow + i, 0);
        std::string marker = (i == selectedIndex) ? "> " : "  ";
        std::cout << marker << files[i];
    }
}

void FileExplorer::moveSelection(int delta) {
    selectedIndex += delta;
    if (selectedIndex < 0) selectedIndex = 0;
    if (selectedIndex >= files.size()) selectedIndex = files.size() - 1;
}

std::string FileExplorer::getSelected() const {
    if (selectedIndex < files.size()) return files[selectedIndex];
    return "";
}

// Editor implementation
Editor::Editor() {
    Terminal::enterRawMode();
    buffers.push_back(std::make_shared<Buffer>());
    
    // Basic C++ syntax highlighting
    highlighter.addRule(R"(\b(int|void|return|if|else|for|while|class)\b)", "\x1b[34m");
    highlighter.addRule(R"(".*?")", "\x1b[32m");
    highlighter.addRule(R"(//.*)", "\x1b[90m");
}

Editor::~Editor() {
    Terminal::exitRawMode();
}

void Editor::run() {
    while (running) {
        render();
        processKeypress();
    }
}

void Editor::processKeypress() {
    char c;
    if (read(STDIN_FILENO, &c, 1) != 1) return;
    
    if (commandMode) {
        if (c == '\r') {
            executeCommand(commandBuffer);
            commandMode = false;
            commandBuffer.clear();
        } else if (c == 27) {
            commandMode = false;
            commandBuffer.clear();
        } else if (c == 127) {
            if (!commandBuffer.empty()) commandBuffer.pop_back();
        } else {
            commandBuffer += c;
        }
        return;
    }
    
    pluginManager.notifyKeyPress(c);
    
    if (c == ':') {
        commandMode = true;
    } else if (c == 27) {
        // ESC key
    } else if (c == 127) {
        deleteChar();
    } else if (c == '\r') {
        newLine();
    } else if (c >= 32 && c <= 126) {
        insertChar(c);
    }
}

void Editor::insertChar(char c) {
    getCurrentBuffer().insertChar(cursorRow, cursorCol, c);
    cursorCol++;
    pluginManager.notifyBufferChange();
}

void Editor::deleteChar() {
    if (cursorCol > 0) {
        getCurrentBuffer().deleteChar(cursorRow, cursorCol);
        cursorCol--;
        pluginManager.notifyBufferChange();
    }
}

void Editor::newLine() {
    std::string& line = getCurrentBuffer().getLine(cursorRow);
    std::string rest = line.substr(cursorCol);
    line = line.substr(0, cursorCol);
    getCurrentBuffer().insertLine(cursorRow);
    cursorRow++;
    cursorCol = 0;
    pluginManager.notifyBufferChange();
}

void Editor::executeCommand(const std::string& cmd) {
    if (cmd == "q") quit();
    else if (cmd == "w") saveFile();
    else if (cmd == "wq") { saveFile(); quit(); }
    else if (cmd.substr(0, 2) == "e ") openFile(cmd.substr(2));
    else if (cmd == "explorer") { showExplorer = !showExplorer; fileExplorer.scanDirectory("."); }
    else statusMessage = "Unknown command: " + cmd;
}

void Editor::openFile(const std::string& filepath) {
    buffers.push_back(std::make_shared<Buffer>(filepath));
    currentBuffer = buffers.size() - 1;
    cursorRow = cursorCol = 0;
}

void Editor::saveFile() {
    getCurrentBuffer().save();
    statusMessage = "File saved";
}

void Editor::quit() {
    if (getCurrentBuffer().isModified()) {
        statusMessage = "Unsaved changes! Use :q! to force quit";
    } else {
        running = false;
    }
}

void Editor::render() {
    Terminal::hideCursor();
    Terminal::clearScreen();
    
    auto [rows, cols] = Terminal::getWindowSize();
    
    // Render buffer
    for (int i = 0; i < rows - 2; i++) {
        int fileRow = i + rowOffset;
        if (fileRow < getCurrentBuffer().getLineCount()) {
            std::string line = getCurrentBuffer().getLine(fileRow);
            line = highlighter.highlight(line);
            std::cout << line.substr(colOffset, cols) << "\r\n";
        } else {
            std::cout << "~\r\n";
        }
    }
    
    renderStatusBar();
    renderCommandLine();
    
    Terminal::moveCursor(cursorRow - rowOffset, cursorCol - colOffset);
    Terminal::showCursor();
}

void Editor::renderStatusBar() {
    auto [rows, cols] = Terminal::getWindowSize();
    Terminal::moveCursor(rows - 2, 0);
    std::cout << "\x1b[7m";
    std::string status = getCurrentBuffer().getFilepath();
    if (getCurrentBuffer().isModified()) status += " [+]";
    status += " | " + std::to_string(cursorRow + 1) + ":" + std::to_string(cursorCol + 1);
    std::cout << status;
    for (int i = status.size(); i < cols; i++) std::cout << " ";
    std::cout << "\x1b[0m";
}

void Editor::renderCommandLine() {
    auto [rows, cols] = Terminal::getWindowSize();
    Terminal::moveCursor(rows - 1, 0);
    if (commandMode) {
        std::cout << ":" << commandBuffer;
    } else if (!statusMessage.empty()) {
        std::cout << statusMessage;
        statusMessage.clear();
    }
}
