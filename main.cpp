int main(int argc, char* argv[]) {
  Editor editor;
  if (argc > 1) {
    editor.openFile(argv[1]);
  }
  editor.run();
  return 0;
}
