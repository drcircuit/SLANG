// Lesson: object types, mutable fields, lists, and typed functions.
// Try: add another pet to the list.
obj Pet:
    name as text
    legs as int
    hungry as bool
.

func Feed <pet as Pet>:
    if pet.hungry:
        print<"Feeding", pet.name>
        pet.hungry = false
    else:
        print<pet.name, "is already full.">
    .
.

var pets = Pet<"Rex", 4, true>, Pet<"Kiwi", 2, true>
for pet in pets:
    Feed<pet>
.
Feed<pets.1>
assert<pets.1.hungry is false>
print<"First pet:", pets.1.name, "Legs:", pets.1.legs>
