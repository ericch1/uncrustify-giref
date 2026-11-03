// Test: align_func_proto_paren in Java

public interface Shape
{
    double area();
    String fullName(boolean full);
    void moveBy(int dx, int dy);
    java.util.List<Integer> layers();
}

public abstract class Square implements Shape
{
    public Square(int side) { this.side = side; }
    public Square() { this(1); }
    public abstract double area();
    protected abstract void resize(int factor);
    private int side;
}
