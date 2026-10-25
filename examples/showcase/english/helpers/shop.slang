// Import this library from 06-library.slang; it is not a standalone lesson.
lib shop:
    func Validate <price>:
        assert<price >= 0, "A price cannot be negative">
    .

    public obj Item:
        name as text
        price as dec
    .

    public func Total <items>:
        var total = 0.00
        for item in items:
            Validate<item.price>
            total = total + item.price
        .
        ret total
    .
.
