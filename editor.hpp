#pragma once
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <map>
#include <regex>

using namespace std;

class Terminal {
  public:
    static void enterRawMode();
    static void exitRawMode();
    static void clearScreen();
    static void moveCursor(int row, int col);
    static void hideCursor();
    static void showCursor();
    static pair<int, int> getWindowSize();
};

struct HighlightRule {
  regex pattern;
  string color;
};

class SyntaxHighlighter {
  vector<HighlightRule> rules;
public:
  void addRule(const string& pattern, const string& color);
  string highlight(const string& line);
};

class Buffer {
  vector<string> lines;
  string filepath;
  bool modified = false;
public:
  Buffer() { lines.push_back(""); }
  explicit Buffer(const string& path);

  void insertChar(int row, int col, char c);
  void deleteChar(int row, int col);
  void insertLine(int row);
  void deleteLine(int row);

  string getLine(int row) const;
  int getLineCount() const { return lines.size(); }
  bool isModified() const { return modified; }
  void save();
  void load();

  const string& getFilePath() const { return filepath; }
  void setFilepath(const strings& path) { filepath = path; }
};

class Plugin {
public:
  virtual ~Plugin() = default;
  virtual void onLoad() {}
  virtual void onKeyPress(int key) {}
  virtual void onBufferChange() {}
  virtual string getName() const = 0;
};

class PluginManager{
  map<string, shared_ptr<Plugin>> plugins;
public:
  void loadPlugin(shared_ptr<Plugin> plugin);
  void loadPlugin(const string& name);
  void notifyKeyPress(int key);
  void notifyBufferChange();
};

class FileExplorer {
  vector<string> files;
  int selectedIndex = 0;
public:
  void scanDirectory(const string& path);
  void render(int startRow, int height);
  void moveSelection(int delta);
  string getSelected() const;
};

class Editor {
  vector<shared_ptr<Buffer>> buffers;
  int currentBuffer = 0;
  int cursorRow = 0, cursorCol = 0;
  int rowOffset = 0, colOffset = 0;
  string statusMessage;
  string commandBuffer;
  bool commandMode = false;
  bool running = true;

  SyntaxHighlighter highlighter;
  PluginManager pluginManager;
  FileExplorer fileExplorer;
  bool showExplorer = false;

public:
  Editor();
  ~Editor();

  void run();
  void processKeyPress();
  void moveCursor();
  void insertChar();
  void deleteChar();
  void newLine();
  void executeCommand(const string& cmd);
  void render();
  void renderStatusBar();
  void renderCommandLine();

  void openFile(const string& filepath);
  void saveFile();
  void quit();

  Buffer& getCurrentBuffer() { return *buffers[currentBuffer]; }
};


