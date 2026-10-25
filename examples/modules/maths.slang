lib maths:
    var calls = 0

    func Validate <val>:
        assert<val >= 0, "Expected a non-negative number">
    .

    public func Square <val>:
        Validate<val>
        calls++
        ret val * val
    .

    public func Calls <>:
        ret calls
    .
.
