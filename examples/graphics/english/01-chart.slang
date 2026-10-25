// CC0. Lesson: draw a bar chart from a list. No canvas setup is needed.
// Try: change the votes or colours. Close the window when finished.
use graphics as g

var names = "Cats", "Dogs", "Birds", "Fish", "Rabbits"
var votes = 6, 9, 3, 5, 7
var colours = "#FF70A6", "#70D6FF", "#FFD670", "#A8E6A1", "#BE95FF"

g.background<"#18243A">
g.fill<"#AABBD0">
g.line<50, 50, 50, 400>
g.line<50, 400, 600, 400>
for i from 1 to length<votes>:
    var height = votes.<i> * 32
    var x = 75 + (i - 1) * 105
    g.fill<colours.<i>>
    g.rect<x, 400 - height, 65, height>
    print<names.<i>, votes.<i>>
.
print<"Our favourite pets — bars follow the console order.">
