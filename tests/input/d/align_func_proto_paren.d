// Test: align_func_proto_paren in D

interface Shape
{
    double area();
    string name(bool full);
    void moveBy(int dx, int dy);
    T convert(T)(T value);
    int sides();
}

class Square : Shape
{
    this(int side);
    ~this();
    double area();
    string fullName(bool full);
}
