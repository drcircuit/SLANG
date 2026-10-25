// A label, input and button. Try changing the greeting in the callback.
use window as ui
use strings

ui.create<"My first window", 480, 380>
ui.label<"Type your name, then click Greet.">
var name = ui.input<"Your name", "Åse">
var message = ui.label<"Ready!">

func Greet <>:
    var cleaned = strings.trim<name.value>
    if cleaned is "":
        message.caption = "Please enter your name."
    else:
        message.caption = "Hello, " + cleaned + "!"
    .
.

ui.button<"Greet", Greet>
ui.button<"Close", ui.close>
ui.run<>
