use files
use time

// This example creates/overwrites student-notes.txt beside this source file.
var start = time.seconds<>
files.write<"student-notes.txt", "My programming notebook\n">
files.append<"student-notes.txt", "I can save text with æ, ø, and å!\n">
assert<files.exists<"student-notes.txt">>
print<files.read<"student-notes.txt">>
print<"Seconds used:", time.seconds<> - start>
