// All seven controls in one form. Click the IDE canvas before typing.
// Tab moves between controls; arrows change choices; Space toggles a checkbox.
use window as ui
use strings

ui.create<"Start a school club", 560, 900>
ui.label<"Choose a project and join the club.">
var name = ui.input<"Your name">
var interests = ui.checklist<"Interests", list<"Programming", "Drawing", "Music">>
var level = ui.radio_group<"Experience", list<"Beginner", "Explorer">>
var project = ui.listbox<"Project", list<"Game", "Weather station", "Robot">>
var day = ui.combobox<"Meeting day", list<"Monday", "Tuesday", "Wednesday">>
var message = ui.label<"Ready when you are.">

func Join <>:
    var cleaned = strings.trim<name.value>
    if cleaned is "":
        message.caption = "Please enter your name first."
        ret
    .
    message.caption = "Welcome to the club, " + cleaned + "!"
    print<message.caption>
    print<"Interests:", strings.join<interests.selected, ", ">>
    print<"Experience:", level.value>
    print<"Project:", project.value>
    print<"Meeting:", day.value>
.

ui.button<"Join the club", Join>
ui.button<"Close", ui.close>
ui.run<>
