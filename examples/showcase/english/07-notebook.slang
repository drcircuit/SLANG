// Lesson: save UTF-8 text and find it again on the next run.
// This creates/appends to my-notebook.txt beside this source file.
// Try: run twice, then open my-notebook.txt in the file browser.
use files

var path = "my-notebook.txt"
if files.exists<path> is false:
    files.write<path, "My programming notebook\n">
.
files.append<path, "Today I learned to save text with æ, ø, and å.\n">
print<files.read<path>>
