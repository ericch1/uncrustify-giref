// Test: align_func_proto_paren in C#

public abstract class Shape
{
    public Shape() {}
    public Shape(int sides) { this.sides = sides; }

    public abstract double Area();
    public abstract string FullName(bool full);
    protected abstract void MoveBy(int dx, int dy);

    public int Sides() { return sides; }
    public static Shape Create(string name) { return null; }

    private int sides;
}

public interface IDrawable
{
    void Draw();
    bool IsVisible(int layer);
    System.Collections.Generic.List<int> Layers();
}
